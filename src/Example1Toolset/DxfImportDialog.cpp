#include <DxfImportDialog.h>

#include <Example1Form.h>
#include <MultiSelectComboBox.h>

#include <DxfData.h>
#include "DxfInputFile.h"
#include <libdxfrw.h>

#include <cmdGCommandDeliveryRole.h>
#include <nexException.h>
#include <omuArguments.h>
#include <omuMethodCall.h>

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QValidator>
#include <QVBoxLayout>

#include <exception>
#include <set>

Example1DXFImportDialog::Example1DXFImportDialog(Example1Form* form)
    : SAMDataDialog(form, tr("DXF Import"), OK | CANCEL)
{
    setMinimumSize(470, 270);

    QGroupBox* dxfGroup = new QGroupBox(tr("DXF File"), this);
    m_dxfPathEdit = new QLineEdit(dxfGroup);
    m_dxfPathEdit->setPlaceholderText(tr("Enter or paste DXF file path"));
    connect(m_dxfPathEdit, SIGNAL(editingFinished()),
        this, SLOT(onDxfPathEditingFinished()));
    m_browseButton = new QPushButton(tr("Browse..."), dxfGroup);
    connect(m_browseButton, SIGNAL(clicked()), this, SLOT(onBrowse()));

    QHBoxLayout* pathLayout = new QHBoxLayout;
    pathLayout->addWidget(m_dxfPathEdit);
    pathLayout->addWidget(m_browseButton);
    QVBoxLayout* groupLayout = new QVBoxLayout(dxfGroup);
    groupLayout->addLayout(pathLayout);
    QVBoxLayout* paramLayout = new QVBoxLayout;
    paramLayout->addWidget(dxfGroup);

    QGroupBox* baseGroup = new QGroupBox(tr("Base Point (DXF)"), this);
    m_doubleValidator = new QDoubleValidator(baseGroup);
    m_doubleValidator->setNotation(QDoubleValidator::StandardNotation);

    m_baseXEdit = new QLineEdit(baseGroup);
    m_baseXEdit->setText("0.0");
    m_baseXEdit->setValidator(m_doubleValidator);
    m_baseXEdit->setPlaceholderText("0.0");
    m_baseYEdit = new QLineEdit(baseGroup);
    m_baseYEdit->setText("0.0");
    m_baseYEdit->setValidator(m_doubleValidator);
    m_baseYEdit->setPlaceholderText("0.0");
    m_baseZEdit = new QLineEdit(baseGroup);
    m_baseZEdit->setText("0.0");
    m_baseZEdit->setValidator(m_doubleValidator);
    m_baseZEdit->setPlaceholderText("0.0");

    QFormLayout* baseLayout = new QFormLayout(baseGroup);
    baseLayout->addRow(tr("X:"), m_baseXEdit);
    baseLayout->addRow(tr("Y:"), m_baseYEdit);
    baseLayout->addRow(tr("Z:"), m_baseZEdit);
    paramLayout->addWidget(baseGroup);

    QGroupBox* drawingSizeGroup = new QGroupBox(tr("Drawing Size"), this);
    m_drawingSizeCombo = new QComboBox(drawingSizeGroup);
    m_drawingSizeCombo->addItem(
        tr("Small drawing (100,000 entities)"),
        DxfImportDefaults::kDefaultMaxOutputEntities);
    m_drawingSizeCombo->addItem(
        tr("Large drawing (500,000 entities)"),
        DxfImportDefaults::kLargeDrawingEntityLimit);
    m_drawingSizeCombo->addItem(
        tr("Unlimited (no final output limit)"),
        DxfImportDefaults::kUnlimitedOutputEntities);
    m_drawingSizeCombo->setToolTip(
        tr("Unlimited removes the final output limit but keeps INSERT safety limits."));
    connect(m_drawingSizeCombo, SIGNAL(currentIndexChanged(int)),
        this, SLOT(onDrawingSizeChanged(int)));
    QFormLayout* drawingSizeLayout = new QFormLayout(drawingSizeGroup);
    drawingSizeLayout->addRow(tr("Profile:"), m_drawingSizeCombo);
    paramLayout->addWidget(drawingSizeGroup);

    QGroupBox* discretizationGroup =
        new QGroupBox(tr("Curve Discretization"), this);
    m_toleranceValidator = new QDoubleValidator(
        DxfImportDefaults::kMinimumCurveTolerance,
        DxfImportDefaults::kMaximumCurveTolerance,
        12,
        discretizationGroup);
    m_toleranceValidator->setNotation(QDoubleValidator::StandardNotation);
    m_toleranceEdit = new QLineEdit(discretizationGroup);
    m_toleranceEdit->setText(QString::number(
        DxfImportDefaults::kCurveTolerance, 'g', 12));
    m_toleranceEdit->setValidator(m_toleranceValidator);
    m_toleranceEdit->setPlaceholderText(QString::number(
        DxfImportDefaults::kCurveTolerance, 'g', 12));
    m_toleranceEdit->setToolTip(
        tr("Maximum deviation between a curve and its line segments."));
    QFormLayout* discretizationLayout = new QFormLayout(discretizationGroup);
    discretizationLayout->addRow(tr("Tolerance (mm):"), m_toleranceEdit);
    paramLayout->addWidget(discretizationGroup);

    QGroupBox* layerGroup = new QGroupBox(tr("Ignore Layers"), this);
    m_ignoreLayersCombo = new MultiSelectComboBox(layerGroup);
    m_ignoreLayersCombo->setToolTip(
        tr("Select one or more layers. Entities on selected layers will be excluded from import."));
    QVBoxLayout* layerLayout = new QVBoxLayout(layerGroup);
    layerLayout->addWidget(m_ignoreLayersCombo);
    paramLayout->addWidget(layerGroup);

    QGroupBox* importModeGroup = new QGroupBox(tr("Import Mode"), this);
    m_importModeCombo = new QComboBox(importModeGroup);
    m_importModeCombo->addItem(tr("Sketch"), QStringLiteral("Sketch"));
    m_importModeCombo->addItem(
        tr("Finite Element"), QStringLiteral("FiniteElement"));
    m_importModeCombo->setToolTip(
        tr("Choose whether the DXF is imported as a sketch or a finite-element part."));
    connect(m_importModeCombo, SIGNAL(currentIndexChanged(int)),
        this, SLOT(onImportModeChanged(int)));
    m_modelNameEdit = new QLineEdit(importModeGroup);
    m_modelNameEdit->setText(QStringLiteral("Model-1"));
    m_modelNameEdit->setPlaceholderText(tr("Existing model name"));
    m_partNameEdit = new QLineEdit(importModeGroup);
    m_partNameEdit->setText(QStringLiteral("DXF_ImportedPart"));
    m_partNameEdit->setPlaceholderText(tr("New part name"));
    QFormLayout* importModeLayout = new QFormLayout(importModeGroup);
    importModeLayout->addRow(tr("Mode:"), m_importModeCombo);
    importModeLayout->addRow(tr("Model name:"), m_modelNameEdit);
    importModeLayout->addRow(tr("Part name:"), m_partNameEdit);
    paramLayout->addWidget(importModeGroup);
    onImportModeChanged(m_importModeCombo->currentIndex());

    QHBoxLayout* contentLayout = new QHBoxLayout(contentArea);
    contentLayout->addLayout(paramLayout);
}

void Example1DXFImportDialog::onImportModeChanged(int index)
{
    const bool isFeMode =
        m_importModeCombo->itemData(index).toString() ==
        QStringLiteral("FiniteElement");
    m_modelNameEdit->setEnabled(isFeMode);
    m_partNameEdit->setEnabled(isFeMode);
}

void Example1DXFImportDialog::onDrawingSizeChanged(int index)
{
    const int profileLimit = m_drawingSizeCombo->itemData(index).toInt();
    const bool large =
        profileLimit > DxfImportDefaults::kDefaultMaxOutputEntities ||
        profileLimit == DxfImportDefaults::kUnlimitedOutputEntities;
    m_toleranceEdit->setText(QString::number(
        large
            ? DxfImportDefaults::kLargeDrawingCurveTolerance
            : DxfImportDefaults::kCurveTolerance,
        'g', 12));
}

Example1DXFImportDialog::~Example1DXFImportDialog() = default;

void Example1DXFImportDialog::onBrowse()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Select DXF File"),
        QString(),
        tr("DXF Files (*.dxf);;All Files (*.*)"));

    if (!filePath.isEmpty()) {
        m_dxfPathEdit->setText(QDir::fromNativeSeparators(filePath));
        QStringList layers;
        if (collectDxfLayers(filePath, layers)) {
            m_ignoreLayersCombo->setItems(layers);
        }
    }
}

void Example1DXFImportDialog::onDxfPathEditingFinished()
{
    const QString filePath = m_dxfPathEdit->text().trimmed();
    if (filePath.isEmpty() || !QFileInfo::exists(filePath) ||
        QFileInfo(filePath).suffix().compare("dxf", Qt::CaseInsensitive) != 0) {
        return;
    }

    QStringList layers;
    if (collectDxfLayers(filePath, layers)) {
        m_ignoreLayersCombo->setItems(layers);
    }
}

void Example1DXFImportDialog::onCmdOk(int id)
{
    // Qt 不允许 C++ 异常穿过槽函数返回事件循环。SAM SDK/Python 命令边界可能抛出
    // 非 Qt 异常，因此用 stage 记录执行位置，并在本槽函数最外层统一截获。
    const char* stage = "validate input";
    try {
    QString path = m_dxfPathEdit->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("Warning"),
            tr("Please enter or select a DXF file path."));
        return;
    }
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, tr("Warning"),
            tr("File does not exist:\n%1").arg(path));
        return;
    }
    if (QFileInfo(path).suffix().compare("dxf", Qt::CaseInsensitive) != 0) {
        QMessageBox::warning(this, tr("Warning"), tr("Please select a DXF file."));
        return;
    }

    bool baseOkX = false, baseOkY = false, baseOkZ = false;
    double baseX = m_baseXEdit->text().toDouble(&baseOkX);
    double baseY = m_baseYEdit->text().toDouble(&baseOkY);
    double baseZ = m_baseZEdit->text().toDouble(&baseOkZ);
    bool toleranceOk = false;
    double tolerance = m_toleranceEdit->text().toDouble(&toleranceOk);
    if (!baseOkX || !baseOkY || !baseOkZ) {
        QMessageBox::warning(this, tr("Warning"),
            tr("Please enter valid numeric values for Base Point X, Y, Z."));
        return;
    }
    if (!toleranceOk ||
        tolerance < DxfImportDefaults::kMinimumCurveTolerance ||
        tolerance > DxfImportDefaults::kMaximumCurveTolerance) {
        QMessageBox::warning(
            this,
            tr("Warning"),
            tr("Please enter a curve tolerance between %1 and %2.")
                .arg(DxfImportDefaults::kMinimumCurveTolerance)
                .arg(DxfImportDefaults::kMaximumCurveTolerance));
        return;
    }

    const QString importMode = m_importModeCombo->currentData().toString();
    const bool isFeMode =
        importMode.compare(QStringLiteral("FiniteElement"), Qt::CaseInsensitive) == 0;
    const QString modelName = m_modelNameEdit->text().trimmed();
    const QString partName = m_partNameEdit->text().trimmed();
    const int maxOutputEntities = m_drawingSizeCombo->currentData().toInt();
    if (isFeMode && (modelName.isEmpty() || partName.isEmpty())) {
        QMessageBox::warning(this, tr("Warning"),
            tr("Finite Element mode requires both model and part names."));
        return;
    }

    const QString ignoreLayers = m_ignoreLayersCombo->checkedItems().join(',');
    const QString importPath = QDir::fromNativeSeparators(
        QFileInfo(path).absoluteFilePath());
    stage = "construct SAM arguments";
    // 前六项沿用位置参数；后四项使用命名参数，与核心端 args.Get 的名称一致。
    omuArguments args(10);
    args.Put(importPath);
    args.Put(baseX);
    args.Put(baseY);
    args.Put(baseZ);
    args.Put(tolerance);
    args.Put(ignoreLayers);
    args.Put(importMode, "importMode");
    args.Put(modelName, "modelName");
    args.Put(partName, "partName");
    args.Put(maxOutputEntities, "maxOutputEntities");

    stage = "construct Example1.importDxf command";
    const omuMethodCall methodCall("Example1", "importDxf", args);
    QString cmd;
    cmd.append(methodCall);

    stage = "get SAM command delivery singleton";
    cmdGCommandDeliveryRole& delivery = cmdGCommandDeliveryRole::Instance();

    stage = "import Example1 module";
    // SendCommand 会把 SAM 内核回复中的 nexException 重新抛出。Qt 槽函数不能让
    // 这种异常逃回事件循环，因此这里改用 SDK 其他新插件采用的同步回复模式。
    delivery.SyncSendCommand("import Example1", false, false);
    if (delivery.HasReply() && delivery.GetCmdReply().HasValue()) {
        const QString report = delivery.GetCmdReply().Exception().UserReport();
        qCritical("DXF import module load failed: %s", qPrintable(report));
        QMessageBox::critical(
            this,
            tr("DXF Import Error"),
            tr("Failed to import Example1 module:\n\n%1").arg(report));
        return;
    }

    stage = "execute Example1.importDxf";
    delivery.SyncSendCommand(cmd, false, false);
    if (delivery.HasReply() && delivery.GetCmdReply().HasValue()) {
        const QString report = delivery.GetCmdReply().Exception().UserReport();
        qCritical("DXF import command failed: %s", qPrintable(report));
        QMessageBox::critical(
            this,
            tr("DXF Import Error"),
            tr("Example1.importDxf failed:\n\n%1").arg(report));
        return;
    }
    stage = "close DXF import dialog";
    SAMDataDialog::onCmdOk(id);
    }
    catch (const std::exception& error) {
        qCritical(
            "DXF import caught std::exception at stage '%s': %s",
            stage,
            error.what());
        QMessageBox::critical(
            this,
            tr("DXF Import Error"),
            tr("An exception occurred at stage: %1\n\n%2")
                .arg(QString::fromLatin1(stage),
                     QString::fromLocal8Bit(error.what())));
    }
    catch (...) {
        qCritical(
            "DXF import caught unknown C++ exception at stage '%s'",
            stage);
        QMessageBox::critical(
            this,
            tr("DXF Import Error"),
            tr("An unknown C++ exception occurred at stage: %1")
                .arg(QString::fromLatin1(stage)));
    }
}

namespace {

class LayerReader : public DRW_Interface
{
public:
    std::set<std::string> layers;

    void addLayer(const DRW_Layer& data) override { layers.insert(data.name); }
    void addHeader(const DRW_Header*) override {}
    void addLType(const DRW_LType&) override {}
    void addDimStyle(const DRW_Dimstyle&) override {}
    void addVport(const DRW_Vport&) override {}
    void addTextStyle(const DRW_Textstyle&) override {}
    void addAppId(const DRW_AppId&) override {}
    void setBlock(const int) override {}
    void addBlock(const DRW_Block&) override {}
    void endBlock() override {}
    void addPoint(const DRW_Point&) override {}
    void addLine(const DRW_Line&) override {}
    void addRay(const DRW_Ray&) override {}
    void addXline(const DRW_Xline&) override {}
    void addCircle(const DRW_Circle&) override {}
    void addArc(const DRW_Arc&) override {}
    void addEllipse(const DRW_Ellipse&) override {}
    void addPolyline(const DRW_Polyline&) override {}
    void addSpline(const DRW_Spline*) override {}
    void addKnot(const DRW_Entity&) override {}
    void addInsert(const DRW_Insert&) override {}
    void addTrace(const DRW_Trace&) override {}
    void add3dFace(const DRW_3Dface&) override {}
    void addSolid(const DRW_Solid&) override {}
    void addLWPolyline(const DRW_LWPolyline&) override {}
    void addText(const DRW_Text&) override {}
    void addMText(const DRW_MText&) override {}
    void addDimAlign(const DRW_DimAligned*) override {}
    void addDimLinear(const DRW_DimLinear*) override {}
    void addDimRadial(const DRW_DimRadial*) override {}
    void addDimDiametric(const DRW_DimDiametric*) override {}
    void addDimAngular(const DRW_DimAngular*) override {}
    void addDimAngular3P(const DRW_DimAngular3p*) override {}
    void addDimOrdinate(const DRW_DimOrdinate*) override {}
    void addLeader(const DRW_Leader*) override {}
    void addHatch(const DRW_Hatch*) override {}
    void addViewport(const DRW_Viewport&) override {}
    void addImage(const DRW_Image*) override {}
    void linkImage(const DRW_ImageDef*) override {}
    void addComment(const char*) override {}
    void addPlotSettings(const DRW_PlotSettings*) override {}
    void writeHeader(DRW_Header&) override {}
    void writeBlocks() override {}
    void writeBlockRecords() override {}
    void writeEntities() override {}
    void writeLTypes() override {}
    void writeLayers() override {}
    void writeTextstyles() override {}
    void writeVports() override {}
    void writeDimstyles() override {}
    void writeObjects() override {}
    void writeAppId() override {}
};

} // namespace

bool collectDxfLayers(const QString& filePath, QStringList& layers)
{
    layers.clear();
    if (filePath.isEmpty()) {
        return false;
    }

    DxfInputFile inputFile(filePath);
    if (!inputFile.prepare()) {
        return false;
    }

    dxfRW dxf(inputFile.encodedPath().constData());
    LayerReader reader;
    if (!dxf.read(&reader, true)) {
        return false;
    }

    for (const std::string& layer : reader.layers) {
        layers.append(QString::fromLocal8Bit(layer.c_str()));
    }
    layers.sort(Qt::CaseInsensitive);
    return true;
}

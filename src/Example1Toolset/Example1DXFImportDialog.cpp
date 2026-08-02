#include <Example1DXFImportDialog.h>
#include <Example1Form.h>
#include <DxfLayerReader.h>
#include <DxfImportDefaults.h>
#include <MultiSelectComboBox.h>

#include <QValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>

#include <omuArguments.h>
#include <omuMethodCall.h>
#include <cmdGCommandDeliveryRole.h>



Example1DXFImportDialog::Example1DXFImportDialog(Example1Form* form)
	:SAMDataDialog(form, tr("DXF Import"), OK | CANCEL)
{
	setMinimumSize(470, 270);

	// ======== DXF Path ========
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

	// ======== Base Point ========
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

	// ======== Drawing Size / Output Budget ========
	QGroupBox* drawingSizeGroup =
		new QGroupBox(tr("Drawing Size"), this);
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

	// ======== Curve Discretization ========
	QGroupBox* discretizationGroup =
		new QGroupBox(tr("Curve Discretization"), this);

	m_toleranceValidator =
		new QDoubleValidator(
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

	QFormLayout* discretizationLayout =
		new QFormLayout(discretizationGroup);
	discretizationLayout->addRow(tr("Tolerance (mm):"), m_toleranceEdit);

	paramLayout->addWidget(discretizationGroup);

	// ======== Ignore Layers ========
	QGroupBox* layerGroup =
		new QGroupBox(tr("Ignore Layers"), this);

	m_ignoreLayersCombo = new MultiSelectComboBox(layerGroup);
	m_ignoreLayersCombo->setToolTip(
		tr("Select one or more layers. Entities on selected layers will be excluded from import."));

	QVBoxLayout* layerLayout = new QVBoxLayout(layerGroup);
	layerLayout->addWidget(m_ignoreLayersCombo);

	paramLayout->addWidget(layerGroup);

	// ======== Import Mode ========
	QGroupBox* importModeGroup =
		new QGroupBox(tr("Import Mode"), this);

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


Example1DXFImportDialog::~Example1DXFImportDialog()
{

}


void Example1DXFImportDialog::onBrowse()
{
	QString filePath = QFileDialog::getOpenFileName(
		this,
		tr("Select DXF File"),
		QString(),
		tr("DXF Files (*.dxf);;All Files (*.*)")
	);

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
	QString path = m_dxfPathEdit->text().trimmed();

	// validate
	if (path.isEmpty()) {
		QMessageBox::warning(
			this,
			tr("Warning"),
			tr("Please enter or select a DXF file path.")
		);
		return;
	}

	if (!QFileInfo::exists(path)) {
		QMessageBox::warning(
			this,
			tr("Warning"),
			tr("File does not exist:\n%1").arg(path)
		);
		return;
	}

	if (QFileInfo(path).suffix().compare("dxf", Qt::CaseInsensitive) != 0) {
		QMessageBox::warning(
			this,
			tr("Warning"),
			tr("Please select a DXF file.")
		);
		return;
	}

	// validate base point
	bool baseOkX = false, baseOkY = false, baseOkZ = false;
	double baseX = m_baseXEdit->text().toDouble(&baseOkX);
	double baseY = m_baseYEdit->text().toDouble(&baseOkY);
	double baseZ = m_baseZEdit->text().toDouble(&baseOkZ);
	bool toleranceOk = false;
	double tolerance = m_toleranceEdit->text().toDouble(&toleranceOk);

	if (!baseOkX || !baseOkY || !baseOkZ) {
		QMessageBox::warning(
			this,
			tr("Warning"),
			tr("Please enter valid numeric values for Base Point X, Y, Z.")
		);
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
				.arg(DxfImportDefaults::kMaximumCurveTolerance)
		);
		return;
	}

	const QString importMode =
		m_importModeCombo->currentData().toString();
	const bool isFeMode =
		importMode.compare(QStringLiteral("FiniteElement"), Qt::CaseInsensitive) == 0;
	const QString modelName = m_modelNameEdit->text().trimmed();
	const QString partName = m_partNameEdit->text().trimmed();
	const int maxOutputEntities =
		m_drawingSizeCombo->currentData().toInt();
	if (isFeMode && (modelName.isEmpty() || partName.isEmpty())) {
		QMessageBox::warning(
			this,
			tr("Warning"),
			tr("Finite Element mode requires both model and part names.")
		);
		return;
	}

	// call importDxf
	const QString ignoreLayers = m_ignoreLayersCombo->checkedItems().join(',');
	const QString importPath = QDir::fromNativeSeparators(
		QFileInfo(path).absoluteFilePath());
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
	omuMethodCall mc("Example1", "importDxf", args);

	QString cmd;
	cmd.append(mc);
	cmdGCommandDeliveryRole::Instance().SendCommand("import Example1");
	cmdGCommandDeliveryRole::Instance().SendCommand(cmd);

	// close dialog
	SAMDataDialog::onCmdOk(id);
}

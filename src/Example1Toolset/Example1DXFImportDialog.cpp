#include <Example1DXFImportDialog.h>
#include <Example1Form.h>

#include <QValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
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

	/*************************DXF Path*************************************/
	QGroupBox* dxfGroup = new QGroupBox(tr("DXF File"), this);

	m_dxfPathEdit = new QLineEdit(dxfGroup);
	m_dxfPathEdit->setPlaceholderText(tr("Enter or paste DXF file path"));

	m_browseButton = new QPushButton(tr("Browse..."), dxfGroup);
	connect(m_browseButton, SIGNAL(clicked()), this, SLOT(onBrowse()));

	QHBoxLayout* pathLayout = new QHBoxLayout;
	pathLayout->addWidget(m_dxfPathEdit);
	pathLayout->addWidget(m_browseButton);

	QVBoxLayout* groupLayout = new QVBoxLayout(dxfGroup);
	groupLayout->addLayout(pathLayout);

	QVBoxLayout* paramLayout = new QVBoxLayout;
	paramLayout->addWidget(dxfGroup);

	/*************************Base Point*************************************/
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

	/*************************Curve Discretization***************************/
	QGroupBox* discretizationGroup =
		new QGroupBox(tr("Curve Discretization"), this);

	m_toleranceValidator =
		new QDoubleValidator(0.000000000001, 1000.0, 12, discretizationGroup);
	m_toleranceValidator->setNotation(QDoubleValidator::StandardNotation);

	m_toleranceEdit = new QLineEdit(discretizationGroup);
	m_toleranceEdit->setText("0.01");
	m_toleranceEdit->setValidator(m_toleranceValidator);
	m_toleranceEdit->setPlaceholderText("0.01");
	m_toleranceEdit->setToolTip(
		tr("Maximum deviation between a curve and its line segments."));

	QFormLayout* discretizationLayout =
		new QFormLayout(discretizationGroup);
	discretizationLayout->addRow(tr("Tolerance (mm):"), m_toleranceEdit);

	paramLayout->addWidget(discretizationGroup);

	QHBoxLayout* contentLayout = new QHBoxLayout(contentArea);
	contentLayout->addLayout(paramLayout);

}


/// Destructor.
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

	if (!toleranceOk || tolerance <= 0.0) {
		QMessageBox::warning(
			this,
			tr("Warning"),
			tr("Please enter a curve tolerance greater than zero.")
		);
		return;
	}

	// call importDxf
	const QString importPath = QDir::fromNativeSeparators(
		QFileInfo(path).absoluteFilePath());
	omuArguments args(5);
	args.Put(importPath);
	args.Put(baseX);
	args.Put(baseY);
	args.Put(baseZ);
	args.Put(tolerance);
	omuMethodCall mc("Example1", "importDxf", args);

	QString cmd;
	cmd.append(mc);
	cmdGCommandDeliveryRole::Instance().SendCommand("import Example1");
	cmdGCommandDeliveryRole::Instance().SendCommand(cmd);

	// close dialog
	SAMDataDialog::onCmdOk(id);
}

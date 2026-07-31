#include <Example1DB.h>
#include <Example1Form.h>

#include <QValidator>
#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <QMessageBox>

#include <omuArguments.h>
#include <omuMethodCall.h>

#include <cmdGCommandDeliveryRole.h>




Example1DB::Example1DB(Example1Form* form)
	:SAMDataDialog(form, tr("Example1"), OK | CANCEL)
{
	setMinimumSize(470, 200);

	// ======== Parameters ========
	QGroupBox* geomGroup = new QGroupBox(tr("Example1GroupBox"));
	QDoubleValidator* doubleValidator = new QDoubleValidator(geomGroup);

	lengthEdit = new QLineEdit("10", geomGroup);
	widthEdit = new QLineEdit("5", geomGroup);

	QFormLayout* geomLayout = new QFormLayout(geomGroup);
	geomLayout->addRow(tr("Length (L):"), lengthEdit);
	geomLayout->addRow(tr("Width (W):"), widthEdit);

	QVBoxLayout* paramLayout = new QVBoxLayout;
	paramLayout->addWidget(geomGroup);

	QHBoxLayout* contentLayout = new QHBoxLayout(contentArea);
	contentLayout->addLayout(paramLayout);

}

Example1DB::~Example1DB()
{

}


void Example1DB::onCmdOk(int id)
{
	double length, width;
	length = lengthEdit->text().toDouble();
	width = widthEdit->text().toDouble();
	omuArguments args(2);
	args.Put(length);
	args.Put(width);
	omuMethodCall mc("Example1", "clacArea", args);
	QString cmd;
	cmd.append(mc);
	cmdGCommandDeliveryRole::Instance().SendCliCommand("import Example1");
	cmdGCommandDeliveryRole::Instance().SendCliCommand("cmd");



	
}





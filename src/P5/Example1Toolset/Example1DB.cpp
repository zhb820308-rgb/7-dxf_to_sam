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
	:SAMDataDialog(form, tr("Example1"), OK | ButtonID::APPLY | CANCEL)
{
	setMinimumSize(470, 200);

	/*************************Parameters*************************************/
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

/// Destructor.
Example1DB::~Example1DB()
{

}


void Example1DB::onCmdOk(int id)
{
	double length, width;
	length = lengthEdit->text().toDouble();
	width = widthEdit->text().toDouble();

	omuArguments args(2);
	args.Put(length, "Length");
	args.Put(width, "Width");

	omuMethodCall mc("Example1", "calcArea", args);

	QString cmd;
	cmd.append(mc);

	cmdGCommandDeliveryRole::Instance().SendCommand("import Example1");
	cmdGCommandDeliveryRole::Instance().SendCommand(cmd);
}

void Example1DB::onCmdApply(int id)
{
	int nodeID = 1;
	
	omuArguments args(1);
	args.Put(nodeID, "NodeID");
	
	omuMethodCall mc("mdb.models['Model-1'].parts['Part-1']", "getNodeInfo", args);

	QString cmd;
	cmd.append(mc);

	cmdGCommandDeliveryRole::Instance().SendCommand("import Example1");
	cmdGCommandDeliveryRole::Instance().SendCommand(cmd);
}





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
	
}





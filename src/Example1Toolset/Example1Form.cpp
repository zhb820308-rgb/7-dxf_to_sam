#include <Example1Form.h>
#include <Example1DB.h>
#include <Example1DXFImportDialog.h>


Example1Form::Example1Form(SAMGuiObjectManager* owner)
	: SAMForm(owner)
{
}

Example1Form::~Example1Form()
{

}

SAMDialog* Example1Form::getFirstDialog()
{
	return new Example1DXFImportDialog(this);
}

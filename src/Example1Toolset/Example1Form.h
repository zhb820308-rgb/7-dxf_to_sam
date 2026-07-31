#ifndef Example1Form_h
#define Example1Form_h

// Begin local includes
#include <SAMCreateEditForm.h>

// Forward declarations
class SAMGuiObjectManager;

// Class definition
/// @brief Top-level SAM form that owns the DXF import dialog.
class Example1Form : public SAMForm
{
	Q_OBJECT
	friend class Example1DB;
public:
	Example1Form(SAMGuiObjectManager* owner);

	~Example1Form();

protected:
	virtual SAMDialog* getFirstDialog();

};

#endif  // #ifndef Example1Form_h

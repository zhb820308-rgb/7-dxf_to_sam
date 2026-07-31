/* -*- mode: c++ -*- */
#ifndef Example1DB_h
#define Example1DB_h

// Begin Local Includes
#include <SAMDataDialog.h>

// Forward declarations
class Example1Form;
class QLineEdit;
//
// Class definition
//

/// @brief Minimal database dialog for SAM context.
class Example1DB : public SAMDataDialog
{
	Q_OBJECT
public:
	/// @brief Constructor — creates UI fields for length and width.
	Example1DB(Example1Form* form);

	/// @brief Destructor.
	~Example1DB();

private:
	QLineEdit* lengthEdit;
	QLineEdit* widthEdit;

private slots:
	void onCmdOk(int) override;

};

#endif
#ifndef CSSCAMBERDB_H
#define CSSCAMBERDB_H

#include <SAMDataDialog.h>

class CSSForm;
class CSSParameters;
class QLineEdit;

class CSSCamberDB: public SAMDataDialog
{
    Q_OBJECT

public:
    CSSCamberDB(CSSForm*, CSSParameters*);
    ~CSSCamberDB();

public slots:
    virtual void onActionButtonClicked(int) override;
    virtual void onCmdContinue(int) override;
    virtual void onCmdCancel(int) override;

private:
    CSSParameters* css_params_ptr = nullptr;

    QLineEdit* camber_height_le_ptr = nullptr;
    QLineEdit* deck_distance_le_ptr = nullptr;

    void setupUi();
};

#endif // CSSCAMBERDB_H

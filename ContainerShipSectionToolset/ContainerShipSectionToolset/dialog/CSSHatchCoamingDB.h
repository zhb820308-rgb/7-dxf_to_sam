#ifndef CSSHATCHCOAMINGDB_H
#define CSSHATCHCOAMINGDB_H

#include <SAMDataDialog.h>

class CSSForm;
class CSSParameters;
class QLineEdit;

class CSSHatchCoamingDB: public SAMDataDialog
{
    Q_OBJECT

public:
    CSSHatchCoamingDB(CSSForm*, CSSParameters*);
    ~CSSHatchCoamingDB();

public slots:
    virtual void onActionButtonClicked(int) override;
    virtual void onCmdContinue(int) override;
    virtual void onCmdCancel(int) override;

private:
    CSSParameters* cssParams = nullptr;

    QLineEdit* hcHeightLineEdit;
    QLineEdit* hcWidthLineEdit;

    void setupUi();
};

#endif // CSSHATCHCOAMINGDB_H

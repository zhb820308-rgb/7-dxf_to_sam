#ifndef CSSGRIDERDB_H
#define CSSGRIDERDB_H

#include <SAMDataDialog.h>

class CSSForm;
class CSSParameters;
class QSpinBox;
class QListWidget;

class CSSGriderDB: public SAMDataDialog
{
    Q_OBJECT

public:
    CSSGriderDB(CSSForm*, CSSParameters*);
    ~CSSGriderDB();

public slots:
    virtual void onActionButtonClicked(int) override;
    virtual void onCmdContinue(int) override;
    virtual void onCmdCancel(int) override;

private:
    CSSParameters* cssParams = nullptr;

    QSpinBox* griderSpinBox;
    QListWidget* griderListWidget;

    void setupUi();

private slots:
    void refreshInputs(int);
};

#endif // CSSGRIDERDB_H

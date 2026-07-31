#ifndef CSSSTRINGERDB_H
#define CSSSTRINGERDB_H

#include <SAMDataDialog.h>

class CSSForm;
class CSSParameters;
class QSpinBox;
class QListWidget;

class CSSStringerDB: public SAMDataDialog
{
    Q_OBJECT

public:
    CSSStringerDB(CSSForm*, CSSParameters*);
    ~CSSStringerDB();

public slots:
    virtual void onActionButtonClicked(int) override;
    virtual void onCmdContinue(int) override;
    virtual void onCmdCancel(int) override;

private:
    CSSParameters* css_params_ptr = nullptr;

    QSpinBox* stringersSpinBox;
    QListWidget* stringersListWidget;

    void setupUi();

private slots:
    void refreshInputs(int);
};

#endif // CSSSTRINGERDB_H

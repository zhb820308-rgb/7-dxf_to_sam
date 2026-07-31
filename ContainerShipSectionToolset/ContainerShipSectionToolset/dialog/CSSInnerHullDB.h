#ifndef CSSINNERHULLDB_H
#define CSSINNERHULLDB_H

#include <SAMDataDialog.h>

class CSSForm;
class CSSParameters;
class QLineEdit;

class CSSInnerHullDB: public SAMDataDialog
{
    Q_OBJECT

public:
    CSSInnerHullDB(CSSForm*, CSSParameters*);
    ~CSSInnerHullDB();

public slots:
    virtual void onActionButtonClicked(int) override;
    virtual void onCmdContinue(int) override;
    virtual void onCmdCancel(int) override;

private:
    CSSParameters* css_params_ptr = nullptr;

    QLineEdit* innerHullHeightEdit;
    QLineEdit* innerHullDistanceEdit;

    void setupUi();
};

#endif // CSSINNERHULLDB_H

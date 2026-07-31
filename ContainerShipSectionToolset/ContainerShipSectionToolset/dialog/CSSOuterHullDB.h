#ifndef CSSOUTERHULLDB_H
#define CSSOUTERHULLDB_H

#include <SAMDataDialog.h>

class CSSForm;
class CSSParameters;
class QLineEdit;

class CSSOuterHullDB: public SAMDataDialog
{
    Q_OBJECT

public:
    CSSOuterHullDB(CSSForm*, CSSParameters*);
    ~CSSOuterHullDB();

public slots:
    virtual void onActionButtonClicked(int) override;
    virtual void onCmdContinue(int) override;
    virtual void onCmdCancel(int) override;

private:
    CSSParameters* css_params_ptr = nullptr;

    QLineEdit* half_breadth_le_ptr = nullptr;
    QLineEdit* depth_le_ptr = nullptr;
    QLineEdit* bilge_radius_le_ptr = nullptr;
    QLineEdit* deck_corner_radius_le_ptr = nullptr;

    void setupUi();
};

#endif // CSSOUTERHULLDB_H

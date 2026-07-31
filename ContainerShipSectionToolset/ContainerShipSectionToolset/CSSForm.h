#ifndef CSSFORM_H
#define CSSFORM_H

#include <SAMForm.h>
#include <QString>
#include <QPoint>

class SAMGuiObjectManager;
class SAMDialog;
class QObject;
class CSSParameters;

class CSSForm: public SAMForm
{
    Q_OBJECT

public:
    CSSForm(SAMGuiObjectManager*);
    ~CSSForm();

    virtual void activate() override;
    int rollbackMode();
    virtual int continueMode() override;
    virtual int commit() override;
    virtual void cancel(QObject* = nullptr, const char* = nullptr) override;

    QString getCmdPrefix() const;
    QPoint getPrevDialogCenter() const;

public slots:
    virtual int onCmdActivate(QObject*, uint, void*) override;

protected:
    virtual SAMDialog* getFirstDialog() override;
    SAMDialog* getPreviousDialog(SAMDialog*);
    virtual SAMDialog* getNextDialog(SAMDialog*) override;

private:
    CSSForm(const CSSForm&) = delete;
    CSSForm& operator=(const CSSForm&) = delete;

    QString cmd_target;
    QPoint prev_dialog_center;
    CSSParameters* css_params_ptr = nullptr;

    void setDialogCenter();
};

#endif // CSSFORM_H

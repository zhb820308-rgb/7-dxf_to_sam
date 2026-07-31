#ifndef CSSPREPROCESSDB_H
#define CSSPREPROCESSDB_H

#include <SAMDataDialog.h>

class CSSForm;
class CSSParameters;
class QLineEdit;

class CSSPreprocessDB: public SAMDataDialog
{
    Q_OBJECT

public:
    CSSPreprocessDB(CSSForm*, CSSParameters*);
    ~CSSPreprocessDB();

public slots:
    virtual void onActionButtonClicked(int) override;
    virtual void onCmdContinue(int) override;
    virtual void onCmdCancel(int) override;

private:
    CSSParameters* css_params_ptr = nullptr;

    QLineEdit* file_path_le_ptr = nullptr;

    void setupUi();

private slots:
    void openFileDialog();
};

#endif // CSSPREPROCESSDB_H

#ifndef Example1DXFImportDialog_h
#define Example1DXFImportDialog_h

#include <SAMDataDialog.h>

class QLineEdit;
class QPushButton;
class QDoubleValidator;
class Example1Form;

class Example1DXFImportDialog : public SAMDataDialog
{
    Q_OBJECT
public:
    explicit Example1DXFImportDialog(Example1Form* form);
    ~Example1DXFImportDialog();

private slots:
    void onCmdOk(int id) override;
    void onBrowse();

private:
    QLineEdit* m_dxfPathEdit;
    QLineEdit* m_baseXEdit;
    QLineEdit* m_baseYEdit;
    QLineEdit* m_baseZEdit;
    QPushButton* m_browseButton;
    QDoubleValidator* m_doubleValidator;
    static const int BROWSE_BUTTON_ID = 2;
};

#endif
#ifndef CSSPOSTPROCESSDB_H
#define CSSPOSTPROCESSDB_H

#include <SAMDataDialog.h>
#include <SAMComboBox.h>
class CSSForm;
class CSSParameters;
class QCheckBox;
class QLineEdit;
class QSpinBox;
class QComboBox;

class CSSPostprocessDB: public SAMDataDialog
{
    Q_OBJECT

public:
    CSSPostprocessDB(CSSForm*, CSSParameters*);
    ~CSSPostprocessDB();

public slots:
    virtual void onActionButtonClicked(int) override;
    virtual void onCmdOk(int) override;
    virtual void onCmdCancel(int) override;

private:
    CSSParameters* cssParams = nullptr;

    QCheckBox* mirrorBox;
    QCheckBox* extrudeBox;
    QLineEdit* extrudeDx;
    QSpinBox* extrudeTimes;
    QCheckBox* refineBox;
    QCheckBox* stiffenerCheckBox;
	SAMComboBox* stiffenerProfileBox;
	SAMComboBox* stiffenerMaterialBox;

    QLineEdit* filePathEdit;

    void setupUi();

private slots:
    void toggleExtrudeControls();
    void openFileDialog();
};

#endif // CSSPOSTPROCESSDB_H

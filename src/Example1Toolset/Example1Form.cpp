#include <Example1Form.h>

#include <DxfImportDialog.h>

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QVBoxLayout>

#include <cmdGCommandDeliveryRole.h>
#include <omuArguments.h>
#include <omuMethodCall.h>

Example1Form::Example1Form(SAMGuiObjectManager* owner)
    : SAMForm(owner)
{
}

Example1Form::~Example1Form() = default;

SAMDialog* Example1Form::getFirstDialog()
{
    return new Example1DXFImportDialog(this);
}

Example1DB::Example1DB(Example1Form* form)
    : SAMDataDialog(form, tr("Example1"), OK | CANCEL),
      m_lengthEdit(nullptr),
      m_widthEdit(nullptr)
{
    setMinimumSize(470, 200);

    QGroupBox* geometryGroup = new QGroupBox(tr("Example1GroupBox"));
    m_lengthEdit = new QLineEdit(QStringLiteral("10"), geometryGroup);
    m_widthEdit = new QLineEdit(QStringLiteral("5"), geometryGroup);

    QFormLayout* geometryLayout = new QFormLayout(geometryGroup);
    geometryLayout->addRow(tr("Length (L):"), m_lengthEdit);
    geometryLayout->addRow(tr("Width (W):"), m_widthEdit);

    QVBoxLayout* parameterLayout = new QVBoxLayout;
    parameterLayout->addWidget(geometryGroup);

    QHBoxLayout* contentLayout = new QHBoxLayout(contentArea);
    contentLayout->addLayout(parameterLayout);
}

Example1DB::~Example1DB() = default;

void Example1DB::onCmdOk(int id)
{
    Q_UNUSED(id);

    const double length = m_lengthEdit->text().toDouble();
    const double width = m_widthEdit->text().toDouble();

    omuArguments arguments(2);
    arguments.Put(length);
    arguments.Put(width);
    omuMethodCall methodCall("Example1", "clacArea", arguments);
    QString command;
    command.append(methodCall);

    cmdGCommandDeliveryRole::Instance().SendCliCommand("import Example1");
    cmdGCommandDeliveryRole::Instance().SendCliCommand("cmd");
}

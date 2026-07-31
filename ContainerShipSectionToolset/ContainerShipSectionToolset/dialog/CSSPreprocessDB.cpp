#include "CSSPreprocessDB.h"

#include "CSSForm.h"
#include "CSSParameters.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QFileDialog>
#include <QDir>

#include <QDebug>

CSSPreprocessDB::CSSPreprocessDB(CSSForm* form_ptr, CSSParameters* params_ptr):
    SAMDataDialog(form_ptr,
        tr("Start Container Ship Section"),
        ButtonID::CONTINUE | ButtonID::CANCEL)
{
    css_params_ptr = params_ptr;

    setupUi();
}

CSSPreprocessDB::~CSSPreprocessDB()
{
    // qDebug() << "CSSPreprocessDB::~CSSPreprocessDB() called";
}

void CSSPreprocessDB::setupUi()
{
    setMinimumSize(537, 125);

    QHBoxLayout* content_layout_ptr = new QHBoxLayout(contentArea);

    file_path_le_ptr = new QLineEdit(contentArea);
    file_path_le_ptr->setPlaceholderText(tr("Select a file..."));
    file_path_le_ptr->setMinimumWidth(400);
    // file_path_le_ptr->setAcceptDrops(true);
    content_layout_ptr->addWidget(file_path_le_ptr);

    QPushButton* browse_btn_ptr = new QPushButton(tr("Browse..."), contentArea);
    connect(browse_btn_ptr, &QPushButton::clicked, this, &CSSPreprocessDB::openFileDialog);
    content_layout_ptr->addWidget(browse_btn_ptr);
}

void CSSPreprocessDB::onActionButtonClicked(int click_id)
{
    // qDebug() << "CSSPreprocessDB::onActionButtonClicked() called with click_id: " << click_id;
    SAMDataDialog::onActionButtonClicked(click_id); // call onCmd*()
    // qDebug() << "CSSPreprocessDB::onActionButtonClicked() completed.";
}

void CSSPreprocessDB::onCmdContinue(int click_id)
{
    // qDebug() << "CSSPreprocessDB::onCmdContinue() called with click_id: " << click_id;
    QString invalid_style_sheet = "QLineEdit { background: #FFCCCC; }";

    QString file_path = file_path_le_ptr->text().trimmed();
    if(!file_path.isEmpty())
    {
        if(!QDir().exists(file_path))
        {
            file_path_le_ptr->setStyleSheet(invalid_style_sheet);
            QMessageBox::information(this,
                tr("Invalid File Path"),
                tr("The specified file path does not exist."));
            return;
        }
        else
        {
            file_path_le_ptr->setStyleSheet("");
        }
        css_params_ptr->importFromFile(file_path);
    }

    // qDebug() << "file_path: " << file_path;
    // qDebug() << "size: " << size();

    SAMDataDialog::onCmdContinue(click_id); // call getMode()->continueMode()
    // qDebug() << "CSSPreprocessDB::onCmdContinue() completed.";
}

void CSSPreprocessDB::onCmdCancel(int click_id)
{
    // qDebug() << "CSSPreprocessDB::onCmdCancel() called with click_id: " << click_id;
    SAMDataDialog::onCmdCancel(click_id); // call getMode()->cancel(QObject*, const char*)
    // qDebug() << "CSSPreprocessDB::onCmdCancel() completed.";
}

void CSSPreprocessDB::openFileDialog()
{
    QString path = QFileDialog::getOpenFileName(this,
        tr("Open File"),
        QDir::toNativeSeparators(QDir::homePath()),
        tr("SAM Section Parameter Files (*.samsp)"));
    if(!path.isEmpty())
    {
        file_path_le_ptr->setText(path);
    }
}

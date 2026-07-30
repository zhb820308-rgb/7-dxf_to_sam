#include "MultiSelectComboBox.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDesktopWidget>
#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include <QVBoxLayout>

MultiSelectComboBox::MultiSelectComboBox(QWidget* parent)
    : QComboBox(parent),
      m_popup(nullptr),
      m_searchEdit(nullptr),
      m_layerList(nullptr)
{
    setEditable(true);
    setInsertPolicy(QComboBox::NoInsert);
    lineEdit()->setReadOnly(true);
    lineEdit()->setPlaceholderText(tr("Select layers to exclude"));
    setMinimumWidth(300);
    updateSummary();
}

MultiSelectComboBox::~MultiSelectComboBox()
{
    delete m_popup;
}

void MultiSelectComboBox::setItems(const QStringList& items)
{
    if (!m_popup) {
        createPopup();
    }

    QSignalBlocker blocker(m_layerList);
    m_layerList->clear();
    for (const QString& layer : items) {
        QListWidgetItem* item = new QListWidgetItem(layer, m_layerList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    updateSummary();
}

QStringList MultiSelectComboBox::checkedItems() const
{
    QStringList checked;
    if (!m_layerList) {
        return checked;
    }

    for (int i = 0; i < m_layerList->count(); ++i) {
        QListWidgetItem* item = m_layerList->item(i);
        if (item->checkState() == Qt::Checked) {
            checked.append(item->text());
        }
    }
    return checked;
}

void MultiSelectComboBox::createPopup()
{
    m_popup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    m_popup->setFrameShape(QFrame::Box);

    QVBoxLayout* layout = new QVBoxLayout(m_popup);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_searchEdit = new QLineEdit(m_popup);
    m_searchEdit->setPlaceholderText(tr("Search layers..."));
    connect(m_searchEdit, SIGNAL(textChanged(QString)),
            this, SLOT(filterItems(QString)));
    layout->addWidget(m_searchEdit);

    m_layerList = new QListWidget(m_popup);
    m_layerList->setSelectionMode(QAbstractItemView::NoSelection);
    m_layerList->setMinimumHeight(220);
    m_layerList->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    connect(m_layerList, SIGNAL(itemChanged(QListWidgetItem*)),
            this, SLOT(onItemChanged(QListWidgetItem*)));
    layout->addWidget(m_layerList);
}

void MultiSelectComboBox::showPopup()
{
    if (!m_popup) {
        createPopup();
    }

    if (m_popup->isVisible()) {
        m_popup->close();
        return;
    }

    m_searchEdit->clear();
    const int popupWidth = qMax(width(), 340);
    const int popupHeight = qMin(330, QApplication::desktop()->availableGeometry(this).height() - 20);
    m_popup->resize(popupWidth, popupHeight);

    QPoint topLeft = mapToGlobal(QPoint(0, height()));
    const QRect available = QApplication::desktop()->availableGeometry(this);
    if (topLeft.x() + popupWidth > available.right()) {
        topLeft.setX(available.right() - popupWidth + 1);
    }
    if (topLeft.y() + popupHeight > available.bottom()) {
        topLeft.setY(mapToGlobal(QPoint(0, 0)).y() - popupHeight);
    }

    m_popup->move(topLeft);
    m_popup->show();
    m_searchEdit->setFocus();
}

void MultiSelectComboBox::filterItems(const QString& text)
{
    const QString needle = text.trimmed();
    for (int i = 0; i < m_layerList->count(); ++i) {
        QListWidgetItem* item = m_layerList->item(i);
        item->setHidden(!needle.isEmpty() &&
                        !item->text().contains(needle, Qt::CaseInsensitive));
    }
}

void MultiSelectComboBox::onItemChanged(QListWidgetItem*)
{
    updateSummary();
    emit checkedItemsChanged();
}

void MultiSelectComboBox::updateSummary()
{
    const int count = checkedItems().size();
    lineEdit()->setText(count == 0
                        ? tr("No layers selected")
                        : tr("%1 layer(s) selected").arg(count));
}

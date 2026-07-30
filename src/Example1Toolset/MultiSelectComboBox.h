#ifndef MultiSelectComboBox_h
#define MultiSelectComboBox_h

#include <QComboBox>
#include <QStringList>

class QFrame;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

class MultiSelectComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit MultiSelectComboBox(QWidget* parent = nullptr);
    ~MultiSelectComboBox() override;

    void setItems(const QStringList& items);
    QStringList checkedItems() const;

signals:
    void checkedItemsChanged();

protected:
    void showPopup() override;

private slots:
    void filterItems(const QString& text);
    void onItemChanged(QListWidgetItem* item);

private:
    void createPopup();
    void updateSummary();

    QFrame* m_popup;
    QLineEdit* m_searchEdit;
    QListWidget* m_layerList;
};

#endif

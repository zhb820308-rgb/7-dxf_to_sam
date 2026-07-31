#ifndef MultiSelectComboBox_h
#define MultiSelectComboBox_h

#include <QComboBox>
#include <QStringList>

class QFrame;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

/// @brief Custom combo box with multi-select checkboxes and search filter.
///
/// Displays a popup frame with a search line-edit above a checklist.
/// Emits checkedItemsChanged() when any item state changes.
class MultiSelectComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit MultiSelectComboBox(QWidget* parent = nullptr);
    ~MultiSelectComboBox() override;

    /// @brief Replace all items in the list.
    void setItems(const QStringList& items);

    /// @brief Return the list of currently checked items.
    QStringList checkedItems() const;

signals:
    /// @brief Emitted when any checklist item is toggled.
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

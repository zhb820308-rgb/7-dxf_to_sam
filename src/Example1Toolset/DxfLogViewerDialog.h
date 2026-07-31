#ifndef DxfLogViewerDialog_h
#define DxfLogViewerDialog_h

#include <QDialog>
#include <QMap>
#include <QString>

class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/// @brief Dialog for browsing and viewing DXF import log files.
///
/// Supports filtering by severity level and entity type, expand/collapse
/// tree navigation, and structured display of log entries.
class DxfLogViewerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DxfLogViewerDialog(QWidget* parent = nullptr);

private slots:
    /// @brief Refresh the log file dropdown from the log directory.
    void refreshLogFiles();
    /// @brief Load and display the currently selected log file.
    void loadSelectedLog();
    /// @brief Expand all tree items.
    void expandAllItems();
    /// @brief Collapse all tree items.
    void collapseAllItems();

private:
    /// @brief Severity level filter mode for log display.
    enum FilterMode {
        SummaryWithDetails = 0,  ///< Show summary with detailed sub-items.
        AllLevels,               ///< Show all log entries.
        TraceOnly,               ///< Show only trace-level entries.
        DebugOnly,               ///< Show only debug-level entries.
        InfoOnly,                ///< Show only info-level entries.
        WarningOnly,             ///< Show only warning-level entries.
        ErrorOnly                ///< Show only error-level entries.
    };

    QString logRootDirectory() const;
    QString detectLevel(const QString& line) const;
    QString extractTimestamp(const QString& line, const QString& level) const;
    QString extractMessage(const QString& line, const QString& level) const;
    bool acceptsLine(const QString& line, const QString& level) const;
    bool matchesEntityFilter(const QString& line) const;
    void addLogLine(const QString& line);
    void styleItem(QTreeWidgetItem* item, const QString& level, bool isParent = false);
    QTreeWidgetItem* findOrCreateEntityItem(
        const QString& type, const QString& entityId, const QString& level);

    QComboBox* m_logFileCombo;
    QComboBox* m_levelCombo;
    QLineEdit* m_entityFilterEdit;
    QPushButton* m_refreshButton;
    QPushButton* m_expandButton;
    QPushButton* m_collapseButton;
    QLabel* m_statusLabel;
    QTreeWidget* m_logTree;
    QMap<QString, QTreeWidgetItem*> m_entityItems;
};

#endif

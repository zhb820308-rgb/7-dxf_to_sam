#ifndef DxfLogViewerDialog_h
#define DxfLogViewerDialog_h

#include <QDialog>
#include <QMap>
#include <QString>

class QComboBox;
class QBuffer;
class QFile;
class QLineEdit;
class QLabel;
class QPushButton;
class QTimer;
class QTextStream;
class QTreeWidget;
class QTreeWidgetItem;

class DxfLogViewerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DxfLogViewerDialog(QWidget* parent = nullptr);
    ~DxfLogViewerDialog() override;

private slots:
    void refreshLogFiles();
    void loadSelectedLog();
    void loadNextLogChunk();
    void cancelLoading();
    void expandAllItems();
    void collapseAllItems();

private:
    enum FilterMode {
        SummaryWithDetails = 0,
        AllLevels,
        TraceOnly,
        DebugOnly,
        InfoOnly,
        WarningOnly,
        ErrorOnly
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
    void finishLoading();

    QComboBox* m_logFileCombo;
    QComboBox* m_levelCombo;
    QLineEdit* m_entityFilterEdit;
    QPushButton* m_refreshButton;
    QPushButton* m_cancelLoadButton;
    QPushButton* m_expandButton;
    QPushButton* m_collapseButton;
    QLabel* m_statusLabel;
    QTreeWidget* m_logTree;
    QMap<QString, QTreeWidgetItem*> m_entityItems;
    QTimer* m_loadTimer;
    QBuffer* m_logBuffer;
    QTextStream* m_logStream;
    int m_scannedLineCount;
    int m_displayedLineCount;
    bool m_snapshotTruncated;
};

#endif

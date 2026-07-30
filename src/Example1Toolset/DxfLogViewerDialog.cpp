#include "DxfLogViewerDialog.h"

#include <QBrush>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace
{
    const int kPathRole = Qt::UserRole;

    QString entityKey(const QString& type, const QString& id)
    {
        return type.toUpper() + ':' + id;
    }

    QString captured(const QString& text, const QString& pattern, int group = 1)
    {
        const QRegularExpressionMatch match =
            QRegularExpression(pattern).match(text);
        return match.hasMatch() ? match.captured(group) : QString();
    }
}

DxfLogViewerDialog::DxfLogViewerDialog(QWidget* parent)
    : QDialog(parent),
      m_logFileCombo(new QComboBox(this)),
      m_levelCombo(new QComboBox(this)),
      m_entityFilterEdit(new QLineEdit(this)),
      m_refreshButton(new QPushButton(tr("Refresh"), this)),
      m_expandButton(new QPushButton(tr("Expand All"), this)),
      m_collapseButton(new QPushButton(tr("Collapse All"), this)),
      m_statusLabel(new QLabel(this)),
      m_logTree(new QTreeWidget(this))
{
    setWindowTitle(tr("DXF Import Log Viewer"));
    resize(1180, 720);
    setMinimumSize(850, 520);

    m_levelCombo->addItem(tr("Summary (expandable)"), SummaryWithDetails);
    m_levelCombo->addItem(tr("All levels"), AllLevels);
    m_levelCombo->addItem(tr("Trace"), TraceOnly);
    m_levelCombo->addItem(tr("Debug"), DebugOnly);
    m_levelCombo->addItem(tr("Info"), InfoOnly);
    m_levelCombo->addItem(tr("Warning"), WarningOnly);
    m_levelCombo->addItem(tr("Error / Critical"), ErrorOnly);

    m_entityFilterEdit->setPlaceholderText(tr("Entity ID (empty = all)"));
    m_entityFilterEdit->setClearButtonEnabled(true);

    m_logTree->setColumnCount(5);
    m_logTree->setHeaderLabels(QStringList()
        << tr("Time") << tr("Level") << tr("Type")
        << tr("Entity ID") << tr("Details"));
    m_logTree->setAlternatingRowColors(true);
    m_logTree->setUniformRowHeights(true);
    m_logTree->setRootIsDecorated(true);
    m_logTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_logTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_logTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_logTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_logTree->header()->setSectionResizeMode(4, QHeaderView::Stretch);

    QHBoxLayout* filters = new QHBoxLayout;
    filters->addWidget(new QLabel(tr("Log:"), this));
    filters->addWidget(m_logFileCombo, 3);
    filters->addWidget(new QLabel(tr("Level:"), this));
    filters->addWidget(m_levelCombo);
    filters->addWidget(new QLabel(tr("Entity:"), this));
    filters->addWidget(m_entityFilterEdit);
    filters->addWidget(m_refreshButton);

    QHBoxLayout* actions = new QHBoxLayout;
    actions->addWidget(m_statusLabel, 1);
    actions->addWidget(m_expandButton);
    actions->addWidget(m_collapseButton);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(filters);
    layout->addWidget(m_logTree, 1);
    layout->addLayout(actions);

    connect(m_refreshButton, SIGNAL(clicked()), this, SLOT(refreshLogFiles()));
    connect(m_logFileCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(loadSelectedLog()));
    connect(m_levelCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(loadSelectedLog()));
    connect(m_entityFilterEdit, SIGNAL(textChanged(QString)),
            this, SLOT(loadSelectedLog()));
    connect(m_expandButton, SIGNAL(clicked()), this, SLOT(expandAllItems()));
    connect(m_collapseButton, SIGNAL(clicked()), this, SLOT(collapseAllItems()));

    refreshLogFiles();
}

QString DxfLogViewerDialog::logRootDirectory() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath("logs");
}

void DxfLogViewerDialog::refreshLogFiles()
{
    const QString previousPath = m_logFileCombo->currentData(kPathRole).toString();
    m_logFileCombo->blockSignals(true);
    m_logFileCombo->clear();

    const QDir importDirectory(QDir(logRootDirectory()).filePath("imports"));
    const QFileInfoList files = importDirectory.entryInfoList(
        QStringList() << "dxf_import_*.log", QDir::Files, QDir::Time);

    for (const QFileInfo& file : files)
    {
        m_logFileCombo->addItem(
            tr("Import: %1  (%2)")
                .arg(file.completeBaseName())
                .arg(file.lastModified().toString("yyyy-MM-dd HH:mm:ss")));
        m_logFileCombo->setItemData(
            m_logFileCombo->count() - 1, file.absoluteFilePath(), kPathRole);
    }

    const QFileInfo errorLog(QDir(logRootDirectory()).filePath("dxf_import_errors.log"));
    if (errorLog.exists())
    {
        m_logFileCombo->addItem(tr("Error summary: dxf_import_errors.log"));
        m_logFileCombo->setItemData(
            m_logFileCombo->count() - 1, errorLog.absoluteFilePath(), kPathRole);
    }

    int index = previousPath.isEmpty() ? 0 :
        m_logFileCombo->findData(previousPath, kPathRole);
    if (index < 0)
        index = 0;
    if (m_logFileCombo->count() > 0)
        m_logFileCombo->setCurrentIndex(index);
    m_logFileCombo->blockSignals(false);

    loadSelectedLog();
}

QString DxfLogViewerDialog::detectLevel(const QString& line) const
{
    static const QStringList levels = QStringList()
        << "trace" << "debug" << "info"
        << "warning" << "error" << "critical";
    for (const QString& level : levels)
    {
        if (line.contains('[' + level + ']'))
            return level;
    }
    return "unknown";
}

QString DxfLogViewerDialog::extractTimestamp(
    const QString& line, const QString& level) const
{
    const int levelPosition = line.indexOf('[' + level + ']');
    return levelPosition > 0 ? line.left(levelPosition).trimmed() : QString();
}

QString DxfLogViewerDialog::extractMessage(
    const QString& line, const QString& level) const
{
    const QString marker = '[' + level + ']';
    const int position = line.indexOf(marker);
    return position >= 0 ? line.mid(position + marker.size()).trimmed() : line;
}

bool DxfLogViewerDialog::matchesEntityFilter(const QString& line) const
{
    const QString entityId = m_entityFilterEdit->text().trimmed();
    if (entityId.isEmpty())
        return true;

    const QString escaped = QRegularExpression::escape(entityId);
    const QRegularExpression expression(
        QString("(?:^|\\s)(?:id|parent_id)=%1(?=\\D|$)").arg(escaped));
    return expression.match(line).hasMatch();
}

bool DxfLogViewerDialog::acceptsLine(
    const QString& line, const QString& level) const
{
    if (!matchesEntityFilter(line))
        return false;

    const FilterMode mode = static_cast<FilterMode>(
        m_levelCombo->currentData().toInt());
    switch (mode)
    {
    case SummaryWithDetails:
        return level == "info" || level == "warning" ||
               level == "error" || level == "critical" ||
               line.contains("curve_segment ");
    case AllLevels:  return true;
    case TraceOnly:  return level == "trace";
    case DebugOnly:  return level == "debug";
    case InfoOnly:   return level == "info";
    case WarningOnly:return level == "warning";
    case ErrorOnly:  return level == "error" || level == "critical";
    }
    return true;
}

void DxfLogViewerDialog::loadSelectedLog()
{
    m_logTree->clear();
    m_entityItems.clear();

    const QString path = m_logFileCombo->currentData(kPathRole).toString();
    if (path.isEmpty())
    {
        m_statusLabel->setText(tr("No log files found. Import a DXF first."));
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_statusLabel->setText(tr("Cannot open: %1").arg(path));
        return;
    }

    QTextStream stream(&file);
    while (!stream.atEnd())
    {
        const QString line = stream.readLine();
        const QString level = detectLevel(line);
        if (acceptsLine(line, level))
            addLogLine(line);
    }

    m_logTree->collapseAll();
    m_statusLabel->setText(
        tr("%1 top-level entries - %2")
            .arg(m_logTree->topLevelItemCount())
            .arg(QDir::toNativeSeparators(path)));
}

QTreeWidgetItem* DxfLogViewerDialog::findOrCreateEntityItem(
    const QString& type, const QString& entityId, const QString& level)
{
    const QString key = entityKey(type, entityId);
    QTreeWidgetItem* item = m_entityItems.value(key, nullptr);
    if (item)
        return item;

    item = new QTreeWidgetItem(m_logTree);
    item->setText(1, level);
    item->setText(2, type);
    item->setText(3, entityId);
    item->setText(4, tr("Generated curve details"));
    styleItem(item, level, true);
    m_entityItems.insert(key, item);
    return item;
}

void DxfLogViewerDialog::addLogLine(const QString& line)
{
    const QString level = detectLevel(line);
    const QString timestamp = extractTimestamp(line, level);
    const QString message = extractMessage(line, level);

    const QString curveType = captured(
        message, "curve_segment\\s+type=(\\w+)");
    const QString parentId = captured(message, "parent_id=(\\d+)");
    if (!curveType.isEmpty() && !parentId.isEmpty() &&
        message.contains("curve_segment ") )
    {
        QTreeWidgetItem* parent =
            findOrCreateEntityItem(curveType, parentId, "info");
        QTreeWidgetItem* item = new QTreeWidgetItem(parent);
        item->setText(0, timestamp);
        item->setText(1, level);
        item->setText(2, tr("SEGMENT"));
        item->setText(3, captured(message, "segment_index=(\\d+)"));
        item->setText(4, message);
        item->setToolTip(4, line);
        styleItem(item, level);
        return;
    }

    if (message.contains("converted_curve ") && !parentId.isEmpty())
    {
        const QString type = captured(message, "type=(\\w+)");
        QTreeWidgetItem* parent = findOrCreateEntityItem(type, parentId, level);
        QTreeWidgetItem* item = new QTreeWidgetItem(parent);
        item->setText(0, timestamp);
        item->setText(1, level);
        item->setText(2, tr("SUMMARY"));
        item->setText(3, parentId);
        item->setText(4, message);
        item->setToolTip(4, line);
        styleItem(item, level);
        return;
    }

    const QRegularExpression rawEntityExpression(
        "raw\\s+(POINT|LINE|CIRCLE|ARC|LWPOLYLINE|ELLIPSE)\\s+id=(\\d+)");
    const QRegularExpressionMatch rawMatch = rawEntityExpression.match(message);
    if (rawMatch.hasMatch())
    {
        const QString type = rawMatch.captured(1);
        const QString id = rawMatch.captured(2);
        const bool isPolylineDetail =
            message.contains(" VERTEX ") || message.contains(" SEGMENT ");
        if (isPolylineDetail)
        {
            QTreeWidgetItem* parent = findOrCreateEntityItem(type, id, level);
            QTreeWidgetItem* item = new QTreeWidgetItem(parent);
            item->setText(0, timestamp);
            item->setText(1, level);
            item->setText(2, message.contains(" VERTEX ") ? "VERTEX" : "SEGMENT");
            item->setText(3, captured(message, "index=(\\d+)"));
            item->setText(4, message);
            item->setToolTip(4, line);
            styleItem(item, level);
            return;
        }

        QTreeWidgetItem* item = findOrCreateEntityItem(type, id, level);
        item->setText(0, timestamp);
        item->setText(1, level);
        item->setText(2, type);
        item->setText(3, id);
        item->setText(4, message);
        item->setToolTip(4, line);
        styleItem(item, level, true);
        return;
    }

    QTreeWidgetItem* item = new QTreeWidgetItem(m_logTree);
    item->setText(0, timestamp);
    item->setText(1, level);
    item->setText(2, message.contains("_failed") ? tr("ERROR") : tr("EVENT"));
    item->setText(4, message);
    item->setToolTip(4, line);
    styleItem(item, level);
}

void DxfLogViewerDialog::styleItem(
    QTreeWidgetItem* item, const QString& level, bool isParent)
{
    QColor color(70, 70, 70);
    if (level == "trace")         color = QColor(125, 125, 125);
    else if (level == "debug")    color = QColor(0, 140, 170);
    else if (level == "info")     color = QColor(0, 135, 75);
    else if (level == "warning")  color = QColor(210, 135, 0);
    else if (level == "error")    color = QColor(210, 45, 45);
    else if (level == "critical") color = QColor(160, 35, 160);

    QFont font = item->font(0);
    font.setBold(isParent || level == "error" || level == "critical");
    for (int column = 0; column < m_logTree->columnCount(); ++column)
    {
        item->setForeground(column, QBrush(color));
        item->setFont(column, font);
    }
}

void DxfLogViewerDialog::expandAllItems()
{
    m_logTree->expandAll();
}

void DxfLogViewerDialog::collapseAllItems()
{
    m_logTree->collapseAll();
}

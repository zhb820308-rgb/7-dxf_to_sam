#ifndef DebugLog_h
#define DebugLog_h

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QString>

// 文件日志辅助函数（立即刷写，防止闪退丢日志）
inline void writeLog(const QString& msg)
{
    static QFile logFile;
    static bool inited = false;
    if (!inited)
    {
        QString path = QDir::homePath() + "/Example1_spline_debug.log";
        logFile.setFileName(path);
        logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        inited = true;
        QTextStream(&logFile) << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
                              << " ===== LOG START =====\n";
    }
    QTextStream ts(&logFile);
    ts << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
       << " " << msg << "\n";
    ts.flush();
    logFile.flush();
}

#endif // DebugLog_h
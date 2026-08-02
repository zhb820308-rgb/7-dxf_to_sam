#ifndef ImportBuildResult_h
#define ImportBuildResult_h

#include <QString>

enum class ImportBuildStatus
{
    Success,
    Canceled,
    BeginFailed,
    CreateFailed,
    CommitFailed,
    RollbackFailed
};

struct ImportBuildResult
{
    ImportBuildStatus status = ImportBuildStatus::Success;
    int createdCount = 0;
    QString message;
    bool rollbackAttempted = false;
    bool rollbackSucceeded = false;
    QString rollbackMessage;

    bool succeeded() const { return status == ImportBuildStatus::Success; }

    static ImportBuildResult success(int created = 0)
    {
        return {
            ImportBuildStatus::Success,
            created,
            QString(),
            false,
            false,
            QString()};
    }

    static ImportBuildResult failure(ImportBuildStatus failureStatus,
                                     const QString& error,
                                     int created = 0)
    {
        return {
            failureStatus,
            created,
            error,
            false,
            false,
            QString()};
    }

    void recordRollback(const ImportBuildResult& cleanup)
    {
        rollbackAttempted = true;
        rollbackSucceeded = cleanup.succeeded();
        rollbackMessage = cleanup.message;
    }
};

#endif

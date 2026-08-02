#pragma once

#include "DxfImportError.h"
#include "ImportBuildResult.h"

#include <QString>

enum class DxfImportOutcomeStatus
{
    Succeeded,
    Canceled,
    ValidationFailed,
    ParseFailed,
    ConvertFailed,
    BeginFailed,
    CreateFailed,
    CommitFailed,
    RollbackFailed
};

enum class DxfImportRollbackStatus
{
    NotAttempted,
    Succeeded,
    Failed
};

struct DxfImportOutcome
{
    DxfImportOutcomeStatus status = DxfImportOutcomeStatus::ValidationFailed;
    DxfImportErrorCode errorCode = DxfImportErrorCode::None;
    QString stage;
    QString message;
    int createdCount = 0;
    DxfImportRollbackStatus rollbackStatus =
        DxfImportRollbackStatus::NotAttempted;
    QString rollbackMessage;

    bool succeeded() const
    {
        return status == DxfImportOutcomeStatus::Succeeded;
    }

    bool canceled() const
    {
        return status == DxfImportOutcomeStatus::Canceled;
    }

    static DxfImportOutcome success(int createdCount);

    static DxfImportOutcome failure(
        DxfImportOutcomeStatus status,
        DxfImportErrorCode errorCode,
        const QString& stage,
        const QString& message,
        int createdCount = 0);

    static DxfImportOutcome fromBuildFailure(
        const ImportBuildResult& buildResult,
        const QString& stage,
        const QString& message);
};

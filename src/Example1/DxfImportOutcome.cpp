#include "DxfImportOutcome.h"

namespace {

DxfImportOutcomeStatus outcomeStatus(ImportBuildStatus status)
{
    switch (status)
    {
    case ImportBuildStatus::Canceled:
        return DxfImportOutcomeStatus::Canceled;
    case ImportBuildStatus::BeginFailed:
        return DxfImportOutcomeStatus::BeginFailed;
    case ImportBuildStatus::CreateFailed:
        return DxfImportOutcomeStatus::CreateFailed;
    case ImportBuildStatus::CommitFailed:
        return DxfImportOutcomeStatus::CommitFailed;
    case ImportBuildStatus::RollbackFailed:
        return DxfImportOutcomeStatus::RollbackFailed;
    case ImportBuildStatus::Success:
        break;
    }
    return DxfImportOutcomeStatus::Succeeded;
}

DxfImportErrorCode outcomeErrorCode(ImportBuildStatus status)
{
    switch (status)
    {
    case ImportBuildStatus::Canceled:
        return DxfImportErrorCode::Canceled;
    case ImportBuildStatus::BeginFailed:
        return DxfImportErrorCode::BeginFailed;
    case ImportBuildStatus::CreateFailed:
        return DxfImportErrorCode::CreateFailed;
    case ImportBuildStatus::CommitFailed:
        return DxfImportErrorCode::CommitFailed;
    case ImportBuildStatus::RollbackFailed:
        return DxfImportErrorCode::RollbackFailed;
    case ImportBuildStatus::Success:
        break;
    }
    return DxfImportErrorCode::None;
}

} // namespace

DxfImportOutcome DxfImportOutcome::success(int created)
{
    DxfImportOutcome result;
    result.status = DxfImportOutcomeStatus::Succeeded;
    result.createdCount = created;
    return result;
}

DxfImportOutcome DxfImportOutcome::failure(
    DxfImportOutcomeStatus failureStatus,
    DxfImportErrorCode failureCode,
    const QString& failureStage,
    const QString& failureMessage,
    int created)
{
    DxfImportOutcome result;
    result.status = failureStatus;
    result.errorCode = failureCode;
    result.stage = failureStage;
    result.message = failureMessage;
    result.createdCount = created;
    return result;
}

DxfImportOutcome DxfImportOutcome::fromBuildFailure(
    const ImportBuildResult& buildResult,
    const QString& failureStage,
    const QString& failureMessage)
{
    DxfImportOutcome result = failure(
        outcomeStatus(buildResult.status),
        outcomeErrorCode(buildResult.status),
        failureStage,
        failureMessage,
        buildResult.createdCount);
    if (buildResult.rollbackAttempted)
    {
        result.rollbackStatus = buildResult.rollbackSucceeded
            ? DxfImportRollbackStatus::Succeeded
            : DxfImportRollbackStatus::Failed;
        result.rollbackMessage = buildResult.rollbackMessage;
    }
    else if (buildResult.status == ImportBuildStatus::RollbackFailed)
    {
        result.rollbackStatus = DxfImportRollbackStatus::Failed;
        result.rollbackMessage = buildResult.message;
    }
    return result;
}

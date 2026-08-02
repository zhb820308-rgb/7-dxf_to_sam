#include "ImportTransaction.h"
#include "ImportBuildResult.h"
#include "DxfImportOutcome.h"

#include <gtest/gtest.h>

TEST(ImportTransaction, CompletesValidLifecycle)
{
    ImportTransaction transaction;
    EXPECT_TRUE(transaction.begin());
    EXPECT_TRUE(transaction.markResourceOwned());
    EXPECT_TRUE(transaction.startWriting());
    EXPECT_TRUE(transaction.startCommitting());
    EXPECT_TRUE(transaction.markCommitted());
    EXPECT_EQ(transaction.state(), ImportTransactionState::Committed);
    EXPECT_FALSE(transaction.ownsResource());
}

TEST(ImportTransaction, RollbackFromWritingIsIdempotent)
{
    ImportTransaction transaction;
    ASSERT_TRUE(transaction.begin());
    ASSERT_TRUE(transaction.markResourceOwned());
    ASSERT_TRUE(transaction.startWriting());

    int cleanupCount = 0;
    EXPECT_TRUE(transaction.rollback([&cleanupCount]() {
        ++cleanupCount;
        return true;
    }));
    EXPECT_TRUE(transaction.rollback([&cleanupCount]() {
        ++cleanupCount;
        return true;
    }));
    EXPECT_EQ(cleanupCount, 1);
    EXPECT_EQ(transaction.state(), ImportTransactionState::RolledBack);
}

TEST(ImportTransaction, RollbackFromCommittingCompensatesOnce)
{
    ImportTransaction transaction;
    ASSERT_TRUE(transaction.begin());
    ASSERT_TRUE(transaction.markResourceOwned());
    ASSERT_TRUE(transaction.startWriting());
    ASSERT_TRUE(transaction.startCommitting());

    int cleanupCount = 0;
    EXPECT_TRUE(transaction.rollback([&cleanupCount]() {
        ++cleanupCount;
        return true;
    }));
    EXPECT_EQ(cleanupCount, 1);
    EXPECT_EQ(transaction.state(), ImportTransactionState::RolledBack);
}

TEST(ImportTransaction, FailedCleanupCanBeRetried)
{
    ImportTransaction transaction;
    ASSERT_TRUE(transaction.begin());
    ASSERT_TRUE(transaction.markResourceOwned());
    ASSERT_TRUE(transaction.startWriting());

    int cleanupCount = 0;
    EXPECT_FALSE(transaction.rollback([&cleanupCount]() {
        return ++cleanupCount > 1;
    }));
    EXPECT_EQ(transaction.state(), ImportTransactionState::RollingBack);
    EXPECT_TRUE(transaction.ownsResource());
    EXPECT_TRUE(transaction.rollback([&cleanupCount]() {
        return ++cleanupCount > 1;
    }));
    EXPECT_EQ(cleanupCount, 2);
    EXPECT_EQ(transaction.state(), ImportTransactionState::RolledBack);
}

TEST(ImportTransaction, CommittedResourceCannotBeRolledBack)
{
    ImportTransaction transaction;
    ASSERT_TRUE(transaction.begin());
    ASSERT_TRUE(transaction.markResourceOwned());
    ASSERT_TRUE(transaction.startWriting());
    ASSERT_TRUE(transaction.startCommitting());
    ASSERT_TRUE(transaction.markCommitted());

    int cleanupCount = 0;
    EXPECT_FALSE(transaction.rollback([&cleanupCount]() {
        ++cleanupCount;
        return true;
    }));
    EXPECT_EQ(cleanupCount, 0);
    EXPECT_EQ(transaction.state(), ImportTransactionState::Committed);
}

TEST(ImportTransaction, RejectsInvalidTransitions)
{
    ImportTransaction transaction;
    EXPECT_FALSE(transaction.startWriting());
    ASSERT_TRUE(transaction.begin());
    EXPECT_FALSE(transaction.startWriting());
    EXPECT_FALSE(transaction.startCommitting());
    EXPECT_FALSE(transaction.markCommitted());
}

TEST(ImportBuildResult, SuccessCarriesCreatedCount)
{
    const ImportBuildResult result = ImportBuildResult::success(42);
    EXPECT_TRUE(result.succeeded());
    EXPECT_EQ(result.status, ImportBuildStatus::Success);
    EXPECT_EQ(result.createdCount, 42);
    EXPECT_TRUE(result.message.isEmpty());
}

TEST(ImportBuildResult, StatusDoesNotDependOnMessageText)
{
    const ImportBuildResult canceled = ImportBuildResult::failure(
        ImportBuildStatus::Canceled, QStringLiteral("localized cancellation"), 7);
    const ImportBuildResult createFailed = ImportBuildResult::failure(
        ImportBuildStatus::CreateFailed,
        QStringLiteral("import canceled by user"), 3);

    EXPECT_FALSE(canceled.succeeded());
    EXPECT_EQ(canceled.status, ImportBuildStatus::Canceled);
    EXPECT_EQ(canceled.createdCount, 7);
    EXPECT_EQ(createFailed.status, ImportBuildStatus::CreateFailed);
    EXPECT_EQ(createFailed.createdCount, 3);
}

TEST(ImportBuildResult, RecordsRollbackWithoutOverwritingPrimaryFailure)
{
    ImportBuildResult primary = ImportBuildResult::failure(
        ImportBuildStatus::CreateFailed,
        QStringLiteral("node creation failed"),
        3);
    const ImportBuildResult cleanup = ImportBuildResult::failure(
        ImportBuildStatus::RollbackFailed,
        QStringLiteral("part still exists"),
        3);

    primary.recordRollback(cleanup);

    EXPECT_EQ(primary.status, ImportBuildStatus::CreateFailed);
    EXPECT_EQ(primary.message, QStringLiteral("node creation failed"));
    EXPECT_TRUE(primary.rollbackAttempted);
    EXPECT_FALSE(primary.rollbackSucceeded);
    EXPECT_EQ(primary.rollbackMessage, QStringLiteral("part still exists"));
}

TEST(DxfImportOutcome, MapsEveryBuildFailureToStableApplicationStatus)
{
    struct Case
    {
        ImportBuildStatus buildStatus;
        DxfImportOutcomeStatus outcomeStatus;
        DxfImportErrorCode errorCode;
    };
    const Case cases[] = {
        {ImportBuildStatus::Canceled,
         DxfImportOutcomeStatus::Canceled,
         DxfImportErrorCode::Canceled},
        {ImportBuildStatus::BeginFailed,
         DxfImportOutcomeStatus::BeginFailed,
         DxfImportErrorCode::BeginFailed},
        {ImportBuildStatus::CreateFailed,
         DxfImportOutcomeStatus::CreateFailed,
         DxfImportErrorCode::CreateFailed},
        {ImportBuildStatus::CommitFailed,
         DxfImportOutcomeStatus::CommitFailed,
         DxfImportErrorCode::CommitFailed},
        {ImportBuildStatus::RollbackFailed,
         DxfImportOutcomeStatus::RollbackFailed,
         DxfImportErrorCode::RollbackFailed}
    };

    for (const Case& item : cases)
    {
        const ImportBuildResult build = ImportBuildResult::failure(
            item.buildStatus, QStringLiteral("localized message"), 7);
        const DxfImportOutcome outcome =
            DxfImportOutcome::fromBuildFailure(
                build,
                QStringLiteral("build"),
                QStringLiteral("user-facing message"));
        EXPECT_EQ(outcome.status, item.outcomeStatus);
        EXPECT_EQ(outcome.errorCode, item.errorCode);
        EXPECT_EQ(outcome.createdCount, 7);
        EXPECT_EQ(outcome.stage, QStringLiteral("build"));
        EXPECT_EQ(outcome.message, QStringLiteral("user-facing message"));
    }
}

TEST(DxfImportOutcome, KeepsPrimaryAndRollbackFailuresSeparate)
{
    ImportBuildResult build = ImportBuildResult::failure(
        ImportBuildStatus::CommitFailed,
        QStringLiteral("commit failed"),
        9);
    build.recordRollback(ImportBuildResult::failure(
        ImportBuildStatus::RollbackFailed,
        QStringLiteral("cleanup failed"),
        9));

    const DxfImportOutcome outcome = DxfImportOutcome::fromBuildFailure(
        build,
        QStringLiteral("commit"),
        QStringLiteral("commit failed; rollback failed"));

    EXPECT_EQ(outcome.status, DxfImportOutcomeStatus::CommitFailed);
    EXPECT_EQ(outcome.errorCode, DxfImportErrorCode::CommitFailed);
    EXPECT_EQ(outcome.rollbackStatus, DxfImportRollbackStatus::Failed);
    EXPECT_EQ(outcome.rollbackMessage, QStringLiteral("cleanup failed"));
    EXPECT_EQ(outcome.createdCount, 9);
}

TEST(DxfImportOutcome, SuccessAndValidationFailureHaveExplicitSemantics)
{
    const DxfImportOutcome success = DxfImportOutcome::success(12);
    EXPECT_TRUE(success.succeeded());
    EXPECT_FALSE(success.canceled());
    EXPECT_EQ(success.createdCount, 12);
    EXPECT_EQ(success.errorCode, DxfImportErrorCode::None);

    const DxfImportOutcome invalid = DxfImportOutcome::failure(
        DxfImportOutcomeStatus::ValidationFailed,
        DxfImportErrorCode::InvalidArgument,
        QStringLiteral("validate_params"),
        QStringLiteral("invalid curve tolerance"));
    EXPECT_FALSE(invalid.succeeded());
    EXPECT_FALSE(invalid.canceled());
    EXPECT_EQ(invalid.status, DxfImportOutcomeStatus::ValidationFailed);
    EXPECT_EQ(invalid.errorCode, DxfImportErrorCode::InvalidArgument);
}

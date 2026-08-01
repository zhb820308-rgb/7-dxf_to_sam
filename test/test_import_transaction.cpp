#include "ImportTransaction.h"

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

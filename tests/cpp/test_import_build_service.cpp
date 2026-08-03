#include <gtest/gtest.h>

#include <QString>

#include <string>
#include <vector>

#include "FeConversion.h"
#include "SketchConversion.h"

namespace {

enum class FailurePoint
{
    None,
    Begin,
    FirstCreate,
    SecondCreate,
    Commit
};

class FakeSamBuilder : public ISamImportBuilder
{
public:
    FailurePoint failure = FailurePoint::None;
    ImportBuildStatus createFailureStatus = ImportBuildStatus::CreateFailed;
    int partialCreated = 0;
    bool failNextRollback = false;
    std::vector<std::string> calls;

    ImportBuildResult beginImport(const QString&) override
    {
        calls.push_back("begin");
        if (failure == FailurePoint::Begin)
            return fail(ImportBuildStatus::BeginFailed, "begin failed");
        active = true;
        created = 0;
        return ImportBuildResult::success();
    }

    ImportBuildResult createLines(
        const std::vector<DxfLine>& lines) override
    {
        calls.push_back("lines");
        if (failure == FailurePoint::FirstCreate) {
            created += partialCreated;
            return fail(createFailureStatus, "line creation failed", partialCreated);
        }
        created += static_cast<int>(lines.size());
        return ImportBuildResult::success(static_cast<int>(lines.size()));
    }

    ImportBuildResult createCircles(
        const std::vector<DxfCircle>& circles) override
    {
        calls.push_back("circles");
        if (failure == FailurePoint::SecondCreate) {
            created += partialCreated;
            return fail(createFailureStatus, "circle creation failed", partialCreated);
        }
        created += static_cast<int>(circles.size());
        return ImportBuildResult::success(static_cast<int>(circles.size()));
    }

    ImportBuildResult commit() override
    {
        calls.push_back("commit");
        if (failure == FailurePoint::Commit)
            return fail(ImportBuildStatus::CommitFailed, "commit failed", created);
        active = false;
        return ImportBuildResult::success(created);
    }

    ImportBuildResult rollback() override
    {
        calls.push_back("rollback");
        if (failNextRollback) {
            failNextRollback = false;
            return fail(
                ImportBuildStatus::RollbackFailed,
                "rollback failed", created);
        }
        active = false;
        created = 0;
        return ImportBuildResult::success();
    }

    bool isActive() const { return active; }

private:
    static ImportBuildResult fail(
        ImportBuildStatus status,
        const char* message,
        int count = 0)
    {
        return ImportBuildResult::failure(
            status, QString::fromLatin1(message), count);
    }

    bool active = false;
    int created = 0;
};

class FakeFeBuilder : public IFeImportBuilder
{
public:
    FailurePoint failure = FailurePoint::None;
    ImportBuildStatus createFailureStatus = ImportBuildStatus::CreateFailed;
    int partialCreated = 0;
    bool failNextRollback = false;
    std::vector<std::string> calls;
    std::vector<QString> requestedParts;

    ImportBuildResult beginImport(
        const QString& modelName, const QString& partName) override
    {
        calls.push_back("begin");
        requestedParts.push_back(modelName + QStringLiteral("/") + partName);
        if (failure == FailurePoint::Begin)
            return fail(ImportBuildStatus::BeginFailed, "begin failed");
        active = true;
        created = 0;
        return ImportBuildResult::success();
    }

    ImportBuildResult createNodes(
        const std::vector<FeNode>& nodes) override
    {
        calls.push_back("nodes");
        if (failure == FailurePoint::FirstCreate) {
            created += partialCreated;
            return fail(createFailureStatus, "node creation failed", partialCreated);
        }
        created += static_cast<int>(nodes.size());
        return ImportBuildResult::success(static_cast<int>(nodes.size()));
    }

    ImportBuildResult createTrusses(
        const std::vector<FeTruss>& trusses) override
    {
        calls.push_back("trusses");
        if (failure == FailurePoint::SecondCreate) {
            created += partialCreated;
            return fail(createFailureStatus, "truss creation failed", partialCreated);
        }
        created += static_cast<int>(trusses.size());
        return ImportBuildResult::success(static_cast<int>(trusses.size()));
    }

    ImportBuildResult commit() override
    {
        calls.push_back("commit");
        if (failure == FailurePoint::Commit)
            return fail(ImportBuildStatus::CommitFailed, "commit failed", created);
        active = false;
        return ImportBuildResult::success(created);
    }

    ImportBuildResult rollback() override
    {
        calls.push_back("rollback");
        if (failNextRollback) {
            failNextRollback = false;
            return fail(
                ImportBuildStatus::RollbackFailed,
                "rollback failed", created);
        }
        active = false;
        created = 0;
        return ImportBuildResult::success();
    }

    bool isActive() const { return active; }

private:
    static ImportBuildResult fail(
        ImportBuildStatus status,
        const char* message,
        int count = 0)
    {
        return ImportBuildResult::failure(
            status, QString::fromLatin1(message), count);
    }

    bool active = false;
    int created = 0;
};

SamData sampleSamData()
{
    SamData data;
    data.addLine(DxfLine(
        DxfPoint(0.0, 0.0, 0.0), DxfPoint(1.0, 0.0, 0.0)));
    data.addLine(DxfLine(
        DxfPoint(1.0, 0.0, 0.0), DxfPoint(2.0, 0.0, 0.0)));
    data.addLine(DxfLine(
        DxfPoint(2.0, 0.0, 0.0), DxfPoint(3.0, 0.0, 0.0)));
    data.addCircle(DxfCircle(DxfPoint(10.0, 10.0, 0.0), 1.0));
    data.addCircle(DxfCircle(DxfPoint(20.0, 20.0, 0.0), 2.0));
    return data;
}

FeData sampleFeData()
{
    FeData data;
    data.addOrGetNode(0.0, 0.0, 0.0, 0.0);
    data.addOrGetNode(1.0, 0.0, 0.0, 0.0);
    data.addOrGetNode(2.0, 0.0, 0.0, 0.0);
    data.addTruss(0, 1);
    data.addTruss(1, 2);
    return data;
}

} // namespace

TEST(ImportBuildService, BeginFailureStopsBeforeCreationAndRollback)
{
    FakeSamBuilder builder;
    builder.failure = FailurePoint::Begin;

    const ImportBuildResult result =
        DxfImportBuildService::buildSamSketch(sampleSamData(), builder);

    EXPECT_EQ(result.status, ImportBuildStatus::BeginFailed);
    EXPECT_EQ(result.createdCount, 0);
    EXPECT_EQ(builder.calls, std::vector<std::string>({"begin"}));
    EXPECT_FALSE(builder.isActive());
}

TEST(ImportBuildService, LineFailureStopsLaterStagesAndRollsBackPartialWork)
{
    FakeSamBuilder builder;
    builder.failure = FailurePoint::FirstCreate;
    builder.partialCreated = 2;

    const ImportBuildResult result =
        DxfImportBuildService::buildSamSketch(sampleSamData(), builder);

    EXPECT_EQ(result.status, ImportBuildStatus::CreateFailed);
    EXPECT_EQ(result.createdCount, 2);
    EXPECT_TRUE(result.rollbackAttempted);
    EXPECT_TRUE(result.rollbackSucceeded);
    EXPECT_TRUE(result.rollbackMessage.isEmpty());
    EXPECT_EQ(
        builder.calls,
        std::vector<std::string>({"begin", "lines", "rollback"}));
    EXPECT_FALSE(builder.isActive());
}

TEST(ImportBuildService, CancellationDuringCirclesSkipsCommitAndRollsBack)
{
    FakeSamBuilder builder;
    builder.failure = FailurePoint::SecondCreate;
    builder.createFailureStatus = ImportBuildStatus::Canceled;
    builder.partialCreated = 1;

    const ImportBuildResult result =
        DxfImportBuildService::buildSamSketch(sampleSamData(), builder);

    EXPECT_EQ(result.status, ImportBuildStatus::Canceled);
    EXPECT_EQ(result.createdCount, 4);
    EXPECT_TRUE(result.rollbackAttempted);
    EXPECT_TRUE(result.rollbackSucceeded);
    EXPECT_EQ(
        builder.calls,
        std::vector<std::string>(
            {"begin", "lines", "circles", "rollback"}));
    EXPECT_FALSE(builder.isActive());
}

TEST(ImportBuildService, CommitFailureIsCompensatedWithCreatedCount)
{
    FakeSamBuilder builder;
    builder.failure = FailurePoint::Commit;

    const ImportBuildResult result =
        DxfImportBuildService::buildSamSketch(sampleSamData(), builder);

    EXPECT_EQ(result.status, ImportBuildStatus::CommitFailed);
    EXPECT_EQ(result.createdCount, 5);
    EXPECT_TRUE(result.rollbackAttempted);
    EXPECT_TRUE(result.rollbackSucceeded);
    EXPECT_EQ(
        builder.calls,
        std::vector<std::string>(
            {"begin", "lines", "circles", "commit", "rollback"}));
    EXPECT_FALSE(builder.isActive());
}

TEST(ImportBuildService, RollbackFailureKeepsBothErrorsAndAllowsCleanupRetry)
{
    FakeSamBuilder builder;
    builder.failure = FailurePoint::FirstCreate;
    builder.partialCreated = 1;
    builder.failNextRollback = true;

    const ImportBuildResult result =
        DxfImportBuildService::buildSamSketch(sampleSamData(), builder);

    EXPECT_EQ(result.status, ImportBuildStatus::CreateFailed);
    EXPECT_EQ(result.createdCount, 1);
    EXPECT_TRUE(result.message.contains(QStringLiteral("line creation failed")));
    EXPECT_FALSE(result.message.contains(QStringLiteral("rollback failed")));
    EXPECT_TRUE(result.rollbackAttempted);
    EXPECT_FALSE(result.rollbackSucceeded);
    EXPECT_TRUE(result.rollbackMessage.contains(
        QStringLiteral("rollback failed")));
    EXPECT_TRUE(builder.isActive());

    EXPECT_TRUE(builder.rollback().succeeded());
    EXPECT_FALSE(builder.isActive());
    builder.failure = FailurePoint::None;

    const ImportBuildResult retry =
        DxfImportBuildService::buildSamSketch(sampleSamData(), builder);
    EXPECT_TRUE(retry.succeeded());
    EXPECT_EQ(retry.createdCount, 5);
}

TEST(ImportBuildService, FeNodeFailureStopsTrussesAndRollsBack)
{
    FakeFeBuilder builder;
    builder.failure = FailurePoint::FirstCreate;
    builder.partialCreated = 2;
    FeData data = sampleFeData();

    const ImportBuildResult result = DxfImportBuildService::buildFePart(
        data, QStringLiteral("Model-1"), QStringLiteral("Part-1"), builder);

    EXPECT_EQ(result.status, ImportBuildStatus::CreateFailed);
    EXPECT_EQ(result.createdCount, 2);
    EXPECT_TRUE(result.rollbackAttempted);
    EXPECT_TRUE(result.rollbackSucceeded);
    EXPECT_EQ(
        builder.calls,
        std::vector<std::string>({"begin", "nodes", "rollback"}));
    EXPECT_FALSE(builder.isActive());
}

TEST(ImportBuildService, FeTrussCancellationRollsBackAndSameNameCanRetry)
{
    FakeFeBuilder builder;
    builder.failure = FailurePoint::SecondCreate;
    builder.createFailureStatus = ImportBuildStatus::Canceled;
    builder.partialCreated = 1;
    FeData data = sampleFeData();

    const ImportBuildResult canceled = DxfImportBuildService::buildFePart(
        data, QStringLiteral("Model-1"), QStringLiteral("Part-1"), builder);
    EXPECT_EQ(canceled.status, ImportBuildStatus::Canceled);
    EXPECT_EQ(canceled.createdCount, 4);
    EXPECT_TRUE(canceled.rollbackAttempted);
    EXPECT_TRUE(canceled.rollbackSucceeded);
    EXPECT_FALSE(builder.isActive());

    builder.failure = FailurePoint::None;
    const ImportBuildResult retry = DxfImportBuildService::buildFePart(
        data, QStringLiteral("Model-1"), QStringLiteral("Part-1"), builder);
    EXPECT_TRUE(retry.succeeded());
    EXPECT_EQ(retry.createdCount, 5);
    ASSERT_EQ(builder.requestedParts.size(), 2u);
    EXPECT_EQ(builder.requestedParts[0], builder.requestedParts[1]);
}

TEST(ImportBuildService, FeCommitFailureIsCompensated)
{
    FakeFeBuilder builder;
    builder.failure = FailurePoint::Commit;
    FeData data = sampleFeData();

    const ImportBuildResult result = DxfImportBuildService::buildFePart(
        data, QStringLiteral("Model-1"), QStringLiteral("Part-1"), builder);

    EXPECT_EQ(result.status, ImportBuildStatus::CommitFailed);
    EXPECT_EQ(result.createdCount, 5);
    EXPECT_TRUE(result.rollbackAttempted);
    EXPECT_TRUE(result.rollbackSucceeded);
    EXPECT_EQ(
        builder.calls,
        std::vector<std::string>(
            {"begin", "nodes", "trusses", "commit", "rollback"}));
    EXPECT_FALSE(builder.isActive());
}

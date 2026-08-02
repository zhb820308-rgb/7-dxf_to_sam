#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryFile>
#include <QTextStream>

#include <limits>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "DxfBlockExpansion.h"
#include "DxfParser.h"

#ifndef DXF_TEST_FIXTURE_DIR
#error "DXF_TEST_FIXTURE_DIR must be provided by CMake"
#endif

namespace {

constexpr int kDocumentedMaxExpandDepth = 32;
constexpr double kCoordinateTolerance = 1e-8;

using BlockMap = std::unordered_map<std::string, DxfBlock>;

DxfLine unitLine(double y = 0.0)
{
    return DxfLine(DxfPoint(0.0, y, 0.0), DxfPoint(1.0, y, 0.0));
}

DxfBlock lineBlock(const std::string& name, double y = 0.0)
{
    DxfBlock block;
    block.setName(name);
    block.addLine(unitLine(y));
    return block;
}

InsertInfo insertOf(
    const std::string& blockName,
    double x = 0.0,
    int columns = 1)
{
    InsertInfo insert;
    insert.blockName = blockName;
    insert.insertX = x;
    insert.colCount = columns;
    return insert;
}

bool expand(
    DxfData& output,
    const BlockMap& blocks,
    const std::vector<InsertInfo>& inserts,
    std::size_t maxOutputEntities,
    QString& error)
{
    return expandDxfBlocks(
        output, blocks, inserts, {}, 0.01, 0,
        maxOutputEntities, error);
}

bool writeBlockChain(
    QTemporaryFile& file,
    int deepestDepth,
    bool lineAtEveryDepth)
{
    if (!file.open()) return false;
    QTextStream stream(&file);
    stream << "0\nSECTION\n2\nHEADER\n0\nENDSEC\n"
              "0\nSECTION\n2\nBLOCKS\n";

    for (int depth = 0; depth <= deepestDepth; ++depth) {
        stream << "0\nBLOCK\n8\n0\n2\nDEPTH_" << depth
               << "\n70\n0\n10\n0\n20\n0\n30\n0\n";
        if (lineAtEveryDepth || depth == deepestDepth) {
            stream << "0\nLINE\n8\n0\n10\n0\n20\n" << depth
                   << "\n30\n0\n11\n1\n21\n" << depth
                   << "\n31\n0\n";
        }
        if (depth < deepestDepth) {
            stream << "0\nINSERT\n8\n0\n2\nDEPTH_" << (depth + 1)
                   << "\n10\n1\n20\n0\n30\n0\n"
                      "41\n1\n42\n1\n43\n1\n70\n1\n71\n1\n";
        }
        stream << "0\nENDBLK\n8\n0\n";
    }

    stream << "0\nENDSEC\n0\nSECTION\n2\nENTITIES\n"
              "0\nINSERT\n8\n0\n2\nDEPTH_0\n10\n0\n20\n0\n30\n0\n"
              "41\n1\n42\n1\n43\n1\n70\n1\n71\n1\n"
              "0\nENDSEC\n0\nEOF\n";
    stream.flush();
    const bool written = stream.status() == QTextStream::Ok;
    file.close();
    return written;
}

} // namespace

TEST(DxfBlockExpansion, MutualCycleStopsAtFirstRepeatedBlock)
{
    DxfBlock blockA = lineBlock("A", 0.0);
    blockA.addInsert(insertOf("B", 10.0));

    DxfBlock blockB = lineBlock("B", 1.0);
    blockB.addInsert(insertOf("A", 100.0));

    BlockMap blocks;
    blocks.emplace("A", blockA);
    blocks.emplace("B", blockB);

    DxfData output;
    QString error;
    ASSERT_TRUE(expand(
        output, blocks, {insertOf("A", 1.0)}, 100, error));
    EXPECT_TRUE(error.isEmpty());
    ASSERT_EQ(output.lines().size(), 2u);
    EXPECT_NEAR(output.lines()[0].start().x(), 1.0, kCoordinateTolerance);
    EXPECT_NEAR(output.lines()[0].end().x(), 2.0, kCoordinateTolerance);
    EXPECT_NEAR(output.lines()[1].start().x(), 11.0, kCoordinateTolerance);
    EXPECT_NEAR(output.lines()[1].end().x(), 12.0, kCoordinateTolerance);
}

TEST(DxfBlockExpansion, UnknownReferencesAreSkippedWithoutExpandingScope)
{
    DxfBlock known = lineBlock("KNOWN");
    known.addInsert(insertOf("MISSING_NESTED", 50.0));

    BlockMap blocks;
    blocks.emplace("KNOWN", known);

    DxfData output;
    QString error;
    ASSERT_TRUE(expand(
        output, blocks,
        {insertOf("MISSING_TOP"), insertOf("KNOWN", 3.0)},
        100, error));
    EXPECT_TRUE(error.isEmpty());
    ASSERT_EQ(output.lines().size(), 1u);
    EXPECT_NEAR(output.lines()[0].start().x(), 3.0, kCoordinateTolerance);
    EXPECT_NEAR(output.lines()[0].end().x(), 4.0, kCoordinateTolerance);
}

TEST(DxfBlockExpansion, NestedArraysShareTheGlobalInstanceBudget)
{
    DxfBlock branch;
    branch.setName("BRANCH");
    branch.addInsert(insertOf("LEAF", 0.0, 2));

    BlockMap blocks;
    blocks.emplace("BRANCH", branch);
    blocks.emplace("LEAF", lineBlock("LEAF"));

    DxfData output;
    QString error;
    EXPECT_FALSE(expand(
        output, blocks, {insertOf("BRANCH", 0.0, 99999)},
        std::numeric_limits<std::size_t>::max(), error));
    EXPECT_EQ(output.entityCount(), 0u);
    EXPECT_TRUE(error.contains("block instances", Qt::CaseInsensitive))
        << error.toStdString();
}

TEST(DxfBlockExpansion, NestedBlocksShareTheOutputEntityBudget)
{
    DxfBlock branch = lineBlock("BRANCH", 0.0);
    branch.addInsert(insertOf("LEAF"));

    DxfBlock leaf = lineBlock("LEAF", 1.0);
    leaf.addLine(unitLine(2.0));

    BlockMap blocks;
    blocks.emplace("BRANCH", branch);
    blocks.emplace("LEAF", leaf);

    DxfData exactOutput;
    QString exactError;
    ASSERT_TRUE(expand(
        exactOutput, blocks, {insertOf("BRANCH")}, 3, exactError));
    EXPECT_TRUE(exactError.isEmpty());
    EXPECT_EQ(exactOutput.lines().size(), 3u);

    DxfData limitedOutput;
    QString limitedError;
    EXPECT_FALSE(expand(
        limitedOutput, blocks, {insertOf("BRANCH")}, 2, limitedError));
    EXPECT_EQ(limitedOutput.lines().size(), 2u);
    EXPECT_TRUE(limitedError.contains("output entities", Qt::CaseInsensitive))
        << limitedError.toStdString();
}

TEST(DxfBlockExpansion, DocumentedDepthBoundaryProducesExactCoordinates)
{
    QTemporaryFile file(QDir::tempPath() + "/dxf-depth-ok-XXXXXX.dxf");
    ASSERT_TRUE(writeBlockChain(file, kDocumentedMaxExpandDepth, false));

    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(file.fileName(), data))
        << data.errorMessage().toStdString();
    ASSERT_EQ(data.lines().size(), 1u);
    EXPECT_NEAR(
        data.lines()[0].start().x(),
        static_cast<double>(kDocumentedMaxExpandDepth),
        kCoordinateTolerance);
    EXPECT_NEAR(
        data.lines()[0].end().x(),
        static_cast<double>(kDocumentedMaxExpandDepth + 1),
        kCoordinateTolerance);
    EXPECT_NEAR(
        data.lines()[0].start().y(),
        static_cast<double>(kDocumentedMaxExpandDepth),
        kCoordinateTolerance);
}

TEST(DxfBlockExpansion, DepthFailureClearsPartialOutputAndParserCanBeReused)
{
    QTemporaryFile file(QDir::tempPath() + "/dxf-depth-fail-XXXXXX.dxf");
    ASSERT_TRUE(writeBlockChain(
        file, kDocumentedMaxExpandDepth + 1, true));

    DxfData data;
    DxfParser parser;
    EXPECT_FALSE(parser.parseFile(file.fileName(), data));
    EXPECT_FALSE(data.isValid());
    EXPECT_EQ(data.errorCode(), DxfImportErrorCode::ExpansionLimit);
    EXPECT_EQ(data.entityCount(), 0u);
    EXPECT_EQ(data.entityStats().generatedEntities, 0u);
    EXPECT_TRUE(data.errorMessage().contains("depth", Qt::CaseInsensitive))
        << data.errorMessage().toStdString();

    const QString fixtureDir = QString::fromUtf8(DXF_TEST_FIXTURE_DIR);
    ASSERT_TRUE(parser.parseFile(fixtureDir + "/point_only.dxf", data))
        << data.errorMessage().toStdString();
    EXPECT_TRUE(data.isValid());
    EXPECT_EQ(data.errorCode(), DxfImportErrorCode::None);
    EXPECT_EQ(data.points().size(), 2u);
}

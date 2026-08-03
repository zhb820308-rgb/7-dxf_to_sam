#include "DxfData.h"

#include <gtest/gtest.h>

TEST(DxfImportLayers, EmptyTextProducesNoIgnoredLayers)
{
    EXPECT_TRUE(parseIgnoredDxfLayers(QString()).empty());
}

TEST(DxfImportLayers, TrimsSkipsEmptyAndDeduplicates)
{
    const auto layers = parseIgnoredDxfLayers(
        QStringLiteral(" PARENT,IGNORE ,, KEEP,PARENT,   "));

    EXPECT_EQ(layers.size(), 3u);
    EXPECT_EQ(layers.count("PARENT"), 1u);
    EXPECT_EQ(layers.count("IGNORE"), 1u);
    EXPECT_EQ(layers.count("KEEP"), 1u);
}

TEST(DxfImportLayers, PreservesCaseAndInternalSpaces)
{
    const auto layers = parseIgnoredDxfLayers(
        QStringLiteral("Layer A,layer a"));

    EXPECT_EQ(layers.size(), 2u);
    EXPECT_EQ(layers.count("Layer A"), 1u);
    EXPECT_EQ(layers.count("layer a"), 1u);
}

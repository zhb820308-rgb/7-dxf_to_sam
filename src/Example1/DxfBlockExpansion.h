#pragma once

#include "DxfData.h"

#include <QString>

#include <cstddef>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

bool expandDxfBlocks(
    DxfData& output,
    const std::unordered_map<std::string, DxfBlock>& blocks,
    const std::vector<InsertInfo>& inserts,
    const std::set<std::string>& ignoredLayers,
    double tolerance,
    std::size_t initialEntities,
    std::size_t maxOutputEntities,
    QString& error);

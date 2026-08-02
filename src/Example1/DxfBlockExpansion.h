#pragma once

#include "DxfData.h"

#include <QString>

#include <cstddef>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

enum class DxfBlockExpansionStatus
{
    Success,
    ArrayInstanceLimit,
    BlockInstanceLimit,
    OutputEntityLimit,
    DepthLimit
};

struct DxfBlockExpansionRequest
{
    DxfData& output;
    const std::unordered_map<std::string, DxfBlock>& blocks;
    const std::vector<InsertInfo>& inserts;
    const std::set<std::string>& ignoredLayers;
    double curveTolerance;
    std::size_t initialEntities;
    std::size_t maxOutputEntities;
};

struct DxfBlockExpansionResult
{
    DxfBlockExpansionStatus status = DxfBlockExpansionStatus::Success;
    QString message;

    bool succeeded() const
    {
        return status == DxfBlockExpansionStatus::Success;
    }
};

DxfBlockExpansionResult expandDxfBlocks(
    const DxfBlockExpansionRequest& request);

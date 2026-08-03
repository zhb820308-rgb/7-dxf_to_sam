#pragma once

#include "DxfData.h"

#include <QString>

#include <cstddef>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * BLOCK 是一份可复用的局部坐标几何定义，INSERT 是“在哪里、以何种旋转/缩放放置
 * 这份定义”。展开器把顶层和嵌套 INSERT 递归变成实际模型空间实体。
 */
enum class DxfBlockExpansionStatus
{
    Success,
    ArrayInstanceLimit,
    BlockInstanceLimit,
    OutputEntityLimit,
    DepthLimit
};

// 请求用引用借用 Parser 已收集的输出、块表和 INSERT 表；函数不会复制整张图。
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

/// 展开所有模型空间 INSERT。失败时 request.output 可能含阶段数据，调用者必须丢弃。
DxfBlockExpansionResult expandDxfBlocks(
    const DxfBlockExpansionRequest& request);

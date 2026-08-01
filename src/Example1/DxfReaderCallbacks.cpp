#include "DxfReaderCallbacks.h"

#include "DxfEntityValidation.h"

#include <drw_entities.h>
#include <drw_objects.h>

#include <optional>
#include <utility>

void DxfReaderCallbacks::setIgnoredLayers(
    const std::set<std::string>& ignoredLayers)
{
    m_ignoredLayers = ignoredLayers;
}

DxfData DxfReaderCallbacks::takeData()
{
    return std::move(m_data);
}

bool DxfReaderCallbacks::isLayerIgnored(const DRW_Entity& entity) const
{
    if (m_ignoredLayers.empty()) return false;
    return m_ignoredLayers.count(entity.layer) != 0;
}

bool DxfReaderCallbacks::shouldSkipDuringRead(const DRW_Entity& entity) const
{
    return m_currentBlock == nullptr && isLayerIgnored(entity);
}

void DxfReaderCallbacks::addLine(const DRW_Line& data)
{
    if (shouldSkipDuringRead(data)) return;
    const DxfLine line = DxfEntityValidation::makeLine(data);
    if (m_currentBlock) {
        m_currentBlock->addLine(line);
    } else {
        m_data.addLine(line);
    }
}

void DxfReaderCallbacks::addCircle(const DRW_Circle& data)
{
    if (shouldSkipDuringRead(data)) return;
    const DxfCircle circle = DxfEntityValidation::makeCircle(data);
    if (m_currentBlock) {
        m_currentBlock->addCircle(circle);
    } else {
        m_data.addCircle(circle);
    }
}

void DxfReaderCallbacks::addArc(const DRW_Arc& data)
{
    if (shouldSkipDuringRead(data)) return;
    const std::optional<DxfArc> arc = DxfEntityValidation::makeArc(data);
    if (!arc) return;
    if (m_currentBlock) {
        m_currentBlock->addArc(*arc);
    } else {
        m_data.addArc(*arc);
    }
}

void DxfReaderCallbacks::addEllipse(const DRW_Ellipse& data)
{
    if (shouldSkipDuringRead(data)) return;
    const std::optional<DxfEllipse> ellipse =
        DxfEntityValidation::makeEllipse(data);
    if (!ellipse) return;
    if (m_currentBlock) {
        m_currentBlock->addEllipse(*ellipse);
    } else {
        m_data.addEllipse(*ellipse);
    }
}

void DxfReaderCallbacks::addLWPolyline(const DRW_LWPolyline& data)
{
    if (shouldSkipDuringRead(data)) return;
    const std::optional<DxfLWPolyline> polyline =
        DxfEntityValidation::makeLWPolyline(data);
    if (!polyline) return;
    if (m_currentBlock) {
        m_currentBlock->addLWPolyline(*polyline);
    } else {
        m_data.addLWPolyline(*polyline);
    }
}

void DxfReaderCallbacks::addSpline(const DRW_Spline* data)
{
    if (!data || shouldSkipDuringRead(*data)) return;
    std::optional<DxfSpline> spline =
        DxfEntityValidation::makeSpline(*data);
    if (!spline) return;
    if (m_currentBlock) {
        m_currentBlock->addSpline(std::move(*spline));
    } else {
        m_data.addSpline(std::move(*spline));
    }
}

void DxfReaderCallbacks::addPoint(const DRW_Point& data)
{
    if (shouldSkipDuringRead(data)) return;
    const DxfPoint point = DxfEntityValidation::makePoint(data);
    if (m_currentBlock) {
        m_currentBlock->addPoint(point);
    } else {
        m_data.addPoint(point);
    }
}

void DxfReaderCallbacks::addBlock(const DRW_Block& data)
{
    DxfBlock block;
    block.setName(data.name);
    block.setBase(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    if (block.name() == "*Model_Space"
        || block.name() == "*Paper_Space"
        || block.name() == "*Paper_Space0") {
        m_currentBlock = nullptr;
        return;
    }

    m_blocks[block.name()] = block;
    m_currentBlock = &m_blocks[block.name()];
}

void DxfReaderCallbacks::endBlock()
{
    m_currentBlock = nullptr;
}

void DxfReaderCallbacks::addInsert(const DRW_Insert& data)
{
    InsertInfo insert;
    insert.blockName = data.name;
    insert.layer = data.layer;
    insert.insertX = data.basePoint.x;
    insert.insertY = data.basePoint.y;
    insert.insertZ = data.basePoint.z;
    insert.scaleX = data.xscale;
    insert.scaleY = data.yscale;
    insert.scaleZ = data.zscale;
    insert.angle = data.angle;
    insert.colCount = data.colcount;
    insert.rowCount = data.rowcount;
    insert.colSpace = data.colspace;
    insert.rowSpace = data.rowspace;

    if (m_currentBlock) {
        m_currentBlock->addInsert(insert);
    } else {
        m_modelSpaceInserts.push_back(insert);
    }
}

void DxfReaderCallbacks::addLayer(const DRW_Layer& data)
{
    m_allLayers.insert(data.name);
}

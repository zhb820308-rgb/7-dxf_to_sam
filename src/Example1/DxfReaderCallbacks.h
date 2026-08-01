#pragma once

#include "DxfData.h"

#include <drw_interface.h>

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// Receives libdxfrw callbacks and stores normalized model-space entities,
// block definitions, INSERT references, and layer metadata for DxfParser.
class DxfReaderCallbacks : public DRW_Interface
{
public:
    void setIgnoredLayers(const std::set<std::string>& ignoredLayers);

    const DxfData& data() const { return m_data; }
    DxfData takeData();
    const std::unordered_map<std::string, DxfBlock>& blocks() const
    {
        return m_blocks;
    }
    const std::vector<InsertInfo>& modelSpaceInserts() const
    {
        return m_modelSpaceInserts;
    }
    const std::set<std::string>& allLayers() const { return m_allLayers; }

    void addLine(const DRW_Line& data) override;
    void addCircle(const DRW_Circle& data) override;
    void addArc(const DRW_Arc& data) override;
    void addEllipse(const DRW_Ellipse& data) override;
    void addLWPolyline(const DRW_LWPolyline& data) override;
    void addSpline(const DRW_Spline* data) override;
    void addPoint(const DRW_Point& data) override;

    void addBlock(const DRW_Block& data) override;
    void endBlock() override;
    void addInsert(const DRW_Insert& data) override;
    void addLayer(const DRW_Layer& data) override;

    void addHeader(const DRW_Header*) override {}
    void addLType(const DRW_LType&) override {}
    void addDimStyle(const DRW_Dimstyle&) override {}
    void addVport(const DRW_Vport&) override {}
    void addTextStyle(const DRW_Textstyle&) override {}
    void addAppId(const DRW_AppId&) override {}
    void setBlock(const int) override {}
    void addRay(const DRW_Ray&) override {}
    void addXline(const DRW_Xline&) override {}
    void addPolyline(const DRW_Polyline&) override {}
    void addKnot(const DRW_Entity&) override {}
    void addTrace(const DRW_Trace&) override {}
    void add3dFace(const DRW_3Dface&) override {}
    void addSolid(const DRW_Solid&) override {}
    void addMText(const DRW_MText&) override {}
    void addText(const DRW_Text&) override {}
    void addDimAlign(const DRW_DimAligned*) override {}
    void addDimLinear(const DRW_DimLinear*) override {}
    void addDimRadial(const DRW_DimRadial*) override {}
    void addDimDiametric(const DRW_DimDiametric*) override {}
    void addDimAngular(const DRW_DimAngular*) override {}
    void addDimAngular3P(const DRW_DimAngular3p*) override {}
    void addDimOrdinate(const DRW_DimOrdinate*) override {}
    void addLeader(const DRW_Leader*) override {}
    void addHatch(const DRW_Hatch*) override {}
    void addViewport(const DRW_Viewport&) override {}
    void addImage(const DRW_Image*) override {}
    void linkImage(const DRW_ImageDef*) override {}
    void addComment(const char*) override {}
    void addPlotSettings(const DRW_PlotSettings*) override {}
    void writeHeader(DRW_Header&) override {}
    void writeBlocks() override {}
    void writeBlockRecords() override {}
    void writeEntities() override {}
    void writeLTypes() override {}
    void writeLayers() override {}
    void writeTextstyles() override {}
    void writeVports() override {}
    void writeDimstyles() override {}
    void writeObjects() override {}
    void writeAppId() override {}

private:
    bool isLayerIgnored(const DRW_Entity& entity) const;
    bool shouldSkipDuringRead(const DRW_Entity& entity) const;

    DxfData m_data;
    std::unordered_map<std::string, DxfBlock> m_blocks;
    std::vector<InsertInfo> m_modelSpaceInserts;
    DxfBlock* m_currentBlock = nullptr;
    std::set<std::string> m_ignoredLayers;
    std::set<std::string> m_allLayers;
};

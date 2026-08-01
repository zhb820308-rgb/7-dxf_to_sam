#include "DxfLayerReader.h"

#include "DxfInputFile.h"
#include <libdxfrw.h>

#include <set>

namespace {

class LayerReader : public DRW_Interface
{
public:
    std::set<std::string> layers;

    void addLayer(const DRW_Layer& data) override
    {
        layers.insert(data.name);
    }

    void addHeader(const DRW_Header*) override {}
    void addLType(const DRW_LType&) override {}
    void addDimStyle(const DRW_Dimstyle&) override {}
    void addVport(const DRW_Vport&) override {}
    void addTextStyle(const DRW_Textstyle&) override {}
    void addAppId(const DRW_AppId&) override {}
    void setBlock(const int) override {}
    void addBlock(const DRW_Block&) override {}
    void endBlock() override {}
    void addPoint(const DRW_Point&) override {}
    void addLine(const DRW_Line&) override {}
    void addRay(const DRW_Ray&) override {}
    void addXline(const DRW_Xline&) override {}
    void addCircle(const DRW_Circle&) override {}
    void addArc(const DRW_Arc&) override {}
    void addEllipse(const DRW_Ellipse&) override {}
    void addPolyline(const DRW_Polyline&) override {}
    void addSpline(const DRW_Spline*) override {}
    void addKnot(const DRW_Entity&) override {}
    void addInsert(const DRW_Insert&) override {}
    void addTrace(const DRW_Trace&) override {}
    void add3dFace(const DRW_3Dface&) override {}
    void addSolid(const DRW_Solid&) override {}
    void addLWPolyline(const DRW_LWPolyline&) override {}
    void addText(const DRW_Text&) override {}
    void addMText(const DRW_MText&) override {}
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
};

} // namespace

bool collectDxfLayers(const QString& filePath, QStringList& layers)
{
    layers.clear();
    if (filePath.isEmpty()) {
        return false;
    }

    DxfInputFile inputFile(filePath);
    if (!inputFile.prepare()) {
        return false;
    }

    dxfRW dxf(inputFile.encodedPath().constData());
    LayerReader reader;
    if (!dxf.read(&reader, true)) {
        return false;
    }

    for (const std::string& layer : reader.layers) {
        layers.append(QString::fromLocal8Bit(layer.c_str()));
    }
    layers.sort(Qt::CaseInsensitive);
    return true;
}

#include "DxfParser.h"

#include "GeometryUtils.h"
#include "libdxfrw.h"
#include <QDebug>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace {

// ========================================================================
//  DxfReader — DRW_Interface implementation (internal, only used here)
// ========================================================================

class DxfReader : public DRW_Interface {
public:
    DxfData m_data;                         // final flattened output

    // --- Parsing context ---
    std::unordered_map<std::string, DxfBlock> m_blocks;
    std::vector<InsertInfo>      m_modelSpaceInserts;

    // current block being parsed (nullptr = model space)
    DxfBlock* m_currentBlock = nullptr;

    // --- Implemented entity callbacks ---
    void addLine(const DRW_Line& data) override;
    void addCircle(const DRW_Circle& data) override;
    void addArc(const DRW_Arc& data) override;
    void addEllipse(const DRW_Ellipse& data) override;
    void addLWPolyline(const DRW_LWPolyline& data) override;
    void addSpline(const DRW_Spline* data) override;
    void addPoint(const DRW_Point& data) override;

    // --- Block / Insert callbacks ---
    void addBlock(const DRW_Block& data) override;
    void endBlock() override;
    void addInsert(const DRW_Insert& data) override;

    // --- Stub callbacks (no-op) ---
    void addHeader(const DRW_Header* data) override {}
    void addLType(const DRW_LType& data) override {}
    void addLayer(const DRW_Layer& data) override {}
    void addDimStyle(const DRW_Dimstyle& data) override {}
    void addVport(const DRW_Vport& data) override {}
    void addTextStyle(const DRW_Textstyle& data) override {}
    void addAppId(const DRW_AppId& data) override {}
    void setBlock(const int handle) override {}
    void addRay(const DRW_Ray& data) override {}
    void addXline(const DRW_Xline& data) override {}
    void addPolyline(const DRW_Polyline& data) override {}
    void addKnot(const DRW_Entity& data) override {}
    void addTrace(const DRW_Trace& data) override {}
    void add3dFace(const DRW_3Dface& data) override {}
    void addSolid(const DRW_Solid& data) override {}
    void addMText(const DRW_MText& data) override {}
    void addText(const DRW_Text& data) override {}
    void addDimAlign(const DRW_DimAligned* data) override {}
    void addDimLinear(const DRW_DimLinear* data) override {}
    void addDimRadial(const DRW_DimRadial* data) override {}
    void addDimDiametric(const DRW_DimDiametric* data) override {}
    void addDimAngular(const DRW_DimAngular* data) override {}
    void addDimAngular3P(const DRW_DimAngular3p* data) override {}
    void addDimOrdinate(const DRW_DimOrdinate* data) override {}
    void addLeader(const DRW_Leader* data) override {}
    void addHatch(const DRW_Hatch* data) override {}
    void addViewport(const DRW_Viewport& data) override {}
    void addImage(const DRW_Image* data) override {}
    void linkImage(const DRW_ImageDef* data) override {}
    void addComment(const char* comment) override {}
    void addPlotSettings(const DRW_PlotSettings* data) override {}
    void writeHeader(DRW_Header& data) override {}
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

// ========================================================================
//  Block expansion helpers
// ========================================================================

/// Transform a point by an insert's translation / rotation / scale.
static DxfPoint transformPoint(const DxfPoint& pt,
                               const InsertInfo& ins,
                               double blockBaseX, double blockBaseY, double blockBaseZ)
{
    double x = pt.x() - blockBaseX;
    double y = pt.y() - blockBaseY;
    double z = pt.z() - blockBaseZ;

    x *= ins.scaleX;
    y *= ins.scaleY;
    z *= ins.scaleZ;

    if (std::fabs(ins.angle) > 1e-12) {
        const double cosA = std::cos(ins.angle);
        const double sinA = std::sin(ins.angle);
        const double rx = x * cosA - y * sinA;
        const double ry = x * sinA + y * cosA;
        x = rx;
        y = ry;
    }

    x += ins.insertX;
    y += ins.insertY;
    z += ins.insertZ;

    return DxfPoint(x, y, z);
}

/// Transform and append discretized segments to output.
static void addTransformedSegments(DxfData& output,
                                   const std::vector<DxfLine>& segments,
                                   const InsertInfo& ins,
                                   double baseX, double baseY, double baseZ)
{
    for (const DxfLine& seg : segments) {
        if (!seg.isValid()) continue;
        DxfPoint s = transformPoint(seg.start(), ins, baseX, baseY, baseZ);
        DxfPoint e = transformPoint(seg.end(),   ins, baseX, baseY, baseZ);
        output.addLine(DxfLine(s, e));
    }
}

/// Check whether scales are approximately uniform in XY (for circle preservation).
static bool isScaleUniformXY(const InsertInfo& ins) {
    return std::fabs(ins.scaleX - ins.scaleY) < 1e-9
        && std::fabs(ins.scaleX) > 1e-12;
}

/// Forward declaration for recursive expansion.
static void expandSingleBlock(DxfData& output,
                              const DxfBlock& blk,
                              const InsertInfo& ins,
                              double tolerance,
                              const std::unordered_map<std::string, DxfBlock>& blocks);

/// Expand one INSERT with array (row × col) support.
/// Generates all array instances and delegates each to expandSingleBlock.
static void expandInsertArray(DxfData& output,
                              const DxfBlock& blk,
                              const InsertInfo& ins,
                              double tolerance,
                              const std::unordered_map<std::string, DxfBlock>& blocks)
{
    const int nCols = std::max(1, ins.colCount);
    const int nRows = std::max(1, ins.rowCount);
    for (int row = 0; row < nRows; ++row) {
        for (int col = 0; col < nCols; ++col) {
            InsertInfo insCopy = ins;
            insCopy.insertX += col * ins.colSpace;
            insCopy.insertY += row * ins.rowSpace;
            expandSingleBlock(output, blk, insCopy, tolerance, blocks);
        }
    }
}

/// Expand a single block instance (one INSERT, one array element).
/// @param blocks  block definitions map, needed for recursive nested INSERT expansion
static void expandSingleBlock(DxfData& output,
                              const DxfBlock& blk,
                              const InsertInfo& ins,
                              double tolerance,
                              const std::unordered_map<std::string, DxfBlock>& blocks)
{
    const double bx = blk.baseX(), by = blk.baseY(), bz = blk.baseZ();

    // --- Points: direct transform ---
    for (const DxfPoint& pt : blk.points()) {
        if (!pt.isValid()) continue;
        output.addPoint(transformPoint(pt, ins, bx, by, bz));
    }

    // --- Lines: direct transform ---
    for (const DxfLine& line : blk.lines()) {
        if (!line.isValid()) continue;
        DxfPoint s = transformPoint(line.start(), ins, bx, by, bz);
        DxfPoint e = transformPoint(line.end(),   ins, bx, by, bz);
        output.addLine(DxfLine(s, e));
    }

    // --- Circles ---
    for (const DxfCircle& circle : blk.circles()) {
        if (!circle.isValid()) continue;
        if (isScaleUniformXY(ins) && std::fabs(ins.scaleZ - ins.scaleX) < 1e-9) {
            // Uniform scale → preserve as circle
            DxfPoint c = transformPoint(circle.center(), ins, bx, by, bz);
            double   r = circle.radius() * ins.scaleX;
            if (r > 0.0)
                output.addCircle(DxfCircle(c, r));
        } else {
            // Non-uniform or negative scale → discretize to lines
            std::vector<DxfLine> segs = GeometryUtils::tessellateArc(
                DxfArc(circle.center(), circle.radius(), 0.0, 2.0 * M_PI, true),
                tolerance);
            addTransformedSegments(output, segs, ins, bx, by, bz);
        }
    }

    // --- Arcs: uniform scale → preserve Arc; non-uniform → discretize ---
    const bool uniformXY = isScaleUniformXY(ins);
    for (const DxfArc& arc : blk.arcs()) {
        if (!arc.isValid()) continue;
        if (uniformXY) {
            output.addArc(DxfArc(
                transformPoint(arc.center(), ins, bx, by, bz),
                arc.radius() * ins.scaleX,
                arc.startAngle(), arc.endAngle(), arc.isCCW()));
        } else {
            addTransformedSegments(output,
                                   GeometryUtils::tessellateArc(arc, tolerance),
                                   ins, bx, by, bz);
        }
    }

    // --- LWPolylines: uniform scale → preserve; non-uniform → discretize ---
    for (const DxfLWPolyline& poly : blk.lwPolylines()) {
        if (!poly.isValid()) continue;
        if (uniformXY) {
            std::vector<DxfPoint> verts;
            for (const DxfPoint& v : poly.vertices())
                verts.push_back(transformPoint(v, ins, bx, by, bz));
            output.addLWPolyline(DxfLWPolyline(verts, poly.bulges(),
                                                poly.isClosed(), poly.constZ() * ins.scaleZ));
        } else {
            addTransformedSegments(output,
                                   GeometryUtils::tessellateLWPolyline(poly, tolerance),
                                   ins, bx, by, bz);
        }
    }

    // --- Ellipses: uniform scale → preserve; non-uniform → discretize ---
    for (const DxfEllipse& ellipse : blk.ellipses()) {
        if (!ellipse.isValid()) continue;
        if (uniformXY) {
            DxfPoint c = transformPoint(ellipse.center(), ins, bx, by, bz);
            DxfPoint m(ellipse.majorAxisEnd().x() * ins.scaleX,
                       ellipse.majorAxisEnd().y() * ins.scaleY,
                       ellipse.majorAxisEnd().z() * ins.scaleZ);
            output.addEllipse(DxfEllipse(c, m, ellipse.ratio(),
                                          ellipse.startParam(), ellipse.endParam(), ellipse.isCCW()));
        } else {
            addTransformedSegments(output,
                                   GeometryUtils::tessellateEllipse(ellipse, tolerance),
                                   ins, bx, by, bz);
        }
    }

    // --- Splines: uniform scale → preserve; non-uniform → discretize ---
    for (const DxfSpline& spline : blk.splines()) {
        if (!spline.isValid()) continue;
        if (uniformXY) {
            std::vector<DxfPoint> ctrlPts;
            for (const DxfPoint& cp : spline.controlPoints())
                ctrlPts.push_back(transformPoint(cp, ins, bx, by, bz));
            std::vector<DxfPoint> fitPts;
            for (const DxfPoint& fp : spline.fitPoints())
                fitPts.push_back(transformPoint(fp, ins, bx, by, bz));
            output.addSpline(DxfSpline(ctrlPts, spline.knots(), spline.weights(), fitPts,
                                        spline.degree(), spline.flags(),
                                        spline.tgStartX(), spline.tgStartY(), spline.tgStartZ(),
                                        spline.tgEndX(), spline.tgEndY(), spline.tgEndZ()));
        } else {
            addTransformedSegments(output,
                                   GeometryUtils::tessellateSpline(spline, tolerance),
                                   ins, bx, by, bz);
        }
    }

    // --- Nested INSERTs: recursive expansion ---
    for (const InsertInfo& nested : blk.inserts()) {
        auto it = blocks.find(nested.blockName);
        if (it == blocks.end()) {
            qWarning() << "[BlockExpand] nested INSERT references unknown block:"
                       << nested.blockName.c_str() << "- skipped";
            continue;
        }
        const DxfBlock& nestedBlk = it->second;

        // Compose transforms: outer * inner
        const DxfPoint nestedPos = transformPoint(
            DxfPoint(nested.insertX, nested.insertY, nested.insertZ),
            ins, bx, by, bz);

        InsertInfo composed;
        composed.blockName = nested.blockName;
        composed.insertX   = nestedPos.x();
        composed.insertY   = nestedPos.y();
        composed.insertZ   = nestedPos.z();
        composed.scaleX    = ins.scaleX * nested.scaleX;
        composed.scaleY    = ins.scaleY * nested.scaleY;
        composed.scaleZ    = ins.scaleZ * nested.scaleZ;
        composed.angle     = ins.angle  + nested.angle;
        composed.colCount  = nested.colCount;
        composed.rowCount  = nested.rowCount;
        composed.colSpace  = nested.colSpace;
        composed.rowSpace  = nested.rowSpace;

        // Generate nested array instances
        expandInsertArray(output, nestedBlk, composed, tolerance, blocks);
    }
}

/// Expand all model-space inserts into the output DxfData.
/// @param output  [in/out] already contains model-space entities; INSERT entities appended here
static void expandBlocks(DxfData& output,
                         const std::unordered_map<std::string, DxfBlock>& blocks,
                         const std::vector<InsertInfo>& inserts,
                         double tolerance)
{
    // Model-space entities are already in output (written directly during parsing).
    // Only INSERT expansion is needed here.

    // Expand INSERTs
    int expandedInserts = 0;
    int skippedInserts  = 0;

    for (const InsertInfo& ins : inserts) {
        auto it = blocks.find(ins.blockName);
        if (it == blocks.end()) {
            qWarning() << "[BlockExpand] INSERT references unknown block:" << ins.blockName.c_str() << "- skipped";
            ++skippedInserts;
            continue;
        }
        const DxfBlock& blk = it->second;
        expandInsertArray(output, blk, ins, tolerance, blocks);
        expandedInserts += std::max(1, ins.colCount) * std::max(1, ins.rowCount);
    }

    qDebug() << "[BlockExpand] expanded inserts:" << expandedInserts
             << "skipped:" << skippedInserts;
}

// ========================================================================
//  DxfReader member functions — entity callbacks
// ========================================================================

void DxfReader::addLine(const DRW_Line& data) {
    DxfPoint start(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    DxfPoint end(data.secPoint.x, data.secPoint.y, data.secPoint.z);
    DxfLine line(start, end);

    if (m_currentBlock) {
        m_currentBlock->addLine(line);
    } else {
        m_data.addLine(line);
    }
}

void DxfReader::addCircle(const DRW_Circle& data)
{
    DxfPoint center(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    DxfCircle circle(center, data.radious);

    if (m_currentBlock) {
        m_currentBlock->addCircle(circle);
    } else {
        m_data.addCircle(circle);
    }
}

void DxfReader::addArc(const DRW_Arc& data) {
    const DRW_Coord center = data.basePoint;
    const double radius = data.radious;

    if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)) {
        qDebug() << "[DxfReader] addArc skipped: invalid center";
        return;
    }
    if (!std::isfinite(radius) || radius <= 0.0) {
        qDebug() << "[DxfReader] addArc skipped: invalid radius" << radius;
        return;
    }
    double start = data.staangle;
    double end = data.endangle;
    if (!std::isfinite(start) || !std::isfinite(end)) {
        qDebug() << "[DxfReader] addArc skipped: NaN angles";
        return;
    }

    DxfPoint c(center.x, center.y, center.z);
    DxfArc arc(c, radius, start, end, data.isccw);

    if (m_currentBlock) {
        m_currentBlock->addArc(arc);
    } else {
        m_data.addArc(arc);
    }
}

void DxfReader::addEllipse(const DRW_Ellipse& data) {
    const double majorX = data.secPoint.x;
    const double majorY = data.secPoint.y;
    const double majorLen = std::sqrt(majorX * majorX + majorY * majorY);

    if (majorLen <= 0.0) return;
    if (!std::isfinite(data.ratio) || data.ratio <= 0.0) return;

    DxfPoint center(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    DxfPoint majorAxisEnd(data.secPoint.x, data.secPoint.y, data.secPoint.z);
    DxfEllipse ellipse(center, majorAxisEnd, data.ratio,
                        data.staparam, data.endparam, data.isccw);

    if (m_currentBlock) {
        m_currentBlock->addEllipse(ellipse);
    } else {
        m_data.addEllipse(ellipse);
    }
}

void DxfReader::addLWPolyline(const DRW_LWPolyline& data)
{
    const int numVerts = std::min(data.vertexnum, static_cast<int>(data.vertlist.size()));
    if (numVerts < 2) return;

    const bool isClosed = (data.flags & 1) != 0;

    std::vector<DxfPoint> vertices;
    std::vector<double>   bulges;
    vertices.reserve(numVerts);
    bulges.reserve(isClosed ? numVerts : numVerts - 1);

    for (int i = 0; i < numVerts; ++i) {
        const DRW_Vertex2D& v = *data.vertlist[i];
        vertices.push_back(DxfPoint(v.x, v.y, 0.0));
    }

    if (isClosed) {
        for (int i = 0; i < numVerts; ++i) {
            bulges.push_back(data.vertlist[i]->bulge);
        }
    } else {
        for (int i = 0; i < numVerts - 1; ++i) {
            bulges.push_back(data.vertlist[i]->bulge);
        }
    }

    DxfLWPolyline poly(vertices, bulges, isClosed, 0.0);

    if (m_currentBlock) {
        m_currentBlock->addLWPolyline(poly);
    } else {
        m_data.addLWPolyline(poly);
    }
}

void DxfReader::addSpline(const DRW_Spline* data)
{
    if (!data) return;

    const bool isRational = (data->flags & 4) != 0;
    const bool isPeriodic = (data->flags & 2) != 0;
    const bool isClosed   = (data->flags & 1) != 0;

    const bool hasControlData =
        data->ncontrol > 0
        && static_cast<int>(data->controllist.size()) >= data->ncontrol
        && data->degree >= 1
        && data->ncontrol > data->degree;

    const bool hasFitData =
        data->nfit >= 2
        && static_cast<int>(data->fitlist.size()) >= data->nfit;

    if (!hasControlData && !hasFitData) {
        qDebug() << "[Spline] no valid control or fit data; ncontrol=" << data->ncontrol
                 << " degree=" << data->degree << " nfit=" << data->nfit;
        return;
    }

    if (hasControlData) {
        const int numCtrl = data->ncontrol;
        const int degree  = data->degree;
        const std::vector<double>& knots = data->knotslist;
        const int expectedKnotCount = numCtrl + degree + 1;

        if (static_cast<int>(knots.size()) != expectedKnotCount) {
            qDebug() << "[Spline] ERROR: knot count mismatch";
            return;
        }
        for (int i = 0; i < expectedKnotCount; ++i) {
            if (!std::isfinite(knots[i]) || (i > 0 && knots[i] < knots[i - 1])) {
                qDebug() << "[Spline] ERROR: invalid knot at index" << i;
                return;
            }
        }
        for (int i = 0; i < numCtrl; ++i) {
            const auto& point = data->controllist[i];
            if (!point || !std::isfinite(point->x) || !std::isfinite(point->y) || !std::isfinite(point->z)) {
                qDebug() << "[Spline] ERROR: invalid control point at index" << i;
                return;
            }
        }
        if (isRational) {
            if (static_cast<int>(data->weightlist.size()) < numCtrl) {
                qDebug() << "[Spline] ERROR: insufficient weights";
                return;
            }
            for (int i = 0; i < numCtrl; ++i) {
                const double w = data->weightlist[i];
                if (!std::isfinite(w) || w <= 0.0) {
                    qDebug() << "[Spline] ERROR: invalid weight at index" << i;
                    return;
                }
            }
        }
    }

    std::vector<DxfPoint> ctrlPts;
    for (int i = 0; i < data->ncontrol; ++i) {
        const DRW_Coord& pt = *(data->controllist[i]);
        ctrlPts.push_back(DxfPoint(pt.x, pt.y, pt.z));
    }

    std::vector<double> knots = data->knotslist;

    std::vector<double> weights;
    if (isRational) {
        for (int i = 0; i < data->ncontrol; ++i)
            weights.push_back(data->weightlist[i]);
    }

    std::vector<DxfPoint> fitPts;
    for (int i = 0; i < data->nfit; ++i) {
        const auto& sp = data->fitlist[i];
        if (sp && std::isfinite(sp->x) && std::isfinite(sp->y) && std::isfinite(sp->z))
            fitPts.push_back(DxfPoint(sp->x, sp->y, sp->z));
    }

    double tgStartX = data->tgStart.x, tgStartY = data->tgStart.y, tgStartZ = data->tgStart.z;
    double tgEndX   = data->tgEnd.x,   tgEndY   = data->tgEnd.y,   tgEndZ   = data->tgEnd.z;

    DxfSpline spline(ctrlPts, knots, weights, fitPts,
                      data->degree, data->flags,
                      tgStartX, tgStartY, tgStartZ,
                      tgEndX, tgEndY, tgEndZ);

    if (m_currentBlock) {
        m_currentBlock->addSpline(spline);
    } else {
        m_data.addSpline(spline);
    }
}

void DxfReader::addPoint(const DRW_Point& data) {
    DxfPoint pt(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    if (m_currentBlock) {
        m_currentBlock->addPoint(pt);
    } else {
        m_data.addPoint(pt);
    }
}

// ========================================================================
//  DxfReader member functions — block / insert callbacks
// ========================================================================

void DxfReader::addBlock(const DRW_Block& data) {
    DxfBlock blk;
    blk.setName(data.name);
    blk.setBase(data.basePoint.x, data.basePoint.y, data.basePoint.z);

    qDebug() << "[DxfReader] addBlock:" << blk.name().c_str()
             << "base:(" << blk.baseX() << "," << blk.baseY() << "," << blk.baseZ() << ")"
             << "flags:" << data.flags;

    // Skip layout blocks
    if (blk.name() == "*Model_Space" || blk.name() == "*Paper_Space" || blk.name() == "*Paper_Space0") {
        qDebug() << "[DxfReader] skipping layout block:" << blk.name().c_str();
        m_currentBlock = nullptr;
        return;
    }

    m_blocks[blk.name()] = blk;
    m_currentBlock = &m_blocks[blk.name()];
}

void DxfReader::endBlock() {
    if (m_currentBlock) {
        qDebug() << "[DxfReader] endBlock:" << m_currentBlock->name().c_str()
                 << "lines:" << m_currentBlock->lines().size()
                 << "circles:" << m_currentBlock->circles().size()
                 << "arcs:" << m_currentBlock->arcs().size()
                 << "lwPolylines:" << m_currentBlock->lwPolylines().size()
                 << "ellipses:" << m_currentBlock->ellipses().size()
                 << "splines:" << m_currentBlock->splines().size();
    }
    m_currentBlock = nullptr;
}

void DxfReader::addInsert(const DRW_Insert& data) {
    InsertInfo ins;
    ins.blockName = data.name;
    ins.insertX   = data.basePoint.x;
    ins.insertY   = data.basePoint.y;
    ins.insertZ   = data.basePoint.z;
    ins.scaleX    = data.xscale;
    ins.scaleY    = data.yscale;
    ins.scaleZ    = data.zscale;
    ins.angle     = data.angle;
    ins.colCount  = data.colcount;
    ins.rowCount  = data.rowcount;
    ins.colSpace  = data.colspace;
    ins.rowSpace  = data.rowspace;

    if (m_currentBlock) {
        // Nested INSERT — store in parent block for recursive expansion
        qDebug() << "[DxfReader] addInsert (nested):" << ins.blockName.c_str()
                 << "inside block" << m_currentBlock->name().c_str();
        m_currentBlock->addInsert(ins);
        return;
    }

    qDebug() << "[DxfReader] addInsert:" << ins.blockName.c_str()
             << "at (" << ins.insertX << "," << ins.insertY << "," << ins.insertZ << ")"
             << "scale (" << ins.scaleX << "," << ins.scaleY << "," << ins.scaleZ << ")"
             << "angle" << ins.angle << "rad"
             << "cols" << ins.colCount << "rows" << ins.rowCount;

    m_modelSpaceInserts.push_back(ins);
}

}  // namespace

// ========================================================================
//  DxfParser::parseFile
// ========================================================================

bool DxfParser::parseFile(const QString& filePath, DxfData& outData, double curveTolerance) {
    outData = DxfData();
    if (filePath.isEmpty()) {
        outData.setErrorMessage(QStringLiteral("DXF file is empty"));
        return false;
    }
    const QByteArray pathBytes = filePath.toLocal8Bit();
    dxfRW dxf(pathBytes.constData());
    DxfReader reader;
    if (!dxf.read(&reader, true)) {
        outData.setErrorMessage(
            QStringLiteral("Failed to read DXF file. Error code: %1")
            .arg(static_cast<int>(dxf.getError())));
        return false;
    }

    // --- Move parsed entities from reader to output, then expand blocks ---
    outData = std::move(reader.m_data);

    // --- Expand blocks into flat DxfData ---
    expandBlocks(outData, reader.m_blocks, reader.m_modelSpaceInserts, curveTolerance);

    outData.setValid(true);
    qDebug() << "[DxfParser] read ok:"
             << "lines=" << outData.lines().size()
             << "circles=" << outData.circles().size()
             << "arcs=" << outData.arcs().size()
             << "lwPolylines=" << outData.lwPolylines().size()
             << "ellipses=" << outData.ellipses().size()
             << "splines=" << outData.splines().size()
             << "blocks=" << reader.m_blocks.size()
             << "inserts=" << reader.m_modelSpaceInserts.size();

    const int total = outData.entityCount();
    if (total == 0) {
        outData.setErrorMessage(
            QStringLiteral("DXF was read successfully, but no supported entities were found."));
        return false;
    }
    return true;
}
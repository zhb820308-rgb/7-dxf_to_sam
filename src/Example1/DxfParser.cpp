#include "DxfParser.h"

#include "libdxfrw.h"
#include <QDebug>
#include <cmath>

// ========================================================================
//  DxfReader — DRW_Interface implementation (internal, only used here)
// ========================================================================

namespace {

class DxfReader : public DRW_Interface {
public:
	DxfData m_data;

	// Implemented entity callbacks
	void addLine(const DRW_Line& data) override;
	void addCircle(const DRW_Circle& data) override;
	void addArc(const DRW_Arc& data) override;
	void addEllipse(const DRW_Ellipse& data) override;
	void addLWPolyline(const DRW_LWPolyline& data) override;

	// Stub callbacks (no-op)
	void addHeader(const DRW_Header* data) override {}
	void addLType(const DRW_LType& data) override {}
	void addLayer(const DRW_Layer& data) override {}
	void addDimStyle(const DRW_Dimstyle& data) override {}
	void addVport(const DRW_Vport& data) override {}
	void addTextStyle(const DRW_Textstyle& data) override {}
	void addAppId(const DRW_AppId& data) override {}
	void addBlock(const DRW_Block& data) override {}
	void setBlock(const int handle) override {}
	void endBlock() override {}
	void addPoint(const DRW_Point& data) override {}
	void addRay(const DRW_Ray& data) override {}
	void addXline(const DRW_Xline& data) override {}
	void addPolyline(const DRW_Polyline& data) override {}
	void addSpline(const DRW_Spline* data) override {}
	void addKnot(const DRW_Entity& data) override {}
	void addInsert(const DRW_Insert& data) override {}
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

void DxfReader::addLine(const DRW_Line& data) {
	DxfPoint start(data.basePoint.x, data.basePoint.y, data.basePoint.z);
	DxfPoint end(data.secPoint.x, data.secPoint.y, data.secPoint.z);
	m_data.addLine(DxfLine(start, end));
	if (m_data.lines().size() <= 3)
		qDebug() << "[DxfReader] addLine" << start.x() << start.y() << "->" << end.x() << end.y();
}

void DxfReader::addCircle(const DRW_Circle& data)
{
	DxfPoint center(data.basePoint.x, data.basePoint.y, data.basePoint.z);
	m_data.addCircle(DxfCircle(center, data.radious));
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
	m_data.addArc(DxfArc(c, radius, start, end, data.isccw));
}

void DxfReader::addEllipse(const DRW_Ellipse& data) {
	// 验证长轴长度
	const double majorX = data.secPoint.x;
	const double majorY = data.secPoint.y;
	const double majorLen = std::sqrt(majorX * majorX + majorY * majorY);

	if (majorLen <= 0.0) return;
	if (!std::isfinite(data.ratio) || data.ratio <= 0.0) return;

	DxfPoint center(data.basePoint.x, data.basePoint.y, data.basePoint.z);
	DxfPoint majorAxisEnd(data.secPoint.x, data.secPoint.y, data.secPoint.z);

	m_data.addEllipse(DxfEllipse(center, majorAxisEnd, data.ratio,
	                              data.staparam, data.endparam, data.isccw));
}

void DxfReader::addLWPolyline(const DRW_LWPolyline& data)
{
	const int numVerts = std::min(data.vertexnum, static_cast<int>(data.vertlist.size()));
	qDebug() << "[DxfReader] addLWPolyline vertexnum=" << numVerts
	         << "flags=" << data.flags << "vertlist=" << (int)data.vertlist.size();
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

	// bulge[i] 描述顶点 i 到顶点 i+1 的弧段
	// 闭合时最后一个 bulge 描述最后一个顶点到第一个顶点的弧段
	if (isClosed) {
		for (int i = 0; i < numVerts; ++i) {
			bulges.push_back(data.vertlist[i]->bulge);
		}
	} else {
		for (int i = 0; i < numVerts - 1; ++i) {
			bulges.push_back(data.vertlist[i]->bulge);
		}
	}

	m_data.addLWPolyline(DxfLWPolyline(vertices, bulges, isClosed, 0.0));
}

}  // namespace (anonymous)

// ========================================================================
//  DxfParser::parseFile
// ========================================================================

bool DxfParser::parseFile(const QString& filePath, DxfData& outData) {
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
	outData = reader.m_data;
	outData.setValid(true);
	qDebug() << "[DxfParser] read ok:"
	         << "lines=" << outData.lines().size()
	         << "circles=" << outData.circles().size()
	         << "arcs=" << outData.arcs().size()
	         << "lwPolylines=" << outData.lwPolylines().size()
	         << "ellipses=" << outData.ellipses().size();

	// 检查是否至少有一个实体
	const int total = outData.entityCount();
	if (total == 0) {
		outData.setErrorMessage(
			QStringLiteral("DXF was read successfully, but no supported entities were found."));
		return false;
	}
	return true;
}

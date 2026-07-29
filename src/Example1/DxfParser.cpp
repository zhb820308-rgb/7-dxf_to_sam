#include "DxfParser.h"

#include <QDebug>
#include <cmath>

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

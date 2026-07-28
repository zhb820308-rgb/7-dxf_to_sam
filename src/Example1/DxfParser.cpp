#include "DxfParser.h"
#include "ConversionEngine.h"
#include "GeometryUtils.h"

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
	// ---- 基础量计算 ----
	const DRW_Coord center = data.basePoint;
	const double majorX = data.secPoint.x;
	const double majorY = data.secPoint.y;
	const double majorLen = std::sqrt(majorX * majorX + majorY * majorY);

	if (majorLen <= 0.0) return;
	if (!std::isfinite(data.ratio) || data.ratio <= 0.0) return;

	// 次轴向量（垂直于长轴，长度 = 长轴长度 × ratio）
	const double minorX = -majorY * data.ratio;
	const double minorY =  majorX * data.ratio;

	// ---- 参数范围处理 ----
	double startParam = data.staparam;
	double endParam   = data.endparam;

	double sweep = endParam - startParam;
	if (data.isccw) {
		while (sweep <= 0.0) { sweep += 2.0 * M_PI; }
	} else {
		while (sweep >= 0.0) { sweep -= 2.0 * M_PI; }
	}

	// ---- 分段数计算（基于更大的半轴长度） ----
	const double maxRadius = (majorLen > majorLen * data.ratio)
		? majorLen : majorLen * data.ratio;
	const int segments = calculateSegmentCount(
		maxRadius, std::abs(sweep), 0.01);

	// ---- 采样函数（参数方程） ----
	auto pointAt = [&](double t) -> DxfPoint {
		return DxfPoint(
			center.x + majorX * std::cos(t) + minorX * std::sin(t),
			center.y + majorY * std::cos(t) + minorY * std::sin(t),
			center.z);
	};

	// ---- 采样并输出折线段 ----
	DxfPoint previous = pointAt(startParam);
	for (int i = 1; i <= segments; ++i) {
		double t = startParam + sweep * static_cast<double>(i)
		           / static_cast<double>(segments);
		DxfPoint current = pointAt(t);
		DxfPolylineSegment seg(previous, current, 0.0);
		m_data.addPolylineSegment(seg);
		previous = current;
	}
}
void DxfReader::addLWPolyline(const DRW_LWPolyline& data)
{
	const int numVerts = data.vertexnum;
	qDebug() << "[DxfReader] addLWPolyline vertexnum=" << numVerts
	         << "flags=" << data.flags << "vertlist=" << (int)data.vertlist.size();
	if (numVerts < 2) return;
	const bool isClosed = (data.flags & 1) != 0;

	const double tol = ConversionEngine::defaultBulgeTolerance();

	auto tessellateSegment = [&](const DRW_Vertex2D& v1, const DRW_Vertex2D& v2) {
		DxfPoint p0(v1.x, v1.y, 0.0);
		DxfPoint p1(v2.x, v2.y, 0.0);
		std::vector<DxfPoint> pts =
			GeometryUtils::tessellateBulgeArc(p0, p1, v1.bulge, tol);
		for (size_t i = 1; i < pts.size(); ++i) {
			m_data.addPolylineSegment(
				DxfPolylineSegment(pts[i - 1], pts[i], 0.0));
		}
	};

	for (int i = 0; i < numVerts - 1; ++i) {
		tessellateSegment(*data.vertlist[i], *data.vertlist[i + 1]);
	}
	if (isClosed) {
		tessellateSegment(*data.vertlist.back(), *data.vertlist.front());
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

	double sweep = end - start;
	if (!std::isfinite(sweep)) {
		qDebug() << "[DxfReader] addArc skipped: NaN sweep";
		return;
	}

	// Normalize sweep to [-2pi, 2pi]
	if (data.isccw) {
		while (sweep <= 0.0)   { sweep += 2.0 * M_PI; }
		while (sweep > 2.0 * M_PI) { sweep -= 2.0 * M_PI; }
	} else {
		while (sweep >= 0.0)   { sweep -= 2.0 * M_PI; }
		while (sweep < -2.0 * M_PI) { sweep += 2.0 * M_PI; }
	}

	// Compute arc endpoints from center + radius + angles
	DxfPoint p0(center.x + radius * std::cos(start),
	            center.y + radius * std::sin(start),
	            center.z);
	DxfPoint p1(center.x + radius * std::cos(end),
	            center.y + radius * std::sin(end),
	            center.z);

	// Convert ARC to bulge representation: bulge = tan(sweep / 4)
	double bulge = std::tan(sweep * 0.25);

	// Delegate to shared tessellation
	std::vector<DxfPoint> pts =
	    GeometryUtils::tessellateBulgeArc(p0, p1, bulge,
	        ConversionEngine::defaultBulgeTolerance());

	for (size_t i = 1; i < pts.size(); ++i) {
		m_data.addPolylineSegment(
		    DxfPolylineSegment(pts[i - 1], pts[i], 0.0));
	}
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
	         << "polySegs=" << outData.polylineSegments().size();
	if (outData.lines().empty() && outData.circles().empty() && outData.polylineSegments().empty())
	{
		outData.setErrorMessage(
			QStringLiteral("DXF was read successfully, but no supported entities were found."));
		return false;
	}
	return true;
}

#include "DxfParser.h"


int calculateSegmentCount(double radius, double sweep, double tolerance) {
	if (sweep <= 0.0 || radius <= 0.0) return 1; // 无效或零扫描，至少1段
	if (tolerance <= 0.0) tolerance = 1e-6;
	// 防止 tolerance 过大导致计算错误
	if (tolerance >= radius) return 2; // 至少分两段画一条劣弧
	double halfAngle = acos(1.0 - tolerance / radius);
	double anglePerSegment = 2.0 * halfAngle;
	// 限制最大角度，避免 sweep 非常大时出现过长的线段（可选）
	const double maxAngle = M_PI / 4.0; // 45度
	if (anglePerSegment > maxAngle) anglePerSegment = maxAngle;
	int segments = static_cast<int>(ceil(sweep / anglePerSegment));
	// 确保至少2段，因为至少需要两个点构成弧线
	if (segments < 2) segments = 2;
	return segments;
}
void DxfReader::addLine(const DRW_Line& data) {
	DxfLine line;
	line.start = Point3D(data.basePoint.x,data.basePoint.y,data.basePoint.z);
	line.end = Point3D(data.secPoint.x, data.secPoint.y, data.secPoint.z);
	m_data.lines.push_back(line);


}
void DxfReader::addCircle(const DRW_Circle& data)
{
	DxfCircle circle;
	circle.center= Point3D(data.basePoint.x, data.basePoint.y, data.basePoint.z);
	circle.radius = data.radious;
	m_data.circles.push_back(circle);
}

void DxfReader::addLWPolyline(const DRW_LWPolyline& data)
{
	// 将轻量多段线的相邻顶点分解为带 bulge 的线段
	const int numVerts = data.vertexnum;
	if (numVerts < 2) return;
	const bool isClosed = (data.flags & 1) != 0;
	for (int i = 0; i < numVerts - 1; ++i) {
		const DRW_Vertex2D& v1 = *data.vertlist[i];
		const DRW_Vertex2D& v2 = *data.vertlist[i + 1];
		DxfPolylineSegment seg;
		seg.start = Point3D(v1.x, v1.y, 0.0);
		seg.end   = Point3D(v2.x, v2.y, 0.0);
		seg.bulge = v1.bulge;  // v1.bulge 描述 v1→v2 这段
		m_data.polylineSegments.push_back(seg);
	}
	// 闭合多段线：连接首尾
	if (isClosed) {
		const DRW_Vertex2D& vFirst = *data.vertlist.front();
		const DRW_Vertex2D& vLast  = *data.vertlist.back();
		DxfPolylineSegment seg;
		seg.start = Point3D(vLast.x, vLast.y, 0.0);
		seg.end   = Point3D(vFirst.x, vFirst.y, 0.0);
		seg.bulge = vLast.bulge;  // vLast.bulge 描述最后一段
		m_data.polylineSegments.push_back(seg);
	}
}

void DxfReader::addArc(const DRW_Arc& data) {
	const DRW_Coord center = data.basePoint;
	const double radius = data.radious;

	if (!std::isfinite(radius) || radius <= 0.0) return;

	double start = data.staangle;
	double end = data.endangle;

	double sweep = end - start;
	if (data.isccw) {
		while (sweep <= 0.0) { sweep += 2.0 * M_PI; }
	} else {
		while (sweep >= 0.0) { sweep -= 2.0 * M_PI; }
	}

	const int segments = calculateSegmentCount(
		radius, std::abs(sweep), 0.01);

	auto pointAt = [&](double angle) -> Point3D {
		return Point3D(
			center.x + radius * std::cos(angle),
			center.y + radius * std::sin(angle),
			center.z);
	};

	Point3D previous = pointAt(start);
	for (int i = 1; i <= segments; ++i) {
		double angle = start + sweep * static_cast<double>(i)
		               / static_cast<double>(segments);
		Point3D current = pointAt(angle);
		DxfPolylineSegment seg;
		seg.start = previous;
		seg.end = current;
		seg.bulge = 0.0;
		m_data.polylineSegments.push_back(seg);
		previous = current;
	}
}
bool DxfParser::parseFile(const QString& filePath, DxfData& outData) {
	outData = DxfData();
	if (filePath.isEmpty()) {
		outData.errorMessage = QStringLiteral("DXF file is empty");
		return false;
	}
	const QByteArray pathBytes = filePath.toLocal8Bit();
	dxfRW dxf(pathBytes.constData());
	DxfReader reader;
	if (!dxf.read(&reader, true)) {
	outData.errorMessage =
		QStringLiteral("Failed to read DXF file. Error code: %1")
		.arg(static_cast<int>(dxf.getError()));
	return false;
	}
	outData = reader.m_data;
	outData.isvaild = true;
	if (outData.lines.empty() && outData.circles.empty() && outData.polylineSegments.empty())
	{
		outData.errorMessage =
			QStringLiteral("DXF was read successfully, but no supported entities were found.");
		return false;
	}
	return true;
}
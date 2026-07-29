#include "DxfParser.h"
#include "DebugLog.h"

#include <QDebug>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

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
<<<<<<< HEAD
	// 验证长轴长度
=======
	const DRW_Coord center = data.basePoint;
>>>>>>> 8b27896 (更新：修复编译配置、添加spline支持、完善日志输出)
	const double majorX = data.secPoint.x;
	const double majorY = data.secPoint.y;
	const double majorLen = std::sqrt(majorX * majorX + majorY * majorY);

	if (majorLen <= 0.0) return;
	if (!std::isfinite(data.ratio) || data.ratio <= 0.0) return;

<<<<<<< HEAD
	DxfPoint center(data.basePoint.x, data.basePoint.y, data.basePoint.z);
	DxfPoint majorAxisEnd(data.secPoint.x, data.secPoint.y, data.secPoint.z);

	m_data.addEllipse(DxfEllipse(center, majorAxisEnd, data.ratio,
	                              data.staparam, data.endparam, data.isccw));
=======
	const double minorX = -majorY * data.ratio;
	const double minorY =  majorX * data.ratio;

	double startParam = data.staparam;
	double endParam   = data.endparam;

	double sweep = endParam - startParam;
	if (data.isccw) {
		while (sweep <= 0.0) { sweep += 2.0 * M_PI; }
	} else {
		while (sweep >= 0.0) { sweep -= 2.0 * M_PI; }
	}

	const double maxRadius = (majorLen > majorLen * data.ratio)
		? majorLen : majorLen * data.ratio;
	const int segments = calculateSegmentCount(
		maxRadius, std::abs(sweep), 0.01);

	auto pointAt = [&](double t) -> DxfPoint {
		return DxfPoint(
			center.x + majorX * std::cos(t) + minorX * std::sin(t),
			center.y + majorY * std::cos(t) + minorY * std::sin(t),
			center.z);
	};

	DxfPoint previous = pointAt(startParam);
	for (int i = 1; i <= segments; ++i) {
		double t = startParam + sweep * static_cast<double>(i)
		           / static_cast<double>(segments);
		DxfPoint current = pointAt(t);
		DxfPolylineSegment seg(previous, current, 0.0);
		m_data.addPolylineSegment(seg);
		previous = current;
	}
>>>>>>> 8b27896 (更新：修复编译配置、添加spline支持、完善日志输出)
}

void DxfReader::addLWPolyline(const DRW_LWPolyline& data)
{
<<<<<<< HEAD
	const int numVerts = std::min(data.vertexnum, static_cast<int>(data.vertlist.size()));
=======
	const int numVerts = data.vertexnum;
>>>>>>> 8b27896 (更新：修复编译配置、添加spline支持、完善日志输出)
	qDebug() << "[DxfReader] addLWPolyline vertexnum=" << numVerts
	         << "flags=" << data.flags << "vertlist=" << (int)data.vertlist.size();
	if (numVerts < 2) return;

	const bool isClosed = (data.flags & 1) != 0;
<<<<<<< HEAD

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
=======
	for (int i = 0; i < numVerts - 1; ++i) {
		const DRW_Vertex2D& v1 = *data.vertlist[i];
		const DRW_Vertex2D& v2 = *data.vertlist[i + 1];
		DxfPolylineSegment seg;
		seg.start = DxfPoint(v1.x, v1.y, 0.0);
		seg.end   = DxfPoint(v2.x, v2.y, 0.0);
		seg.bulge = v1.bulge;
		m_data.addPolylineSegment(seg);
	}
	if (isClosed) {
		const DRW_Vertex2D& vFirst = *data.vertlist.front();
		const DRW_Vertex2D& vLast  = *data.vertlist.back();
		DxfPolylineSegment seg;
		seg.start = DxfPoint(vLast.x, vLast.y, 0.0);
		seg.end   = DxfPoint(vFirst.x, vFirst.y, 0.0);
		seg.bulge = vLast.bulge;
		m_data.addPolylineSegment(seg);
>>>>>>> 8b27896 (更新：修复编译配置、添加spline支持、完善日志输出)
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

<<<<<<< HEAD
=======
// -----------------------------------------------
// SPLINE support: DXF B-spline/NURBS → 自适应折线
// -----------------------------------------------
namespace {

struct SplinePoint3
{
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

struct SplinePoint4
{
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
	double w = 0.0;
};

bool evaluateSpline(
	const std::vector<SplinePoint3>& controlPoints,
	const std::vector<double>& weights,
	const std::vector<double>& knots,
	int degree,
	double parameter,
	SplinePoint3& result)
{
	const int numCtrl = static_cast<int>(controlPoints.size());
	const int lastCtrl = numCtrl - 1;
	if (degree < 1 || lastCtrl < degree
	    || static_cast<int>(knots.size()) != numCtrl + degree + 1
	    || static_cast<int>(weights.size()) != numCtrl) {
		return false;
	}

	const double firstParameter = knots[degree];
	const double lastParameter = knots[numCtrl];
	if (parameter < firstParameter || parameter > lastParameter) {
		return false;
	}

	int span = degree;
	if (parameter >= lastParameter) {
		span = lastCtrl;
	} else if (parameter > firstParameter) {
		int low = degree;
		int high = numCtrl;
		while (high - low > 1) {
			const int middle = low + (high - low) / 2;
			if (parameter < knots[middle])
				high = middle;
			else
				low = middle;
		}
		span = low;
	}

	std::vector<SplinePoint4> work(static_cast<std::size_t>(degree + 1));
	for (int j = 0; j <= degree; ++j) {
		const int index = span - degree + j;
		const double weight = weights[index];
		work[j] = {
			controlPoints[index].x * weight,
			controlPoints[index].y * weight,
			controlPoints[index].z * weight,
			weight
		};
	}

	for (int level = 1; level <= degree; ++level) {
		for (int j = degree; j >= level; --j) {
			const int knotIndex = span - degree + j;
			const double denominator =
				knots[knotIndex + degree - level + 1] - knots[knotIndex];
			const double alpha = std::abs(denominator) <= 1.0e-15
				? 0.0
				: (parameter - knots[knotIndex]) / denominator;
			const double oneMinusAlpha = 1.0 - alpha;
			work[j].x = oneMinusAlpha * work[j - 1].x + alpha * work[j].x;
			work[j].y = oneMinusAlpha * work[j - 1].y + alpha * work[j].y;
			work[j].z = oneMinusAlpha * work[j - 1].z + alpha * work[j].z;
			work[j].w = oneMinusAlpha * work[j - 1].w + alpha * work[j].w;
		}
	}

	const SplinePoint4& homogeneous = work[degree];
	if (!std::isfinite(homogeneous.w) || std::abs(homogeneous.w) <= 1.0e-15)
		return false;

	result = {
		homogeneous.x / homogeneous.w,
		homogeneous.y / homogeneous.w,
		homogeneous.z / homogeneous.w
	};
	return std::isfinite(result.x) && std::isfinite(result.y)
	    && std::isfinite(result.z);
}

double squaredDistance(const SplinePoint3& a, const SplinePoint3& b)
{
	const double dx = a.x - b.x;
	const double dy = a.y - b.y;
	const double dz = a.z - b.z;
	return dx * dx + dy * dy + dz * dz;
}

double distanceToSegment(
	const SplinePoint3& point,
	const SplinePoint3& start,
	const SplinePoint3& end)
{
	const double vx = end.x - start.x;
	const double vy = end.y - start.y;
	const double vz = end.z - start.z;
	const double lengthSquared = vx * vx + vy * vy + vz * vz;
	if (lengthSquared <= 1.0e-30)
		return std::sqrt(squaredDistance(point, start));

	const double wx = point.x - start.x;
	const double wy = point.y - start.y;
	const double wz = point.z - start.z;
	const double projection = std::max(
		0.0, std::min(1.0, (wx * vx + wy * vy + wz * vz) / lengthSquared));
	const SplinePoint3 closest {
		start.x + projection * vx,
		start.y + projection * vy,
		start.z + projection * vz
	};
	return std::sqrt(squaredDistance(point, closest));
}

} // namespace

void DxfReader::addSpline(const DRW_Spline* data)
{
	if (!data || data->ncontrol < 2 || data->degree < 1)
		return;

	const int numCtrl = data->ncontrol;
	const int degree = data->degree;
	if (numCtrl <= degree
	    || static_cast<int>(data->controllist.size()) < numCtrl) {
		qDebug() << "[Spline] ERROR: invalid control point count";
		return;
	}

	const bool isRational = (data->flags & 4) != 0;
	const bool isPeriodic = (data->flags & 2) != 0;
	const bool isClosed = (data->flags & 1) != 0;
	const std::vector<double>& knots = data->knotslist;

	qDebug() << "[Spline] degree=" << degree
	         << "ncontrol=" << numCtrl
	         << "nknots=" << knots.size()
	         << "isRational=" << isRational
	         << "isPeriodic=" << isPeriodic
	         << "isClosed=" << isClosed;

	const int expectedKnotCount = numCtrl + degree + 1;
	if (static_cast<int>(knots.size()) != expectedKnotCount) {
		qDebug() << "[Spline] ERROR: knot count=" << knots.size()
		         << "expected=" << expectedKnotCount;
		return;
	}

	for (int i = 0; i < expectedKnotCount; ++i) {
		if (!std::isfinite(knots[i])
		    || (i > 0 && knots[i] < knots[i - 1])) {
			qDebug() << "[Spline] ERROR: invalid knot at index" << i;
			return;
		}
	}

	std::vector<SplinePoint3> controlPoints;
	controlPoints.reserve(static_cast<std::size_t>(numCtrl));
	for (int i = 0; i < numCtrl; ++i) {
		const auto& point = data->controllist[i];
		if (!point || !std::isfinite(point->x)
		    || !std::isfinite(point->y) || !std::isfinite(point->z)) {
			qDebug() << "[Spline] ERROR: invalid control point at index" << i;
			return;
		}
		controlPoints.push_back({point->x, point->y, point->z});
	}

	std::vector<double> weights(static_cast<std::size_t>(numCtrl), 1.0);
	if (isRational) {
		if (static_cast<int>(data->weightlist.size()) < numCtrl) {
			qDebug() << "[Spline] ERROR: rational spline has insufficient weights";
			return;
		}
		for (int i = 0; i < numCtrl; ++i) {
			const double weight = data->weightlist[i];
			if (!std::isfinite(weight) || weight <= 0.0) {
				qDebug() << "[Spline] ERROR: invalid weight at index" << i;
				return;
			}
			weights[i] = weight;
		}
	}

	const double firstParameter = knots[degree];
	const double lastParameter = knots[numCtrl];
	if (!std::isfinite(firstParameter) || !std::isfinite(lastParameter)
	    || lastParameter <= firstParameter) {
		qDebug() << "[Spline] ERROR: empty parameter domain";
		return;
	}

	constexpr double tolerance = 0.01;
	constexpr int maxDepth = 20;
	constexpr std::size_t maxSegments = 2000;
	std::vector<SplinePoint3> sampledPoints;
	sampledPoints.reserve(256);

	SplinePoint3 firstPoint;
	if (!evaluateSpline(
			controlPoints, weights, knots, degree, firstParameter, firstPoint)) {
		qDebug() << "[Spline] ERROR: failed to evaluate first point";
		return;
	}
	sampledPoints.push_back(firstPoint);

	bool evaluationFailed = false;
	bool segmentLimitReached = false;
	std::function<void(double, const SplinePoint3&, double,
	                   const SplinePoint3&, int)> subdivide;
	subdivide = [&](double t0, const SplinePoint3& p0,
	                double t1, const SplinePoint3& p1, int depth) {
		if (evaluationFailed || segmentLimitReached)
			return;
		if (sampledPoints.size() >= maxSegments + 1) {
			segmentLimitReached = true;
			return;
		}

		const double quarter = t0 + (t1 - t0) * 0.25;
		const double middle = t0 + (t1 - t0) * 0.5;
		const double threeQuarter = t0 + (t1 - t0) * 0.75;
		SplinePoint3 q1;
		SplinePoint3 q2;
		SplinePoint3 q3;
		if (!evaluateSpline(controlPoints, weights, knots, degree, quarter, q1)
		    || !evaluateSpline(controlPoints, weights, knots, degree, middle, q2)
		    || !evaluateSpline(
			    controlPoints, weights, knots, degree, threeQuarter, q3)) {
			evaluationFailed = true;
			return;
		}

		const double deviation = std::max({
			distanceToSegment(q1, p0, p1),
			distanceToSegment(q2, p0, p1),
			distanceToSegment(q3, p0, p1)
		});
		if (deviation <= tolerance || depth >= maxDepth) {
			sampledPoints.push_back(p1);
			return;
		}

		subdivide(t0, p0, middle, q2, depth + 1);
		subdivide(middle, q2, t1, p1, depth + 1);
	};

	// 从每个非零节点区间分别开始细分，可保留节点处的连续性变化。
	for (int span = degree; span < numCtrl; ++span) {
		const double t0 = knots[span];
		const double t1 = knots[span + 1];
		if (t1 <= t0)
			continue;

		SplinePoint3 p0;
		SplinePoint3 p1;
		if (!evaluateSpline(controlPoints, weights, knots, degree, t0, p0)
		    || !evaluateSpline(controlPoints, weights, knots, degree, t1, p1)) {
			evaluationFailed = true;
			break;
		}
		if (squaredDistance(sampledPoints.back(), p0) > 1.0e-20)
			sampledPoints.push_back(p0);
		subdivide(t0, p0, t1, p1, 0);
		if (evaluationFailed || segmentLimitReached)
			break;
	}

	if (evaluationFailed || sampledPoints.size() < 2) {
		qDebug() << "[Spline] ERROR: adaptive discretization failed";
		return;
	}
	if (segmentLimitReached)
		qDebug() << "[Spline] WARNING: segment limit reached:" << maxSegments;

	std::size_t addedSegments = 0;
	for (std::size_t i = 1; i < sampledPoints.size(); ++i) {
		const SplinePoint3& previous = sampledPoints[i - 1];
		const SplinePoint3& current = sampledPoints[i];
		if (squaredDistance(previous, current) <= 1.0e-20)
			continue;
		m_data.addPolylineSegment(DxfPolylineSegment(
			DxfPoint(previous.x, previous.y, previous.z),
			DxfPoint(current.x, current.y, current.z), 0.0));
		++addedSegments;
	}

	if (isClosed && addedSegments < maxSegments
	    && squaredDistance(sampledPoints.back(), sampledPoints.front()) > 1.0e-20) {
		const SplinePoint3& last = sampledPoints.back();
		const SplinePoint3& first = sampledPoints.front();
		m_data.addPolylineSegment(DxfPolylineSegment(
			DxfPoint(last.x, last.y, last.z),
			DxfPoint(first.x, first.y, first.z), 0.0));
		++addedSegments;
	}

	qDebug() << "[Spline] De Boor discretization OK:"
	         << "points=" << sampledPoints.size()
	         << "segments=" << addedSegments;
}

>>>>>>> 8b27896 (更新：修复编译配置、添加spline支持、完善日志输出)
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
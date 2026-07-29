#include "DxfParser.h"
#include "DebugLog.h"

#include <Geom_BSplineCurve.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <CPnts_AbscissaPoint.hxx>
#include <GCPnts_TangentialDeflection.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColStd_HArray1OfReal.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColgp_Array1OfVec.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <Precision.hxx>

#include "libdxfrw.h"
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {

// ========================================================================
//  Spline discretization helpers (OCCT-based)
// ========================================================================

// -----------------------------------------------------------------
//  Lightweight 3D point used as internal interchange between
//  OCCT discretization and the rest of the pipeline.
// -----------------------------------------------------------------
struct SplinePoint3
{
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

// -----------------------------------------------------------------
//  Chord-height adaptive discretization
// -----------------------------------------------------------------
bool discretizeByDeflection(
	const Handle(Geom_BSplineCurve)& curve,
	std::vector<SplinePoint3>& sampledPoints,
	QDebug& log)
{
	constexpr double linearDeflection  = 0.01;
	constexpr double angularDeflection = 0.10; // radians, ~5.7 degrees
	constexpr int    minimumPoints = 2;

	try {
		GeomAdaptor_Curve adaptor(curve);

		GCPnts_TangentialDeflection discretizer(
			adaptor,
			curve->FirstParameter(),
			curve->LastParameter(),
			angularDeflection,
			linearDeflection,
			minimumPoints,
			1.0e-9,
			1.0e-7);

		const Standard_Integer nbPoints = discretizer.NbPoints();
		if (nbPoints < 2) {
			log << "[Spline/Deflection] ERROR: too few points:"
			    << nbPoints;
			return false;
		}

		sampledPoints.reserve(static_cast<std::size_t>(nbPoints));
		for (Standard_Integer i = 1; i <= nbPoints; ++i) {
			const gp_Pnt p = discretizer.Value(i);
			sampledPoints.push_back({p.X(), p.Y(), p.Z()});
		}

		log << "[Spline/Deflection] linear=" << linearDeflection
		    << "angular=" << angularDeflection
		    << "points=" << nbPoints;

		return true;
	} catch (const Standard_Failure& e) {
		log << "[Spline/Deflection] Standard_Failure:"
		    << e.GetMessageString();
		return false;
	} catch (const std::exception& e) {
		log << "[Spline/Deflection] std::exception:" << e.what();
		return false;
	} catch (...) {
		log << "[Spline/Deflection] unknown exception";
		return false;
	}
}

// -----------------------------------------------------------------
//  Build OCCT B-spline from control points, knots, and weights
// -----------------------------------------------------------------
Handle(Geom_BSplineCurve) buildCurveFromControlData(
	const DRW_Spline* data,
	QDebug& log)
{
	const int numCtrl = data->ncontrol;
	const int degree   = data->degree;
	const bool isRational = (data->flags & 4) != 0;
	const std::vector<double>& knotsRaw = data->knotslist;

	// Condense unique knots and multiplicities
	std::vector<double> uknots;
	std::vector<int>    umults;
	uknots.reserve(knotsRaw.size());
	umults.reserve(knotsRaw.size());
	for (std::size_t i = 0; i < knotsRaw.size(); ) {
		const double val = knotsRaw[i];
		int mult = 1;
		while (i + mult < knotsRaw.size()
		       && std::abs(knotsRaw[i + mult] - val) <= 1.0e-12)
			++mult;
		uknots.push_back(val);
		umults.push_back(mult);
		i += mult;
	}

	const int numUnique = static_cast<int>(uknots.size());
	if (numUnique < 2) {
		log << "[Spline/Ctrl] ERROR: too few unique knots";
		return nullptr;
	}

	// Fill OCCT 1-based arrays
	TColgp_Array1OfPnt        poles(1, numCtrl);
	TColStd_Array1OfReal      wgts(1, numCtrl);
	TColStd_Array1OfReal      knotsArr(1, numUnique);
	TColStd_Array1OfInteger   multsArr(1, numUnique);

	for (int i = 0; i < numCtrl; ++i) {
		const DRW_Coord& pt = *(data->controllist[i]);
		poles.SetValue(i + 1, gp_Pnt(pt.x, pt.y, pt.z));
		wgts.SetValue(i + 1, isRational ? data->weightlist[i] : 1.0);
	}
	for (int i = 0; i < numUnique; ++i) {
		knotsArr.SetValue(i + 1, uknots[i]);
		multsArr.SetValue(i + 1, umults[i]);
	}

	Handle(Geom_BSplineCurve) curve;
	if (isRational) {
		curve = new Geom_BSplineCurve(poles, wgts, knotsArr, multsArr,
		                              degree, Standard_False);
	} else {
		curve = new Geom_BSplineCurve(poles, knotsArr, multsArr,
		                              degree, Standard_False);
	}

	// Periodic conversion
	const bool dxfPeriodic = (data->flags & 2) != 0;
	if (dxfPeriodic) {
		if (!curve->IsClosed()) {
			log << "[Spline/Ctrl] WARNING: DXF periodic but curve not closed;"
			    << " cannot set periodic";
		} else {
			log << "[Spline/Ctrl] attempting SetPeriodic...";
			try {
				curve->SetPeriodic();
				if (curve->IsPeriodic()) {
					log << "[Spline/Ctrl] SetPeriodic OK";
				} else {
					log << "[Spline/Ctrl] WARNING: SetPeriodic returned but IsPeriodic false";
				}
			} catch (const Standard_Failure& e) {
				log << "[Spline/Ctrl] SetPeriodic failed:"
				    << e.GetMessageString();
			} catch (...) {
				log << "[Spline/Ctrl] SetPeriodic failed (unknown)";
			}
		}
	}

	return curve;
}

// -----------------------------------------------------------------
//  Build OCCT B-spline by interpolating fit points
// -----------------------------------------------------------------
Handle(Geom_BSplineCurve) buildCurveFromFitPoints(
	const DRW_Spline* data,
	QDebug& log)
{
	const bool isPeriodic = (data->flags & 2) != 0;
	const int nfit        = data->nfit;

	// Filter duplicate consecutive fit points
	std::vector<gp_Pnt> uniquePts;
	uniquePts.reserve(nfit);
	const double tolFit = std::max(Precision::Confusion(),
		std::isfinite(data->tolfit) && data->tolfit > 0.0
		? data->tolfit : 1.0e-7);

	for (int i = 0; i < nfit; ++i) {
		const auto& sp = data->fitlist[i];
		if (!sp || !std::isfinite(sp->x)
		    || !std::isfinite(sp->y) || !std::isfinite(sp->z))
			continue;
		gp_Pnt pt(sp->x, sp->y, sp->z);
		if (!uniquePts.empty()) {
			const gp_Pnt& prev = uniquePts.back();
			if (pt.Distance(prev) <= tolFit)
				continue;
		}
		uniquePts.push_back(pt);
	}

	const int actualCount = static_cast<int>(uniquePts.size());
	if (actualCount < 2) {
		log << "[Spline/Fit] ERROR: too few unique fit points:"
		    << actualCount;
		return nullptr;
	}

	if (isPeriodic && actualCount >= 2) {
		const gp_Pnt& first = uniquePts.front();
		const gp_Pnt& last  = uniquePts.back();
		if (last.Distance(first) <= tolFit) {
			uniquePts.pop_back();
			log << "[Spline/Fit] removed duplicate end fit point for periodic curve";
		}
	}

	const int finalCount = static_cast<int>(uniquePts.size());
	if (finalCount < 2) {
		log << "[Spline/Fit] ERROR: after dedup too few points:"
		    << finalCount;
		return nullptr;
	}

	try {
		Handle(TColgp_HArray1OfPnt) points =
			new TColgp_HArray1OfPnt(1, finalCount);
		for (int i = 0; i < finalCount; ++i)
			points->SetValue(i + 1, uniquePts[i]);

		GeomAPI_Interpolate interpolator(
			points,
			isPeriodic ? Standard_True : Standard_False,
			tolFit);

		if (!isPeriodic) {
			const gp_Vec tgStart(data->tgStart.x,
			                     data->tgStart.y,
			                     data->tgStart.z);
			const gp_Vec tgEnd(data->tgEnd.x,
			                   data->tgEnd.y,
			                   data->tgEnd.z);
			const double magStart = tgStart.Magnitude();
			const double magEnd   = tgEnd.Magnitude();
			if (magStart > Precision::Confusion()
			    && magEnd > Precision::Confusion()) {
				interpolator.Load(tgStart, tgEnd);
				log << "[Spline/Fit] loaded start/end tangents";
			}
		}

		interpolator.Perform();
		if (!interpolator.IsDone()) {
			log << "[Spline/Fit] ERROR: interpolation failed";
			return nullptr;
		}

		Handle(Geom_BSplineCurve) curve = interpolator.Curve();
		log << "[Spline/Fit] interpolation OK"
		    << "degree=" << curve->Degree()
		    << "periodic=" << curve->IsPeriodic();
		return curve;

	} catch (const Standard_Failure& e) {
		log << "[Spline/Fit] Standard_Failure:"
		    << e.GetMessageString();
	} catch (const std::exception& e) {
		log << "[Spline/Fit] std::exception:" << e.what();
	} catch (...) {
		log << "[Spline/Fit] unknown exception";
	}
	return nullptr;
}

// -----------------------------------------------------------------
//  Unified B-spline construction + discretization
// -----------------------------------------------------------------
bool buildSplineOCCT(
	const DRW_Spline* data,
	std::vector<SplinePoint3>& sampledPoints,
	QDebug& log)
{
	Handle(Geom_BSplineCurve) curve;

	const bool hasControlData =
		data->ncontrol > data->degree
		&& static_cast<int>(data->controllist.size()) >= data->ncontrol;
	const bool hasFitData =
		data->nfit >= 2
		&& static_cast<int>(data->fitlist.size()) >= data->nfit;

	if (hasControlData) {
		curve = buildCurveFromControlData(data, log);
		if (!curve) return false;
	} else if (hasFitData) {
		curve = buildCurveFromFitPoints(data, log);
		if (!curve) return false;
	} else {
		log << "[Spline/OCCT] ERROR: no valid control or fit data";
		return false;
	}

	// Verify curve length
	try {
		GeomAdaptor_Curve adaptor(curve);
		const double totalLength = CPnts_AbscissaPoint::Length(adaptor);
		if (!std::isfinite(totalLength) || totalLength <= Precision::Confusion()) {
			log << "[Spline/OCCT] ERROR: zero or NaN curve length";
			return false;
		}
	} catch (...) {
		log << "[Spline/OCCT] ERROR: failed to compute curve length";
		return false;
	}

	// Chord-height adaptive discretization
	if (!discretizeByDeflection(curve, sampledPoints, log))
		return false;

	// Closure verification and endpoint snapping
	const bool dxfClosed   = (data->flags & 1) != 0;
	const bool dxfPeriodic = (data->flags & 2) != 0;
	const bool occtPeriodic = curve->IsPeriodic();

	const double closureTolerance =
		std::max(Precision::Confusion(), 1.0e-7);

	if (sampledPoints.size() >= 2) {
		const SplinePoint3& first = sampledPoints.front();
		const SplinePoint3& last  = sampledPoints.back();
		const double dx = last.x - first.x;
		const double dy = last.y - first.y;
		const double dz = last.z - first.z;
		const double gap = std::sqrt(dx * dx + dy * dy + dz * dz);

		log << "[Spline/OCCT] closure:"
		    << "dxfClosed=" << dxfClosed
		    << "dxfPeriodic=" << dxfPeriodic
		    << "occtClosed=" << curve->IsClosed()
		    << "occtPeriodic=" << occtPeriodic
		    << "gap=" << gap;

		if (gap <= closureTolerance) {
			sampledPoints.back() = first;
		} else if (!occtPeriodic && (dxfClosed || dxfPeriodic)) {
			log << "[Spline/OCCT] WARNING:"
			    << "spline marked closed but endpoint gap is"
			    << gap << "; keeping open";
		}
	}

	return true;
}

// ========================================================================
//  DxfReader — DRW_Interface implementation (internal, only used here)
// ========================================================================

class DxfReader : public DRW_Interface {
public:
	DxfData m_data;

	// Implemented entity callbacks
	void addLine(const DRW_Line& data) override;
	void addCircle(const DRW_Circle& data) override;
	void addArc(const DRW_Arc& data) override;
	void addEllipse(const DRW_Ellipse& data) override;
	void addLWPolyline(const DRW_LWPolyline& data) override;
	void addSpline(const DRW_Spline* data) override;

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

// ========================================================================
//  DxfReader member functions — entity callbacks
// ========================================================================

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

void DxfReader::addSpline(const DRW_Spline* data)
{
	if (!data)
		return;

	const bool isRational = (data->flags & 4) != 0;
	const bool isPeriodic = (data->flags & 2) != 0;
	const bool isClosed   = (data->flags & 1) != 0;

	// Determine data source
	const bool hasControlData =
		data->ncontrol > 0
		&& static_cast<int>(data->controllist.size()) >= data->ncontrol
		&& data->degree >= 1
		&& data->ncontrol > data->degree;

	const bool hasFitData =
		data->nfit >= 2
		&& static_cast<int>(data->fitlist.size()) >= data->nfit;

	if (!hasControlData && !hasFitData) {
		qDebug() << "[Spline] no valid control or fit data;"
		         << "ncontrol=" << data->ncontrol
		         << "degree=" << data->degree
		         << "nfit=" << data->nfit
		         << "controllist=" << data->controllist.size()
		         << "fitlist=" << data->fitlist.size()
		         << "isRational=" << isRational
		         << "isPeriodic=" << isPeriodic
		         << "isClosed=" << isClosed;
		return;
	}

	qDebug() << "[Spline]"
	         << "degree=" << data->degree
	         << "ncontrol=" << data->ncontrol
	         << "nknots=" << data->knotslist.size()
	         << "nfit=" << data->nfit
	         << "isRational=" << isRational
	         << "isPeriodic=" << isPeriodic
	         << "isClosed=" << isClosed;

	// Validate control data if that path will be taken
	if (hasControlData) {
		const int numCtrl = data->ncontrol;
		const int degree  = data->degree;
		const std::vector<double>& knots = data->knotslist;
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
		for (int i = 0; i < numCtrl; ++i) {
			const auto& point = data->controllist[i];
			if (!point || !std::isfinite(point->x)
			    || !std::isfinite(point->y) || !std::isfinite(point->z)) {
				qDebug() << "[Spline] ERROR: invalid control point at index" << i;
				return;
			}
		}
		if (isRational) {
			if (static_cast<int>(data->weightlist.size()) < numCtrl) {
				qDebug() << "[Spline] ERROR: rational spline has insufficient weights";
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

	// --- OCCT curve construction and discretization ---
	std::vector<SplinePoint3> sampledPoints;
	if (!buildSplineOCCT(data, sampledPoints, qDebug())) {
		qDebug() << "[Spline] ERROR: OCCT construction or discretization failed, skipping spline";
		return;
	}
	if (sampledPoints.size() < 2) {
		qDebug() << "[Spline] ERROR: too few sampled points (" << sampledPoints.size() << "), skipping spline";
		return;
	}

	qDebug() << "[Spline] OCCT discretization OK:"
	         << "points=" << sampledPoints.size();

	// --- convert sampled points to line segments ---
	std::size_t addedSegments = 0;
	for (std::size_t i = 1; i < sampledPoints.size(); ++i) {
		const SplinePoint3& previous = sampledPoints[i - 1];
		const SplinePoint3& current = sampledPoints[i];
		const double dx = current.x - previous.x;
		const double dy = current.y - previous.y;
		const double dz = current.z - previous.z;
		if (dx * dx + dy * dy + dz * dz <= 1.0e-20)
			continue;
		m_data.addLine(DxfLine(
			DxfPoint(previous.x, previous.y, previous.z),
			DxfPoint(current.x, current.y, current.z)));
		++addedSegments;
	}

	qDebug() << "[Spline] total segments added:" << addedSegments;
}

}  // namespace

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
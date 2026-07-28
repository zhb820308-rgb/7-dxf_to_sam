#include "ConversionEngine.h"
#include <QDebug>
#include <cmath>

bool ConversionEngine::convert(const DxfData& dxfData,
                               double baseX, double baseY, double baseZ,
                               SamData& outData)
{
    outData.clear();

    // --- points ---
    for (const DxfPoint& pt : dxfData.points()) {
        if (!pt.isValid())
            continue;
        outData.addPoint(translate(pt, baseX, baseY, baseZ));
    }

    // --- lines ---
    for (const DxfLine& line : dxfData.lines()) {
        if (!line.isValid())
            continue;
        DxfPoint s = translate(line.start(), baseX, baseY, baseZ);
        DxfPoint e = translate(line.end(),   baseX, baseY, baseZ);
        outData.addLine(DxfLine(s, e));
    }

    // --- circles ---
    for (const DxfCircle& circle : dxfData.circles()) {
        if (!circle.isValid())
            continue;
        DxfPoint c = translate(circle.center(), baseX, baseY, baseZ);
        outData.addCircle(DxfCircle(c, circle.radius()));
    }

    // --- polyline segments (pre-tessellated by parser) ---
    for (const DxfPolylineSegment& seg : dxfData.polylineSegments()) {
        DxfPoint s = translate(seg.start, baseX, baseY, baseZ);
        DxfPoint e = translate(seg.end,   baseX, baseY, baseZ);
        if (s.x() == e.x() && s.y() == e.y() && s.z() == e.z())
            continue;
        outData.addLine(DxfLine(s, e));
    }

    bool ok = !outData.points().empty() ||
              !outData.lines().empty()  ||
              !outData.circles().empty();

    qDebug() << "[ConversionEngine] done:"
             << "points=" << outData.points().size()
             << "lines="  << outData.lines().size()
             << "circles="<< outData.circles().size();

    return ok;
}

DxfPoint ConversionEngine::translate(const DxfPoint& pt,
                                      double bx, double by, double bz)
{
    return DxfPoint(pt.x() + bx,
                    pt.y() + by,
                    pt.z() + bz);
}

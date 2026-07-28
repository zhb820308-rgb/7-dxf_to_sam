#ifndef DxfData_h
#define DxfData_h

#include <vector>
#include <QString>

struct Point3D {
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;

	Point3D() = default;
	Point3D(double ix, double iy, double iz):x(ix), y(iy), z(iz) {}//xyz默认为0

};
struct DxfLine {
	Point3D start;
	Point3D end;//线数据类型
};
struct DxfCircle {
	Point3D center;
	double radius = 0.0;//园
};
struct DxfPolylineSegment {
	Point3D start;
	Point3D end;
	double bulge = 0.0;// 0 = 直线，非0 = 圆弧段
};
struct DxfData {//dxf解析要用的数据结构，先写线和园
	std::vector<DxfLine>lines;
	std::vector<DxfCircle>circles;
	std::vector<DxfPolylineSegment>polylineSegments;
	QString errorMessage;
	bool isvaild = false;

	int entityCount()const {
		return (int)lines.size() + (int)circles.size() + (int)polylineSegments.size();
	}


};
#endif // !DxfData_h


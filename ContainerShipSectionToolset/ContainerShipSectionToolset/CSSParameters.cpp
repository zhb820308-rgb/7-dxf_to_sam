#include "CSSParameters.h"

#include <QSettings>
#include <QtGlobal>
#include <QtMath>
#include <algorithm>

CSSParameters::CSSParameters(QObject* parent_ptr): QObject(parent_ptr)
{ }

CSSParameters::~CSSParameters()
{ }

// Outer Hull 外壳参数
double CSSParameters::getHalfBreadth() const
{
    return half_breadth;
}

bool CSSParameters::setHalfBreadth(double value)
{
    if(value <= 0)
    {
        return false;
    }
    half_breadth = value;
    return true;
}

QString CSSParameters::getHalfBreadthDesc() const
{
    return tr("Half Breadth must be a positive number");
}

double CSSParameters::getDepth() const
{
    return depth;
}

bool CSSParameters::setDepth(double value)
{
    if(value <= 0)
    {
        return false;
    }
    depth = value;
    return true;
}

QString CSSParameters::getDepthDesc() const
{
    return tr("Depth must be a positive number");
}

double CSSParameters::getBilgeRadius() const
{
    return bilge_radius;
}

bool CSSParameters::setBilgeRadius(double value)
{
    if(value <= 0 || value >= half_breadth || value >= depth)
    {
        return false;
    }
    bilge_radius = value;
    return true;
}

QString CSSParameters::getBilgeRadiusDesc() const
{
    return tr("Bilge Radius must be a positive number and less than Half Breadth and Depth");
}

double CSSParameters::getDeckCornerRadius() const
{
    return deck_corner_radius;
}

bool CSSParameters::setDeckCornerRadius(double value)
{
    if(!qFuzzyIsNull(value))
    {
        return false;
    }
    deck_corner_radius = value;
    return true;
}

QString CSSParameters::getDeckCornerRadiusDesc() const
{
    return tr("Deck Corner Radius must be 0");
}

// Camber 外倾角
double CSSParameters::getCamberHeight() const
{
    return camber_height;
}

bool CSSParameters::setCamberHeight(double value)
{
    if(!qFuzzyIsNull(value))
    {
        return false;
    }
    camber_height = value;
    return true;
}

QString CSSParameters::getCamberHeightDesc() const
{
    return tr("Camber Height must be 0");
}

double CSSParameters::getDeckDistance() const
{
    return deck_distance;
}

bool CSSParameters::setDeckDistance(double value)
{
    if(!qFuzzyCompare(value, half_breadth))
    {
        return false;
    }
    deck_distance = value;
    return true;
}

QString CSSParameters::getDeckDistanceDesc() const
{
    return tr("Deck Distance must be equal to Half Breadth");
}

// Inner Hull 内壳参数
double CSSParameters::getInnerHullHeight() const
{
    return inner_hull_height;
}

bool CSSParameters::setInnerHullHeight(double value)
{
    // FIXME: 内壳角点在圆弧外
    if(value >= depth || value < (half_breadth / 10.0) || value < 760.0 || value > 2000.0)
    {
        return false;
    }
    inner_hull_height = value;
    return true;
}

QString CSSParameters::getInnerHullHeightDesc() const
{
    return tr("Inner Hull Height must be between 760 and 2000 mm, and greater than 1/10 of Half Breadth");
}

double CSSParameters::getInnerHullDistance() const
{
    return inner_hull_distance;
}

bool CSSParameters::setInnerHullDistance(double value)
{
    // FIXME: 内壳角点在圆弧外
    if(value <= 0 || value >= half_breadth)
    {
        return false;
    }
    inner_hull_distance = value;
    return true;
}

QString CSSParameters::getInnerHullDistanceDesc() const
{
    return tr("Inner Hull Distance must be between 0 and Half Breadth");
}

uint CSSParameters::getGriderMinimumCnt() const
{
    return qMax<uint>(4, qCeil(inner_hull_distance / 3500.0) + 1);
}

// Grider 双层底纵桁
QVector<double> CSSParameters::getGriderDistances() const
{
    return grider_distances;
}

QVector<bool> CSSParameters::setGriderDistances(const QVector<double>& values)
{
    int values_size = values.size();
    QVector<bool> results(values_size, true);
    if(values_size < 4)
    {
        results.fill(false);
        return results;
    }
    if(!qFuzzyIsNull(values.front()))
    {
        results.front() = false;
    }
    for(int i = 1; i < values_size; ++i)
    {
        if(values.at(i) - values.at(i - 1) <= 0.0 || values.at(i) - values.at(i - 1) > 3500.0)
        {
            results[i] = false;
        }
    }
    if(!qFuzzyCompare(values.back(), inner_hull_distance))
    {
        results.back() = false;
    }
    grider_distances = values;
    return results;
}

QString CSSParameters::getGriderDistancesDesc() const
{
    return tr("Grider Distances must meet the following requirements:\n"
              "- First value must be 0\n"
              "- Last value must match Inner Hull Distance\n"
              "- Values must be in ascending order\n"
              "- Distance between adjacent values must be less than 3500 mm");
}

uint CSSParameters::getStringerMinimumCnt() const
{
    return 2;
}

// Stringer 双壳纵桁参数
QVector<double> CSSParameters::getStringerDistances() const
{
    return stringer_distances;
}

QVector<bool> CSSParameters::setStringerDistances(const QVector<double>& values)
{
    int values_size = values.size();
    QVector<bool> results(values_size, true);
    if(values_size < 2)
    {
        results.fill(false);
        return results;
    }
    if(!qFuzzyCompare(values.front(), inner_hull_height))
    {
        results.front() = false;
    }
    for(int i = 1; i < values_size; ++i)
    {
        if(values.at(i) - values.at(i - 1) <= 0.0)
        {
            results[i] = false;
        }
    }
    if(!qFuzzyCompare(values.back(), depth))
    {
        results.back() = false;
    }
    stringer_distances = values;
    return results;
}

QString CSSParameters::getStringerDistancesDesc() const
{
    return tr("Stringer Distances must meet the following requirements:\n"
              "- First value must match Inner Hull Height\n"
              "- Last value must match Depth\n"
              "- Values must be in ascending order");
}

// Hatch Coaming 舱口围参数
double CSSParameters::getHatchCoamingHeight() const
{
    return hatch_coaming_height;
}

bool CSSParameters::setHatchCoamingHeight(double value)
{
    if(value <= 0)
    {
        return false;
    }
    hatch_coaming_height = value;
    return true;
}

QString CSSParameters::getHatchCoamingHeightDesc() const
{
    return tr("Hatch Coaming Height must be a positive number");
}

double CSSParameters::getHatchCoamingWidth() const
{
    return hatch_coaming_width;
}

bool CSSParameters::setHatchCoamingWidth(double value)
{
    if(value <= 0 || value > half_breadth - inner_hull_distance)
    {
        return false;
    }
    hatch_coaming_width = value;
    return true;
}

QString CSSParameters::getHatchCoamingWidthDesc() const
{
    return tr("Hatch Coaming Width must be a positive number"
              " and less than Half Breadth minus Inner Hull Distance");
}

// Mesh Option 网格选项
bool CSSParameters::needMirrorSection() const
{
    return need_mirror_section;
}

bool CSSParameters::setMirrorSection(bool value)
{
    need_mirror_section = value;
    return true;
}

bool CSSParameters::needExtrudeSection() const
{
    return need_extrude_section;
}

bool CSSParameters::setExtrudeSection(bool value)
{
    need_extrude_section = value;
    return true;
}

double CSSParameters::getExtrudeDirection() const
{
    return extrude_direction;
}

bool CSSParameters::setExtrudeDirection(double value)
{
    if(qFuzzyIsNull(value))
    {
        return false;
    }
    extrude_direction = value;
    return true;
}

int CSSParameters::getExtrudeCnt() const
{
    return extrude_cnt;
}

bool CSSParameters::setExtrudeCnt(int value)
{

    if(value <= 0)
    {
        return false;
    }
    extrude_cnt = value;
    return true;
}

bool CSSParameters::needRefineSection() const
{
    return need_refine_section;
}

bool CSSParameters::setRefineSection(bool value)
{
    need_refine_section = value;
    return true;
}

bool CSSParameters::needStiffener() const
{
    return need_stiffener;
}

bool CSSParameters::setNeedStiffener(bool value)
{
    need_stiffener = value;
    return true;
}

// File 文件参数
QString CSSParameters::getExportFilePath() const
{
    return export_file_path;
}

bool CSSParameters::setExportFilePath(const QString& file_path)
{
    if(file_path.isEmpty())
    {
        return false;
    }
    export_file_path = file_path;
    return true;
}

bool CSSParameters::importFromFile(const QString& file_name)
{
    QSettings settings(file_name, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    half_breadth = settings.value("outer_hull/half_breadth", -1.0).toDouble();
    depth = settings.value("outer_hull/depth", -1.0).toDouble();
    bilge_radius = settings.value("outer_hull/bilge_radius", -1.0).toDouble();
    deck_corner_radius = settings.value("outer_hull/deck_corner_radius", -1.0).toDouble();
    camber_height = settings.value("camber/camber_height", -1.0).toDouble();
    deck_distance = settings.value("camber/deck_distance", -1.0).toDouble();
    inner_hull_height = settings.value("inner_hull/inner_hull_height", -1.0).toDouble();
    inner_hull_distance = settings.value("inner_hull/inner_hull_distance", -1.0).toDouble();
    int size = settings.beginReadArray("grider");
    grider_distances.clear();
    for(int i = 0; i < size; ++i)
    {
        settings.setArrayIndex(i);
        grider_distances.append(settings.value("distance", -1.0).toDouble());
    }
    settings.endArray();
    size = settings.beginReadArray("stringer");
    stringer_distances.clear();
    for(int i = 0; i < size; ++i)
    {
        settings.setArrayIndex(i);
        stringer_distances.append(settings.value("distance", -1.0).toDouble());
    }
    settings.endArray();
    hatch_coaming_height
        = settings.value("hatch_coaming/hatch_coaming_height", -1.0).toDouble();
    hatch_coaming_width
        = settings.value("hatch_coaming/hatch_coaming_width", -1.0).toDouble();
    need_mirror_section = settings.value("mesh_option/need_mirror_section", true).toBool();
    need_extrude_section = settings.value("mesh_option/need_extrude_section", true).toBool();
    extrude_direction = settings.value("mesh_option/extrude_direction", 0.0).toDouble();
    extrude_cnt = settings.value("mesh_option/extrude_cnt", -1).toInt();
    need_refine_section = settings.value("mesh_option/need_refine_section", true).toBool();
    need_stiffener = settings.value("mesh_option/need_stiffener", true).toBool();
    export_file_path = settings.value("file/export_file_path", QString()).toString();
    return true;
}

bool CSSParameters::exportToFile(const QString& file_name) const
{
    QSettings settings(file_name, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    settings.setValue("outer_hull/half_breadth", half_breadth);
    settings.setValue("outer_hull/depth", depth);
    settings.setValue("outer_hull/bilge_radius", bilge_radius);
    settings.setValue("outer_hull/deck_corner_radius", deck_corner_radius);
    settings.setValue("camber/camber_height", camber_height);
    settings.setValue("camber/deck_distance", deck_distance);
    settings.setValue("inner_hull/inner_hull_height", inner_hull_height);
    settings.setValue("inner_hull/inner_hull_distance", inner_hull_distance);
    settings.beginWriteArray("grider", grider_distances.size());
    for(int i = 0; i < grider_distances.size(); ++i)
    {
        settings.setArrayIndex(i);
        settings.setValue("distance", grider_distances.at(i));
    }
    settings.endArray();
    settings.beginWriteArray("stringer", stringer_distances.size());
    for(int i = 0; i < stringer_distances.size(); ++i)
    {
        settings.setArrayIndex(i);
        settings.setValue("distance", stringer_distances.at(i));
    }
    settings.endArray();
    settings.setValue("hatch_coaming/hatch_coaming_height", hatch_coaming_height);
    settings.setValue("hatch_coaming/hatch_coaming_width", hatch_coaming_width);
    settings.setValue("mesh_option/need_mirror_section", need_mirror_section);
    settings.setValue("mesh_option/need_extrude_section", need_extrude_section);
    settings.setValue("mesh_option/extrude_direction", extrude_direction);
    settings.setValue("mesh_option/extrude_cnt", extrude_cnt);
    settings.setValue("mesh_option/need_refine_section", need_refine_section);
    settings.setValue("mesh_option/need_stiffener", need_stiffener);
    settings.setValue("file/export_file_path", export_file_path);
    return true;
}

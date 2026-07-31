#ifndef CSSPARAMETERS_H
#define CSSPARAMETERS_H

#include <QVector>
#include <QObject>

class CSSParameters: public QObject
{
    Q_OBJECT

public:
    explicit CSSParameters(QObject*);
    ~CSSParameters();

    // Outer Hull 外壳参数
    double getHalfBreadth() const;
    bool setHalfBreadth(double);
    QString getHalfBreadthDesc() const;

    double getDepth() const;
    bool setDepth(double);
    QString getDepthDesc() const;

    double getBilgeRadius() const;
    bool setBilgeRadius(double);
    QString getBilgeRadiusDesc() const;

    double getDeckCornerRadius() const;
    bool setDeckCornerRadius(double);
    QString getDeckCornerRadiusDesc() const;

    // Camber 外倾角
    double getCamberHeight() const;
    bool setCamberHeight(double);
    QString getCamberHeightDesc() const;

    double getDeckDistance() const;
    bool setDeckDistance(double);
    QString getDeckDistanceDesc() const;

    // Inner Hull 内壳参数
    double getInnerHullHeight() const;
    bool setInnerHullHeight(double);
    QString getInnerHullHeightDesc() const;

    double getInnerHullDistance() const;
    bool setInnerHullDistance(double);
    QString getInnerHullDistanceDesc() const;

    // Grider 双层底纵桁参数
    uint getGriderMinimumCnt() const;
    QVector<double> getGriderDistances() const;
    QVector<bool> setGriderDistances(const QVector<double>&);
    QString getGriderDistancesDesc() const;

    // Stringer 双壳纵桁参数
    uint getStringerMinimumCnt() const;
    QVector<double> getStringerDistances() const;
    QVector<bool> setStringerDistances(const QVector<double>&);
    QString getStringerDistancesDesc() const;

    // Hatch Coaming 舱口围参数
    double getHatchCoamingHeight() const;
    bool setHatchCoamingHeight(double);
    QString getHatchCoamingHeightDesc() const;

    double getHatchCoamingWidth() const;
    bool setHatchCoamingWidth(double);
    QString getHatchCoamingWidthDesc() const;

    // Mesh Option 网格选项
    bool needMirrorSection() const;
    bool setMirrorSection(bool);

    bool needExtrudeSection() const;
    bool setExtrudeSection(bool);
    double getExtrudeDirection() const;
    bool setExtrudeDirection(double);
    int getExtrudeCnt() const;
    bool setExtrudeCnt(int);

    bool needRefineSection() const;
    bool setRefineSection(bool);

    bool needStiffener() const;
    bool setNeedStiffener(bool);

    // File 文件参数
    QString getExportFilePath() const;
    bool setExportFilePath(const QString&);
    bool importFromFile(const QString&);
    bool exportToFile(const QString&) const;

private:
    // Outer Hull 外壳参数
    double half_breadth = -1;
    double depth = -1;
    double bilge_radius = -1;
    double deck_corner_radius = -1;
    // Camber 外倾角
    double camber_height = -1;
    double deck_distance = -1;
    // Inner Hull 内壳参数
    double inner_hull_height = -1;
    double inner_hull_distance = -1;
    // Grider 双层底纵桁参数
    QVector<double> grider_distances;
    // Stringer 双壳纵桁参数
    QVector<double> stringer_distances;
    // Hatch Coaming 舱口围参数
    double hatch_coaming_height = -1;
    double hatch_coaming_width = -1;
    // Mesh Option 网格选项
    bool need_mirror_section = true;
    bool need_extrude_section = true;
    double extrude_direction = 0;
    int extrude_cnt = -1;
    bool need_refine_section = true;
    bool need_stiffener = true;
    // File 文件参数
    QString export_file_path;
};

#endif // CSSPARAMETERS_H

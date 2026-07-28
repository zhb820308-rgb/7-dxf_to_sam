#ifndef SamBuilder_h
#define SamBuilder_h

#include "SamData.h"
#include <QString>
#include <vector>

class skcSketch;
class skcGeomFactory;

class SamBuilder {
public:
    SamBuilder();
    ~SamBuilder();

    bool beginImport(const QString& modelName = "Model-1");
    bool commit();
    void rollback();

    int createPoints(const std::vector<DxfPoint>& points);
    int createLines(const std::vector<DxfLine>& lines);
    int createCircles(const std::vector<DxfCircle>& circles);

    int   createdCount() const { return m_createdCount; }
    const QString& sketchName() const { return m_sketchName; }
    const QString& lastError() const { return m_lastError; }

private:
    void buildSketchPath();

    QString m_modelName;
    QString m_sketchName;
    QString m_sketchPath;
    QString m_lastError;

    skcSketch*      m_sketch  = nullptr;
    skcGeomFactory* m_factory = nullptr;
    int m_createdCount = 0;
    bool m_active = false;
};

#endif

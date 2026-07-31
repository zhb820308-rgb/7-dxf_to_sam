#ifndef SamBuilder_h
#define SamBuilder_h

#include "SamData.h"
#include <basBasis.h>
#include <QString>
#include <functional>
#include <vector>

class skcSketch;
class skcGeomFactory;

/// @brief SAM SDK adapter for building sketches from DXF geometry.
///
/// Creates lines and circles in the SAM modeling database, manages
/// sketch lifecycle (begin/commit/rollback), and reports progress.
class SamBuilder {
public:
    using ProgressCallback = std::function<bool(const QString& stage, int current, int total)>;

    SamBuilder();
    ~SamBuilder();

    /// @brief Begin an import session. Creates sketch and geometry factory.
    /// @param modelName SAM model name (default "Model-1").
    /// @return true on success; call lastError() on failure.
    bool beginImport(const QString& modelName = "Model-1");

    /// @brief Insert the sketch into the repository and set up scene display.
    /// @return true on success; call lastError() on failure.
    bool commit();

    /// @brief Discard the current import without committing.
    void rollback();

    /// @brief Set a progress callback invoked during geometry creation.
    /// Return false from the callback to cancel the import.
    void setProgressCallback(const ProgressCallback& callback);

    /// @brief Create line entities from DXF line data.
    /// @return Number of lines created, or -1 if canceled.
    int createLines(const std::vector<DxfLine>& lines);

    /// @brief Create circle entities from DXF circle data.
    /// @return Number of circles created, or -1 if canceled.
    int createCircles(const std::vector<DxfCircle>& circles);

    /// @brief Total number of entities created in the current session.
    int   createdCount() const { return m_createdCount; }
    /// @brief Name of the sketch being built.
    const QString& sketchName() const { return m_sketchName; }
    /// @brief Last error message (empty if no error).
    const QString& lastError() const { return m_lastError; }

private:
    void buildSketchPath();
    void extendBounds(double x, double y);
    bool reportProgress(const QString& stage, int current, int total);

    QString m_modelName;
    QString m_sketchName;
    QString m_sketchPath;
    QString m_lastError;

    skcSketch*      m_sketch  = nullptr;
    skcGeomFactory* m_factory = nullptr;
    basMdb          m_mdb;
    int m_createdCount = 0;
    bool m_active = false;
    ProgressCallback m_progressCallback;

    // bounding box of imported geometry (for sheet size)
    bool   m_hasBounds = false;
    double m_minX = 0.0, m_minY = 0.0;
    double m_maxX = 0.0, m_maxY = 0.0;
};

#endif

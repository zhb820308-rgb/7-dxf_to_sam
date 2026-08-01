#ifndef SamData_h
#define SamData_h

#include "DxfData.h"
#include <cstddef>
#include <vector>

/// Describes which original curve produced a converted line segment.
/// lineIndex refers to the same DxfLine stored in SamData::lines(), so this
/// metadata does not duplicate or alter the geometry used to build SAM.
struct CurveSegmentSource {
    EntityType parentType;
    int parentId;
    size_t segmentIndex;
    size_t lineIndex;
};

/// @brief Output of ConversionEngine — ready to pass to skcGeomFactory.
/// Coordinates are already offset by the base point.
class SamData {
public:
    SamData() = default;

    // ======== read-only access ========
    const std::vector<DxfLine>&   lines()   const { return m_lines; }
    const std::vector<DxfCircle>& circles() const { return m_circles; }
    const std::vector<CurveSegmentSource>& curveSegments() const { return m_curveSegments; }

    // ======== modifiers ========
    void addLine(const DxfLine& line)   { m_lines.push_back(line); }
    void addCircle(const DxfCircle& c)  { m_circles.push_back(c); }
    void addCurveSegment(EntityType parentType, int parentId,
                         size_t segmentIndex, const DxfLine& line) {
        m_lines.push_back(line);
        CurveSegmentSource source = {
            parentType, parentId, segmentIndex, m_lines.size() - 1
        };
        m_curveSegments.push_back(source);
    }

    void clear() {
        m_lines.clear();
        m_circles.clear();
        m_curveSegments.clear();
        m_errorCode = DxfImportErrorCode::None;
        m_errorMessage.clear();
    }

    DxfImportErrorCode errorCode() const { return m_errorCode; }
    QString errorMessage() const { return m_errorMessage; }
    void setError(DxfImportErrorCode code, const QString& message) {
        m_errorCode = code;
        m_errorMessage = message;
    }

private:
    std::vector<DxfLine>   m_lines;
    std::vector<DxfCircle> m_circles;
    std::vector<CurveSegmentSource> m_curveSegments;
    DxfImportErrorCode m_errorCode = DxfImportErrorCode::None;
    QString m_errorMessage;
};

#endif

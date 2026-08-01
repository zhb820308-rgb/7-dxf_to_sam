#ifndef PythonFiniteElementBuilder_h
#define PythonFiniteElementBuilder_h

#include <QString>
#include <functional>
#include <vector>

struct FeNode;
struct FeTruss;

// Builds an FE Part through SAMCAE's official Python API:
//   mdl.Part() -> createNode() -> Element(TRUSS) -> displayedObject -> fitView.
// This route is verified to update SAM's Part Manager and standard GUI lifecycle.
class PythonFiniteElementBuilder {
public:
    using ProgressCallback =
        std::function<bool(const QString& stage, int current, int total)>;

    bool beginImport(const QString& modelName, const QString& partName);
    int createNodes(const std::vector<FeNode>& nodes);
    int createTrusses(const std::vector<FeTruss>& trusses);
    bool commit();
    void rollback();

    void setProgressCallback(const ProgressCallback& callback)
    {
        m_progressCallback = callback;
    }

    // Control how many nodes/trusses are sent in a single Python command.
    // Default: 5000.  Larger batches reduce interpreter round-trips at the
    // cost of longer Python command strings and coarser cancellation
    // granularity.  Set before calling createNodes / createTrusses.
    void setBatchSize(int size) { m_batchSize = size; }
    int batchSize() const { return m_batchSize; }

    const QString& lastError() const { return m_lastError; }
    int createdNodeCount() const { return m_createdNodeCount; }
    int createdTrussCount() const { return m_createdTrussCount; }

private:
    bool runCommand(const QString& command, const QString& stage);
    bool reportProgress(const QString& stage, int current, int total);
    static QString pythonStringLiteral(const QString& value);

    QString m_modelName;
    QString m_partName;
    QString m_lastError;
    QString m_partExpression;
    int m_createdNodeCount = 0;
    int m_createdTrussCount = 0;
    bool m_active = false;
    int m_batchSize = 5000;
    ProgressCallback m_progressCallback;
};

#endif // PythonFiniteElementBuilder_h
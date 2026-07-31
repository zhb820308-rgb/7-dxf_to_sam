#include "PythonFiniteElementBuilder.h"
#include "FeData.h"

#include <QCoreApplication>
#include <QDebug>

#include <basBasis.h>
#include <basMdb.h>
#include <ptoKPartRepository.h>
#include <ptoKUtils.h>
#include <pytInterpreterRole.h>

#include <cmath>

QString PythonFiniteElementBuilder::pythonStringLiteral(const QString& value)
{
    QString escaped = value;
    escaped.replace('\\', "\\\\");
    escaped.replace('\'', "\\'");
    escaped.replace('\n', "\\n");
    escaped.replace('\r', "\\r");
    return QString("'%1'").arg(escaped);
}

bool PythonFiniteElementBuilder::runCommand(
    const QString& command, const QString& stage)
{
    pytInterpreterRole& interpreter = pytInterpreterRole::Instance();
    interpreter.RunCommand(command, false);

    const QString traceback = interpreter.GetLastTraceback();
    if (!traceback.isEmpty()) {
        m_lastError = QString("%1 failed: %2").arg(stage, traceback);
        qWarning().noquote() << "[PythonFiniteElementBuilder]" << m_lastError;
        return false;
    }
    return true;
}

bool PythonFiniteElementBuilder::reportProgress(
    const QString& stage, int current, int total)
{
    if (!m_progressCallback)
        return true;

    if (current % 1000 == 0 || current == total)
        QCoreApplication::processEvents();

    if (m_progressCallback(stage, current, total))
        return true;

    m_lastError = QStringLiteral("import canceled by user");
    return false;
}

bool PythonFiniteElementBuilder::beginImport(
    const QString& modelName, const QString& partName)
{
    m_lastError.clear();
    m_modelName = modelName;
    m_partName = partName;
    m_createdNodeCount = 0;
    m_createdTrussCount = 0;
    m_active = false;

    if (modelName.isEmpty() || partName.isEmpty()) {
        m_lastError = QStringLiteral("modelName and partName must not be empty");
        return false;
    }

    try {
        basMdb mdb = basBasis::Instance()->Fetch();
        ptoKPartRepository& parts = ptoKGetPartRepos(mdb, modelName);
        if (parts.IsMember(partName)) {
            m_lastError = QString("Part '%1' already exists in model '%2'")
                .arg(partName, modelName);
            return false;
        }
    } catch (...) {
        m_lastError = QString("model '%1' was not found").arg(modelName);
        return false;
    }

    m_partExpression = QString("mdb.models[%1].parts[%2]")
        .arg(pythonStringLiteral(modelName), pythonStringLiteral(partName));
    const QString command = QString("import samConstants\n"
        "_example1_fe_model = mdb.models[%1]\n"
        "_example1_fe_part = _example1_fe_model.Part(name=%2)")
        .arg(pythonStringLiteral(modelName), pythonStringLiteral(partName));
    if (!runCommand(command, QStringLiteral("create Part")))
        return false;

    m_active = true;
    return true;
}

int PythonFiniteElementBuilder::createNodes(const std::vector<FeNode>& nodes)
{
    if (!m_active) {
        m_lastError = QStringLiteral("createNodes: no active import context");
        return -1;
    }

    const int total = static_cast<int>(nodes.size());
    for (const FeNode& node : nodes) {
        if (!std::isfinite(node.x) || !std::isfinite(node.y) || !std::isfinite(node.z)) {
            m_lastError = QString("createNodes: invalid coordinate for node %1").arg(node.id);
            return -1;
        }
        const QString command = QString("_example1_fe_part.createNode(x=%1, y=%2, z=%3)")
            .arg(QString::number(node.x, 'g', 17))
            .arg(QString::number(node.y, 'g', 17))
            .arg(QString::number(node.z, 'g', 17));
        if (!runCommand(command, QString("create node %1").arg(node.id)))
            return -1;
        ++m_createdNodeCount;
        if (!reportProgress(QStringLiteral("Creating FE nodes"), m_createdNodeCount, total))
            return -1;
    }
    return m_createdNodeCount;
}

int PythonFiniteElementBuilder::createTrusses(const std::vector<FeTruss>& trusses)
{
    if (!m_active) {
        m_lastError = QStringLiteral("createTrusses: no active import context");
        return -1;
    }

    const int total = static_cast<int>(trusses.size());
    for (const FeTruss& truss : trusses) {
        if (truss.startNodeId < 0 || truss.endNodeId < 0 ||
            truss.startNodeId >= m_createdNodeCount ||
            truss.endNodeId >= m_createdNodeCount)
        {
            m_lastError = QString("createTrusses: invalid node ID in truss %1")
                .arg(truss.id);
            return -1;
        }
        const QString command = QString(
            "_example1_fe_part.Element("
            "nodes=(_example1_fe_part.nodes[%1], _example1_fe_part.nodes[%2]), "
            "elemShape=samConstants.TRUSS, intersectNodes=False)")
            .arg(truss.startNodeId)
            .arg(truss.endNodeId);
        if (!runCommand(command, QString("create truss %1").arg(truss.id)))
            return -1;
        ++m_createdTrussCount;
        if (!reportProgress(QStringLiteral("Creating FE trusses"), m_createdTrussCount, total))
            return -1;
    }
    return m_createdTrussCount;
}

bool PythonFiniteElementBuilder::commit()
{
    if (!m_active) {
        m_lastError = QStringLiteral("commit: no active import context");
        return false;
    }

    const QString command = QString(
        "_example1_fe_vp = session.viewports[%1]\n"
        "_example1_fe_vp.setValues(displayedObject=_example1_fe_part)\n"
        "_example1_fe_vp.view.fitView()")
        .arg(pythonStringLiteral(QStringLiteral("Viewport: 1")));
    if (!runCommand(command, QStringLiteral("display FE Part")))
        return false;

    m_active = false;
    return true;
}

void PythonFiniteElementBuilder::rollback()
{
    // SAM's documented stable delete-Part Python API has not been verified.
    // Leave any partially created Part intact and report the original error.
    m_active = false;
    qWarning() << "[PythonFiniteElementBuilder] rollback:"
               << "part may remain in the model after a Python API failure";
}
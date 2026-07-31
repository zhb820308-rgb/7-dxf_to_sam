#include "SamBuilder.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>

#include <gslPoint.h>
#include <gslMatrix.h>

#include <basBasis.h>

#include <gmlSketchRepository.h>
#include <gmlSketchWrapper.h>

#include <skcSketch.h>
#include <skcGeomFactory.h>
#include <skcKReposUtils.h>
#include <skcKToolset.h>
#include <skcKUtils.h>
#include <skcUtils.h>
#include <skcPDO.h>
#include <skcUndoRedoStack.h>

#include <smgSceneManagerRole.h>

#include <gcuScene.h>
#include <gdyScene.h>

#include <sesKSessionState.h>

// ========================================================================
//  Helpers
// ========================================================================

static QString bracket(const QString& name) {
    return QString("[%1]").arg(name);
}

static gcuScene* currentScene(bool force) {
    return static_cast<gcuScene*>(gdyScene::GetCurrentScene(force));
}

void SamBuilder::extendBounds(double x, double y) {
    if (!m_hasBounds) {
        m_minX = m_maxX = x;
        m_minY = m_maxY = y;
        m_hasBounds = true;
        return;
    }
    if (x < m_minX) m_minX = x;
    if (x > m_maxX) m_maxX = x;
    if (y < m_minY) m_minY = y;
    if (y > m_maxY) m_maxY = y;
}

// ========================================================================
//  Constructor / Destructor
// ========================================================================

SamBuilder::SamBuilder() {}

SamBuilder::~SamBuilder() {
    delete m_factory;
    m_factory = nullptr;
}

void SamBuilder::setProgressCallback(const ProgressCallback& callback) {
    m_progressCallback = callback;
}

bool SamBuilder::reportProgress(const QString& stage, int current, int total) {
    if (!m_progressCallback)
        return true;

    if (m_progressCallback(stage, current, total))
        return true;

    m_lastError = "import canceled by user";
    qWarning() << "[SamBuilder] import canceled during" << stage
               << current << "/" << total;
    return false;
}

// ========================================================================
//  beginImport
// ========================================================================

bool SamBuilder::beginImport(const QString& modelName) {
    m_modelName = modelName;
    m_sketchName = QString("DxfImport_%1")
                       .arg(QDateTime::currentMSecsSinceEpoch());
    m_createdCount = 0;
    m_lastError.clear();

    m_hasBounds = false;

    basMdb mdb = basBasis::Instance()->Fetch();
    m_mdb = mdb;  // Save snapshot for reuse in commit()
    gmlSketchRepository& sketches = skcKGetSketchRepos(mdb, m_modelName);

    gslMatrix transform;
    skcSketch* sketch = skcCreateSketchWithXYAxis(&transform);
    if (!sketch) {
        m_lastError = "failed to create sketch";
        return false;
    }

    const uint sketchId = sketches.Size() + 1;
    sketch->SetID(sketchId);
    sketch->DisplayOptions().SetSheetSize(200.0);

    m_sketch = sketch;
    m_factory = new skcGeomFactory(sketch);
    m_active = true;
    return true;
}

// ========================================================================
//  createLines
// ========================================================================

int SamBuilder::createLines(const std::vector<DxfLine>& lines) {
    if (!m_active || !m_factory) return 0;
    const int total = static_cast<int>(lines.size());
    if (!reportProgress(QStringLiteral("Creating lines"), 0, total))
        return -1;

    int count = 0;
    for (const DxfLine& line : lines) {
        gslPoint p1(line.start().x(), line.start().y(), line.start().z());
        gslPoint p2(line.end().x(),   line.end().y(),   line.end().z());
        m_factory->CreateLine(p1, p2, skc_FOREGROUND, false);
        extendBounds(p1.GetX(), p1.GetY());
        extendBounds(p2.GetX(), p2.GetY());
        ++count;

        if (count % 1000 == 0 || count == total) {
            if (!reportProgress(QStringLiteral("Creating lines"), count, total))
                return -1;
            QCoreApplication::processEvents();
        }
    }
    m_createdCount += count;
    return count;
}

// ========================================================================
//  createCircles
// ========================================================================

int SamBuilder::createCircles(const std::vector<DxfCircle>& circles) {
    if (!m_active || !m_factory) return 0;
    const int total = static_cast<int>(circles.size());
    if (!reportProgress(QStringLiteral("Creating circles"), 0, total))
        return -1;

    int count = 0;
    for (const DxfCircle& circle : circles) {
        const DxfPoint& c = circle.center();
        double r = circle.radius();
        gslPoint ptCenter(c.x(), c.y(), c.z());
        gslPoint ptOnCircle(c.x() + r, c.y(), c.z());
        m_factory->CreateCircle(ptCenter, ptOnCircle, skc_FOREGROUND, false);
        extendBounds(c.x() + r, c.y() + r);
        extendBounds(c.x() - r, c.y() - r);
        ++count;

        if (count % 1000 == 0 || count == total) {
            if (!reportProgress(QStringLiteral("Creating circles"), count, total))
                return -1;
            QCoreApplication::processEvents();
        }
    }
    m_createdCount += count;
    return count;
}

// ========================================================================
//  commit
// ========================================================================

bool SamBuilder::commit() {
    if (!m_active || !m_sketch) {
        m_lastError = "no active import context";
        return false;
    }

    // Give the UI one final cancellation point before mutating the repository.
    if (!reportProgress(
            QStringLiteral("Finalizing import"),
            m_createdCount,
            m_createdCount)) {
        return false;
    }

    // 1. Insert sketch into repository
    {
        // Fit sheet size to the imported geometry so it is not outside
        // the (origin-centered) sheet. Sheet must cover the farthest point.
        if (m_hasBounds) {
            const double maxAbs = qMax(qMax(qAbs(m_minX), qAbs(m_maxX)),
                                       qMax(qAbs(m_minY), qAbs(m_maxY)));
            double extent = maxAbs * 2.4;
            if (extent < 200.0) extent = 200.0;
            m_sketch->DisplayOptions().SetSheetSize(extent);
        }

        basMdb mdb = m_mdb;  // Reuse snapshot acquired in beginImport()
        gmlSketchRepository& sketches = skcKGetSketchRepos(mdb, m_modelName);

        gmlSketchWrapper wrapper(m_sketch);
        sketches.Insert(m_sketchName, wrapper);
        basBasis::Instance()->Replace(mdb);
    }
    skcUndoRedoStack::Instance().ClearUndoStates();

    // 2. Scene display
    smgSceneManagerRole& role = smgSceneManagerRole::TheSceneManagerRole();
    const int viewport = role.GetCurrentViewport();
    const omuPrimType sceneType = role.GetSceneManagerName(viewport);
    if (sceneType.DisplayType() != omu_PART)
        role.Map(omu_PART, viewport);

    gcuScene* scene = currentScene(false);
    if (scene) {
        scene = currentScene(true);
        scene->ClearScene();

        skcPDO* sketchPdo = skcKUtils::GetCurrentSketchPDO();
        if (sketchPdo) {
            sketchPdo->SetSketch(m_sketch);
            sketchPdo->Rebuild();
            skcKToolset::Instance().ShowFrontView();

            buildSketchPath();
            sesKSessionState::Instance()->SetPrimaryObjectPath(m_sketchPath);
        }
    }

    m_active = false;
    delete m_factory;
    m_factory = nullptr;
    return true;
}

// ========================================================================
//  rollback
// ========================================================================

void SamBuilder::rollback() {
    delete m_factory;
    m_factory = nullptr;
    m_active = false;
    m_createdCount = 0;
}

// ========================================================================
//  buildSketchPath
// ========================================================================

void SamBuilder::buildSketchPath() {
    m_sketchPath = QString("mdb.models");
    m_sketchPath.append(bracket(m_modelName));
    m_sketchPath.append(".sketches");
    m_sketchPath.append(bracket(m_sketchName));
}

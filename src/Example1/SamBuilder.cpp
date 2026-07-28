#include "SamBuilder.h"

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

// ========================================================================
//  Constructor / Destructor
// ========================================================================

SamBuilder::SamBuilder() {}

SamBuilder::~SamBuilder() {}

// ========================================================================
//  beginImport
// ========================================================================

bool SamBuilder::beginImport(const QString& modelName) {
    m_modelName = modelName;
    m_sketchName = QString("DxfImport_%1")
                       .arg(QDateTime::currentMSecsSinceEpoch());
    m_createdCount = 0;
    m_lastError.clear();

    qDebug() << "[SamBuilder] sketch:" << m_sketchName;

    basMdb mdb = basBasis::Instance()->Fetch();
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

    qDebug() << "[SamBuilder] sketch created, ID:" << sketchId;
    return true;
}

// ========================================================================
//  createPoints
// ========================================================================

int SamBuilder::createPoints(const std::vector<DxfPoint>& points) {
    if (!m_active || !m_factory) return 0;
    int count = 0;
    for (const DxfPoint& pt : points) {
        // TODO: skcGeomFactory::CreatePoint interface TBC
        ++count;
    }
    m_createdCount += count;
    return count;
}

// ========================================================================
//  createLines
// ========================================================================

int SamBuilder::createLines(const std::vector<DxfLine>& lines) {
    if (!m_active || !m_factory) return 0;
    int count = 0;
    for (const DxfLine& line : lines) {
        gslPoint p1(line.start().x(), line.start().y(), line.start().z());
        gslPoint p2(line.end().x(),   line.end().y(),   line.end().z());
        m_factory->CreateLine(p1, p2, skc_FOREGROUND, false);
        ++count;
    }
    m_createdCount += count;
    qDebug() << "[SamBuilder] lines:" << count;
    return count;
}

// ========================================================================
//  createCircles
// ========================================================================

int SamBuilder::createCircles(const std::vector<DxfCircle>& circles) {
    if (!m_active || !m_factory) return 0;
    int count = 0;
    for (const DxfCircle& circle : circles) {
        // TODO: skcGeomFactory::CreateCircle interface TBC
        ++count;
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

    // 1. Insert sketch into repository
    {
        basMdb mdb = basBasis::Instance()->Fetch();
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
    qDebug() << "[SamBuilder] commit done, total:" << m_createdCount;
    return true;
}

// ========================================================================
//  rollback
// ========================================================================

void SamBuilder::rollback() {
    qDebug() << "[SamBuilder] rollback...";
    m_active = false;
    m_createdCount = 0;
    m_lastError.clear();
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

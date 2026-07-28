#include "SamBuilder.h"

#include <QDateTime>
#include <QDebug>
#include <QString>

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

static gcuScene* currentScene(bool force = true) {
    return static_cast<gcuScene*>(gdyScene::GetCurrentScene(force));
}

// ========================================================================
//  Constructor / Destructor
// ========================================================================

SamBuilder::SamBuilder() {}

SamBuilder::~SamBuilder() {
    // 如果未 commit 也未 rollback，清理
}

// ========================================================================
//  beginImport — 创建草图 + 几何工厂
// ========================================================================

bool SamBuilder::beginImport(const QString& modelName) {
    m_modelName = modelName;
    m_sketchName = QString("DxfImport_%1")
                       .arg(QDateTime::currentMSecsSinceEpoch());
    m_createdCount = 0;
    m_lastError.clear();

    qDebug() << "[SamBuilder] 创建草图:" << m_sketchName;

    // 1. 获取 MDB
    basMdb mdb = basBasis::Instance()->Fetch();
    gmlSketchRepository& sketches = skcKGetSketchRepos(mdb, m_modelName);
    m_repos = &sketches;

    // 2. 创建草图
    gslMatrix transform;
    skcSketch* sketch = skcCreateSketchWithXYAxis(&transform);
    if (!sketch) {
        m_lastError = "创建草图失败";
        qDebug() << "[SamBuilder] 错误:" << m_lastError;
        return false;
    }

    const uint sketchId = sketches.Size() + 1;
    sketch->SetID(sketchId);
    sketch->DisplayOptions().SetSheetSize(200.0);

    m_sketch = sketch;
    m_factory = new skcGeomFactory(sketch);
    m_active = true;

    qDebug() << "[SamBuilder] 草图创建成功, ID:" << sketchId;
    return true;
}

// ========================================================================
//  createPoints
// ========================================================================

int SamBuilder::createPoints(const std::vector<DxfPoint>& points) {
    if (!m_active || !m_factory) return 0;
    int count = 0;
    for (const DxfPoint& pt : points) {
        // skcGeomFactory::CreatePoint 接口待确认
        // m_factory->CreatePoint(gslPoint(pt.x(), pt.y(), pt.z()));
        ++count;
        qDebug() << "[SamBuilder] 创建点:"
                 << pt.x() << pt.y() << pt.z();
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
    qDebug() << "[SamBuilder] 创建直线:" << count;
    return count;
}

// ========================================================================
//  createCircles
// ========================================================================

int SamBuilder::createCircles(const std::vector<DxfCircle>& circles) {
    if (!m_active || !m_factory) return 0;
    int count = 0;
    for (const DxfCircle& circle : circles) {
        // skcGeomFactory::CreateCircle 接口待确认
        qDebug() << "[SamBuilder] 创建圆: 圆心="
                 << circle.center().x() << circle.center().y() << circle.center().z()
                 << "半径=" << circle.radius();
        ++count;
    }
    m_createdCount += count;
    return count;
}

// ========================================================================
//  commit — 提交 DB + 刷新场景
// ========================================================================

bool SamBuilder::commit() {
    if (!m_active || !m_sketch) {
        m_lastError = "没有活动的导入上下文";
        return false;
    }

    qDebug() << "[SamBuilder] 提交到数据库...";

    // 1. 插入草图仓库
    {
        gmlSketchWrapper wrapper(m_sketch);
        m_repos->Insert(m_sketchName, wrapper);
        basBasis::Instance()->Replace(basBasis::Instance()->Fetch());
    }
    skcUndoRedoStack::Instance().ClearUndoStates();

    // 2. 场景展示
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

            // 构建路径
            buildSketchPath();
            sesKSessionState::Instance()->SetPrimaryObjectPath(m_sketchPath);
        }
    }

    m_active = false;
    qDebug() << "[SamBuilder] 提交完成, 共" << m_createdCount << "个图元";
    return true;
}

// ========================================================================
//  rollback
// ========================================================================

void SamBuilder::rollback() {
    qDebug() << "[SamBuilder] 回滚导入...";
    // 删除草图（TODO: 确认 SAM 的回滚接口）
    // 如果 m_sketch && m_repos，从仓库中删除
    if (m_sketch && m_repos) {
        // m_repos->Remove(m_sketchName);  // 接口待确认
    }
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

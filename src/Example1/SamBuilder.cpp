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

static const int kProgressUpdateInterval = 5000;

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
    if (m_transaction.ownsResource())
        rollback();
    else {
        delete m_factory;
        m_factory = nullptr;
    }
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

bool SamBuilder::isWriting() const {
    return m_transaction.state() == ImportTransactionState::Writing;
}

// ========================================================================
//  beginImport
// ========================================================================

ImportBuildResult SamBuilder::beginImport(const QString& modelName) {
    m_modelName = modelName;
    m_sketchName = QString("DxfImport_%1")
                       .arg(QDateTime::currentMSecsSinceEpoch());
    m_createdCount = 0;
    m_lastError.clear();
    m_hasBounds = false;
    m_sketchWrapped = false;

    if (!m_transaction.begin()) {
        m_lastError = "beginImport: transaction is already active or committed";
        return ImportBuildResult::failure(
            ImportBuildStatus::BeginFailed, m_lastError);
    }
    if (modelName.isEmpty()) {
        m_lastError = "modelName must not be empty";
        m_transaction.rollback([]() { return true; });
        return ImportBuildResult::failure(
            ImportBuildStatus::BeginFailed, m_lastError);
    }

    try {
        basMdb mdb = basBasis::Instance()->Fetch();
        gmlSketchRepository& sketches = skcKGetSketchRepos(mdb, m_modelName);

        gslMatrix transform;
        skcSketch* sketch = skcCreateSketchWithXYAxis(&transform);
        if (!sketch) {
            m_lastError = "failed to create sketch";
            m_transaction.rollback([]() { return true; });
            return ImportBuildResult::failure(
                ImportBuildStatus::BeginFailed, m_lastError);
        }

        const uint sketchId = sketches.NextID();
        sketch->SetID(sketchId);
        sketch->DisplayOptions().SetSheetSize(200.0);

        m_sketch = sketch;
        m_transaction.markResourceOwned();
        m_factory = new skcGeomFactory(sketch);
        if (!m_transaction.startWriting()) {
            m_lastError = "failed to enter writing state";
            const QString beginError = m_lastError;
            const ImportBuildResult cleanup = rollback();
            if (!cleanup.succeeded())
                return cleanup;
            m_lastError = beginError;
            return ImportBuildResult::failure(
                ImportBuildStatus::BeginFailed, m_lastError);
        }
        return ImportBuildResult::success();
    } catch (...) {
        m_lastError = "failed to prepare sketch import";
        const QString beginError = m_lastError;
        const ImportBuildResult cleanup = rollback();
        if (!cleanup.succeeded())
            return cleanup;
        m_lastError = beginError;
        return ImportBuildResult::failure(
            ImportBuildStatus::BeginFailed, m_lastError);
    }
}

// ========================================================================
//  createLines
// ========================================================================

ImportBuildResult SamBuilder::createLines(const std::vector<DxfLine>& lines) {
    if (!isWriting() || !m_factory) {
        m_lastError = "createLines: no active import context";
        return ImportBuildResult::failure(
            ImportBuildStatus::CreateFailed, m_lastError);
    }
    const int total = static_cast<int>(lines.size());
    if (!reportProgress(QStringLiteral("Creating lines"), 0, total))
        return ImportBuildResult::failure(
            ImportBuildStatus::Canceled, m_lastError);

    int count = 0;
    for (const DxfLine& line : lines) {
        gslPoint p1(line.start().x(), line.start().y(), line.start().z());
        gslPoint p2(line.end().x(),   line.end().y(),   line.end().z());
        m_factory->CreateLine(p1, p2, skc_FOREGROUND, false);
        extendBounds(p1.GetX(), p1.GetY());
        extendBounds(p2.GetX(), p2.GetY());
        ++count;

        if (count % kProgressUpdateInterval == 0 || count == total) {
            if (!reportProgress(QStringLiteral("Creating lines"), count, total))
                return ImportBuildResult::failure(
                    ImportBuildStatus::Canceled, m_lastError, count);
            QCoreApplication::processEvents();
        }
    }
    m_createdCount += count;
    return ImportBuildResult::success(count);
}

// ========================================================================
//  createCircles
// ========================================================================

ImportBuildResult SamBuilder::createCircles(const std::vector<DxfCircle>& circles) {
    if (!isWriting() || !m_factory) {
        m_lastError = "createCircles: no active import context";
        return ImportBuildResult::failure(
            ImportBuildStatus::CreateFailed, m_lastError);
    }
    const int total = static_cast<int>(circles.size());
    if (!reportProgress(QStringLiteral("Creating circles"), 0, total))
        return ImportBuildResult::failure(
            ImportBuildStatus::Canceled, m_lastError);

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

        if (count % kProgressUpdateInterval == 0 || count == total) {
            if (!reportProgress(QStringLiteral("Creating circles"), count, total))
                return ImportBuildResult::failure(
                    ImportBuildStatus::Canceled, m_lastError, count);
            QCoreApplication::processEvents();
        }
    }
    m_createdCount += count;
    return ImportBuildResult::success(count);
}

// ========================================================================
//  commit
// ========================================================================

ImportBuildResult SamBuilder::commit() {
    if (!isWriting() || !m_sketch) {
        m_lastError = "no active import context";
        return ImportBuildResult::failure(
            ImportBuildStatus::CommitFailed, m_lastError);
    }

    // Give the UI one final cancellation point before mutating the repository.
    if (!reportProgress(
            QStringLiteral("Finalizing import"),
            m_createdCount,
            m_createdCount)) {
        return ImportBuildResult::failure(
            ImportBuildStatus::Canceled, m_lastError, m_createdCount);
    }

    if (!m_transaction.startCommitting()) {
        m_lastError = "failed to enter committing state";
        return ImportBuildResult::failure(
            ImportBuildStatus::CommitFailed, m_lastError, m_createdCount);
    }

    // Repository replacement is the transaction boundary. Scene refresh is
    // deliberately performed after the durable commit and cannot roll it back.
    try {
        // Fit sheet size to the imported geometry so it is not outside
        // the (origin-centered) sheet. Sheet must cover the farthest point.
        if (m_hasBounds) {
            const double maxAbs = qMax(qMax(qAbs(m_minX), qAbs(m_maxX)),
                                       qMax(qAbs(m_minY), qAbs(m_maxY)));
            double extent = maxAbs * 2.4;
            if (extent < 200.0) extent = 200.0;
            m_sketch->DisplayOptions().SetSheetSize(extent);
        }

        // Fetch again so a long geometry build cannot replace unrelated model
        // changes with the snapshot taken at beginImport().
        basMdb mdb = basBasis::Instance()->Fetch();
        gmlSketchRepository& sketches = skcKGetSketchRepos(mdb, m_modelName);
        m_sketch->SetID(sketches.NextID());

        gmlSketchWrapper wrapper(m_sketch);
        m_sketchWrapped = true;
        if (!sketches.Insert(m_sketchName, wrapper)) {
            m_lastError = "failed to insert sketch into repository";
            return ImportBuildResult::failure(
                ImportBuildStatus::CommitFailed, m_lastError, m_createdCount);
        }
        basBasis::Instance()->Replace(mdb);
    } catch (...) {
        m_lastError = "failed to replace model database with imported sketch";
        return ImportBuildResult::failure(
            ImportBuildStatus::CommitFailed, m_lastError, m_createdCount);
    }

    delete m_factory;
    m_factory = nullptr;
    if (!m_transaction.markCommitted()) {
        m_lastError = "repository replaced but transaction finalization failed";
        return ImportBuildResult::failure(
            ImportBuildStatus::CommitFailed, m_lastError, m_createdCount);
    }

    try {
        refreshSceneAfterCommit();
    } catch (...) {
        qWarning() << "[SamBuilder] sketch committed, but scene refresh failed";
    }
    return ImportBuildResult::success(m_createdCount);
}

void SamBuilder::refreshSceneAfterCommit() {
    skcUndoRedoStack::Instance().ClearUndoStates();

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
}

// ========================================================================
//  rollback
// ========================================================================

bool SamBuilder::removePublishedSketch() {
    delete m_factory;
    m_factory = nullptr;

    if (!m_sketchWrapped) {
        delete m_sketch;
        m_sketch = nullptr;
        return true;
    }

    try {
        basMdb mdb = basBasis::Instance()->Fetch();
        gmlSketchRepository& sketches = skcKGetSketchRepos(mdb, m_modelName);
        if (sketches.IsMember(m_sketchName)) {
            if (!sketches.Remove(m_sketchName)) {
                m_lastError = "rollback failed: cannot remove sketch from repository";
                return false;
            }
            basBasis::Instance()->Replace(mdb);
        }

        basMdb verifiedMdb = basBasis::Instance()->Fetch();
        gmlSketchRepository& verifiedSketches =
            skcKGetSketchRepos(verifiedMdb, m_modelName);
        if (verifiedSketches.IsMember(m_sketchName)) {
            m_lastError = "rollback failed: sketch still exists in repository";
            return false;
        }
        m_sketch = nullptr;
        return true;
    } catch (...) {
        m_lastError = "rollback failed while restoring sketch repository";
        return false;
    }
}

ImportBuildResult SamBuilder::rollback() {
    const int createdBeforeRollback = m_createdCount;
    const bool cleaned = m_transaction.rollback([this]() {
        return removePublishedSketch();
    });
    if (!cleaned)
        qWarning().noquote() << "[SamBuilder]" << m_lastError;
    if (cleaned) {
        m_sketch = nullptr;
        m_createdCount = 0;
        return ImportBuildResult::success();
    }
    if (m_lastError.isEmpty())
        m_lastError = "rollback failed: transaction cannot be rolled back";
    return ImportBuildResult::failure(
        ImportBuildStatus::RollbackFailed, m_lastError,
        createdBeforeRollback);
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

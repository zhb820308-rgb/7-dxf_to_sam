// ============================================================================
// Sketch 路线用例
// ============================================================================
#include "DxfImportApplication.h"

#include "SketchConversion.h"
#include "DxfData.h"
#include "DxfImportRuntime.h"
#include "SketchImport.h"

#include <QDebug>
#include <QElapsedTimer>

namespace {

// 匿名 namespace 内的辅助函数只服务本编排器，不暴露为整个项目的公共 API。

DxfImportOutcome failedImport(
	DxfImportSession& session,
	DxfImportOutcomeStatus status,
	DxfImportErrorCode errorCode,
	const std::string& stage,
	const std::string& detail,
	const QString& warning)
{
	failImport(
		session.logger(), session.errorLogger(), session.importId(),
		session.pathText(), stage, detail, session.elapsed(), warning);
	return DxfImportOutcome::failure(
		status, errorCode, QString::fromStdString(stage), warning);
}

DxfImportOutcome failedBuildImport(
	DxfImportSession& session,
	const ImportBuildResult& buildResult,
	const std::string& stage,
	std::string detail,
	QString warning)
{
	if (buildResult.rollbackAttempted)
	{
		detail += buildResult.rollbackSucceeded
			? " rollback_succeeded=true"
			: " rollback_succeeded=false";
		if (!buildResult.rollbackSucceeded)
		{
			const std::string rollbackError =
				buildResult.rollbackMessage.toLocal8Bit().toStdString();
			detail += " rollback_error=\"" + rollbackError + "\"";
			warning += QStringLiteral("; rollback failed: ") +
				buildResult.rollbackMessage;
		}
	}
	failImport(
		session.logger(), session.errorLogger(), session.importId(),
		session.pathText(), stage, detail, session.elapsed(), warning);
	return DxfImportOutcome::fromBuildFailure(
		buildResult, QString::fromStdString(stage), warning);
}

DxfImportOutcome canceledImport(
	DxfImportSession& session,
	DxfImportProgress& progress,
	const QChar& separator)
{
	progress.close();
	if (session.logger())
	{
		session.logger()->warn(
			"[import={}] canceled stage=\"{}\" progress={}/{} total_elapsed_ms={}",
			session.importId(),
			progress.canceledStage().toLocal8Bit().toStdString(),
			progress.canceledCurrent(), progress.canceledTotal(),
			session.elapsed());
	}
	qWarning().noquote() << QString("[importDxf] IMPORT CANCELED %1 %2 %3/%4")
		.arg(separator)
		.arg(progress.canceledStage())
		.arg(progress.canceledCurrent())
		.arg(progress.canceledTotal());
	session.finish();
	return DxfImportOutcome::failure(
		DxfImportOutcomeStatus::Canceled,
		DxfImportErrorCode::Canceled,
		progress.canceledStage(),
		QStringLiteral("import canceled"));
}

} // namespace

DxfImportOutcome runSketchImport(
	const DxfImportRequest& request,
	DxfData& dxfData,
	const DxfEntityStats& entityStats,
	std::size_t outputLimit,
	DxfImportSession& session,
	QElapsedTimer& stageTimer)
{
	// Sketch 分支的数据逐级变换：DxfData -> SamData(lines/circles) -> SAM Sketch。
	stageTimer.restart();
	SamData samData;
	ConversionEngine converter;
	if (!converter.convert(
			dxfData, request.baseX, request.baseY, request.baseZ,
			request.curveTolerance, samData, outputLimit))
	{
		const QString errorCode =
			DxfImportFormatting::errorCodeText(samData.errorCode());
		const QString errorMessage = samData.errorMessage().isEmpty()
			? QStringLiteral("no valid entities to import")
			: samData.errorMessage();
		DxfImportFeedback::showBudgetError(
			samData.errorCode(), request.maxOutputEntities);
		return failedImport(
			session,
			DxfImportOutcomeStatus::ConvertFailed,
			samData.errorCode(),
			"conversion",
			" error_code=" + errorCode.toStdString() +
			" error=\"" + errorMessage.toLocal8Bit().toStdString() + "\"",
			QString("[importDxf] ERROR [%1]: %2")
				.arg(errorCode, errorMessage));
	}

	DxfImportFeedback::showSmallDrawingRecommendation(
		samData.lines().size() + samData.circles().size(),
		request.maxOutputEntities);
	dxfData.clear();
	if (session.logger())
	{
		session.logger()->info(
			"[import={}] conversion_completed lines={} circles={}"
			" curve_tolerance={} duration_ms={}",
			session.importId(), samData.lines().size(), samData.circles().size(),
			request.curveTolerance, stageTimer.elapsed());
	}
	logConvertedSamData(session.logger(), session.importId(), samData);

	stageTimer.restart();
	DxfImportProgress progress(
		DxfImportProgressMode::Sketch,
		samData.lines().size(), samData.circles().size());
	SamBuilder builder;
	builder.setProgressCallback(
		[&progress](const QString& stage, int current, int total) {
			return progress.update(stage, current, total);
		});
	const ImportBuildResult buildResult =
		DxfImportBuildService::buildSamSketch(samData, builder);
	if (!buildResult.succeeded())
	{
		progress.close();
		const bool canceled =
			buildResult.status == ImportBuildStatus::Canceled;
		const std::string stage =
			DxfImportFormatting::buildStage(buildResult.status);
		std::string detail =
			" error=\"" + buildResult.message.toLocal8Bit().toStdString() + "\"";
		if (buildResult.status != ImportBuildStatus::BeginFailed)
		{
			detail = " sketch=\"" +
				builder.sketchName().toLocal8Bit().toStdString() + "\"" + detail;
		}
		const QString warning = canceled
			? QString("[importDxf] IMPORT CANCELED — %1 %2/%3")
				.arg(progress.canceledStage())
				.arg(progress.canceledCurrent())
				.arg(progress.canceledTotal())
			: QString("[importDxf] ERROR: build failed, rolling back — %1")
				.arg(buildResult.message);
		return failedBuildImport(
			session, buildResult, stage, detail, warning);
	}

	progress.complete();
	qDebug().noquote() << DxfImportFormatting::summaryText(
		buildResult.createdCount, entityStats);
	if (session.logger())
	{
		session.logger()->info(
			"[import={}] succeeded sketch=\"{}\" submitted={}"
			" build_duration_ms={} total_duration_ms={}",
			session.importId(),
			builder.sketchName().toLocal8Bit().toStdString(),
			buildResult.createdCount, stageTimer.elapsed(), session.elapsed());
	}
	session.finish();
	return DxfImportOutcome::success(buildResult.createdCount);
}

// ============================================================================
// SAM Sketch 写入
// ============================================================================

#include <QCoreApplication>
#include <QDateTime>

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
    // QString::arg 用参数替换 %1，例如 Model-1 -> [Model-1]。
    return QString("[%1]").arg(name);
}

static gcuScene* currentScene(bool force) {
    // 基类 API 返回较通用的场景指针，这里显式向下转换为实际 gcuScene 类型。
    // static_cast 不做运行时检查，因此依赖 SAM SDK 对返回类型的约定。
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
    // RAII 最后保险：对象离开作用域时，若仍持有未提交资源就自动回滚。
    // 正常成功路径已 markCommitted，此时不会误删已提交 Sketch。
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

    // SAM SDK 部分接口会抛异常，但没有在公共类型中暴露稳定异常类，因此边界处
    // catch (...) 统一转成项目自己的 ImportBuildResult，避免异常穿过 Python/Qt ABI。
    try {
        // Fetch 取得当前模型数据库的工作副本；此处只用来确认仓库并分配对象。
        basMdb mdb = basBasis::Instance()->Fetch();
        gmlSketchRepository& sketches = skcKGetSketchRepos(mdb, m_modelName);

        // 创建位于 XY 平面的 Sketch；transform 由 SDK 填充/使用。
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
        // Factory 是 SAM 创建 Sketch 几何的专用接口。只有 sketch 成功后才创建。
        m_factory = new skcGeomFactory(sketch);
        if (!m_transaction.startWriting()) {
            m_lastError = "failed to enter writing state";
            const QString beginError = m_lastError;
            ImportBuildResult failure = ImportBuildResult::failure(
                ImportBuildStatus::BeginFailed, beginError);
            const ImportBuildResult cleanup = rollback();
            failure.recordRollback(cleanup);
            m_lastError = beginError;
            return failure;
        }
        return ImportBuildResult::success();
    } catch (...) {
        m_lastError = "failed to prepare sketch import";
        const QString beginError = m_lastError;
        ImportBuildResult failure = ImportBuildResult::failure(
            ImportBuildStatus::BeginFailed, beginError);
        const ImportBuildResult cleanup = rollback();
        failure.recordRollback(cleanup);
        m_lastError = beginError;
        return failure;
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
        // 将项目自己的 DxfPoint 适配为 SAM SDK 的 gslPoint，再让几何工厂创建线。
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
            // 长循环主动让 Qt 处理一次事件，进度条才能重绘、取消按钮才能响应；
            // 不能每条线都调用，否则事件分发开销会压垮大图导入。
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
        // SAM 的圆接口用“圆心 + 圆周上一点”确定圆，因此构造 (cx+r, cy, cz)。
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
        // 再次 Fetch 很重要：创建几何可能很久，若复用 beginImport 时的旧快照，
        // Replace 可能覆盖其他操作在这段时间写入的模型变化。
        basMdb mdb = basBasis::Instance()->Fetch();
        gmlSketchRepository& sketches = skcKGetSketchRepos(mdb, m_modelName);
        m_sketch->SetID(sketches.NextID());

        // Repository 接受 Wrapper 而不是裸 sketch。Insert 成功并 Replace 后才是持久化
        // 提交边界；后面的视图刷新失败只能告警，不能撤销已经保存的正确模型。
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
    // 以下都是“显示层”工作：清撤销栈、切换 Part 视口、重建 PDO、正视图、当前路径。
    // 它们不参与几何持久化，因此与上面的 Repository commit 分开。
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
    // `[this]` 捕获当前对象指针，使无参数 lambda 能调用成员清理函数。
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

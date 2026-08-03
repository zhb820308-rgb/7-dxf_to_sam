// ============================================================================
// FE 路线用例
// ============================================================================
#include "DxfImportApplication.h"

#include "FeConversion.h"
#include "DxfData.h"
#include "DxfImportRuntime.h"
#include "FeImport.h"

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

DxfImportOutcome runFiniteElementImport(
	const DxfImportRequest& request,
	DxfData& dxfData,
	std::size_t outputLimit,
	DxfImportSession& session,
	QElapsedTimer& stageTimer)
{
	// FE 分支的数据逐级变换：DxfData -> FeData(nodes/trusses) -> SAM Part。
	stageTimer.restart();
	DxfImportProgress progress(DxfImportProgressMode::FiniteElement);
	FeData feData;
	FeConversionEngine converter;
	converter.setProgressCallback(
		// `[&progress]` 是 lambda 捕获：按引用使用外层进度对象，避免复制，也让转换器
		// 每次汇报都更新同一个对话框。lambda 返回 false 时表示用户请求取消。
		[&progress](const QString& stage, int current, int total) {
			return progress.update(stage, current, total);
		});
	if (!converter.convert(
			dxfData, request.baseX, request.baseY, request.baseZ,
			request.curveTolerance, request.nodeMergeTolerance,
			feData, outputLimit))
	{
		if (feData.errorCode() == DxfImportErrorCode::Canceled)
			return canceledImport(session, progress, QLatin1Char('-'));
		const QString errorCode =
			DxfImportFormatting::errorCodeText(feData.errorCode());
		const QString errorMessage = feData.errorMessage().isEmpty()
			? QStringLiteral("no valid FE nodes to import")
			: feData.errorMessage();
		DxfImportFeedback::showBudgetError(
			feData.errorCode(), request.maxOutputEntities);
		return failedImport(
			session,
			DxfImportOutcomeStatus::ConvertFailed,
			feData.errorCode(),
			"fe_conversion",
			" error_code=" + errorCode.toStdString() +
			" error=\"" + errorMessage.toLocal8Bit().toStdString() + "\"",
			QString("[importDxf] ERROR [%1]: %2")
				.arg(errorCode, errorMessage));
	}

	DxfImportFeedback::showSmallDrawingRecommendation(
		feData.nodes().size() + feData.trusses().size(),
		request.maxOutputEntities);
	// 转换成功后原始 DXF 数据不再需要，提前释放大图占用的 vector 内存/元素。
	dxfData.clear();
	const FeConversionStats stats = feData.stats();
	if (session.logger())
	{
		session.logger()->info(
			"[import={}] fe_conversion_completed nodes={} trusses={}"
			" points_processed={}"
			" merged={} skipped_zero_length={} skipped_dup_truss={}"
			" curve_tolerance={} node_merge_tolerance={} duration_ms={}",
			session.importId(), feData.nodes().size(), feData.trusses().size(),
			stats.pointsProcessed,
			stats.mergedNodes, stats.skippedZeroLength,
			stats.skippedDuplicateTruss, request.curveTolerance,
			request.nodeMergeTolerance, stageTimer.elapsed());
	}

	stageTimer.restart();
	PythonFiniteElementBuilder builder;
	builder.setProgressCallback(
		[&progress](const QString& stage, int current, int total) {
			return progress.update(stage, current, total);
		});
	const ImportBuildResult buildResult = DxfImportBuildService::buildFePart(
		// BuildService 固定 begin/create/commit/rollback 顺序；具体 SAM 操作由 builder 做。
		feData, request.modelName, request.partName, builder);
	if (!buildResult.succeeded())
	{
		progress.close();
		const bool canceled =
			buildResult.status == ImportBuildStatus::Canceled;
		const std::string stage =
			DxfImportFormatting::buildStage(buildResult.status);
		std::string detail =
			" error=\"" + buildResult.message.toLocal8Bit().toStdString() + "\"";
		if (!canceled && buildResult.status != ImportBuildStatus::BeginFailed)
		{
			detail = " model=\"" + request.modelName.toLocal8Bit().toStdString() +
				"\" part=\"" + request.partName.toLocal8Bit().toStdString() +
				"\"" + detail;
		}
		const QString warning = canceled
			? QString("[importDxf] IMPORT CANCELED - %1 %2/%3")
				.arg(progress.canceledStage())
				.arg(progress.canceledCurrent())
				.arg(progress.canceledTotal())
			: QString("[importDxf] ERROR: FE build failed, rolling back - %1")
				.arg(buildResult.message);
		return failedBuildImport(
			session, buildResult, stage, detail, warning);
	}

	progress.complete();
	if (session.logger())
	{
		session.logger()->info(
			"[import={}] succeeded mode=FiniteElement model=\"{}\" part=\"{}\""
			" nodes={} trusses={} submitted={} build_duration_ms={}"
			" total_duration_ms={}",
			session.importId(),
			request.modelName.toLocal8Bit().toStdString(),
			request.partName.toLocal8Bit().toStdString(),
			feData.nodes().size(), feData.trusses().size(),
			buildResult.createdCount, stageTimer.elapsed(), session.elapsed());
	}
	session.finish();
	return DxfImportOutcome::success(buildResult.createdCount);
}

// ============================================================================
// SAM FE 写入
// ============================================================================

#include <QCoreApplication>

#include <basBasis.h>
#include <basMdb.h>
#include <ptoKPartRepository.h>
#include <ptoKUtils.h>
#include <pytInterpreterRole.h>

#include <algorithm>
#include <chrono>

PythonFiniteElementBuilder::~PythonFiniteElementBuilder()
{
    if (m_transaction.ownsResource())
        rollback();
}

QString PythonFiniteElementBuilder::pythonStringLiteral(const QString& value)
{
    // 用户输入会被嵌入 Python 源码，必须转义反斜杠、引号和换行。否则带引号的名称
    // 会破坏命令，甚至改变命令含义。这和 SQL 参数化要解决的是同一类边界问题。
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
    // pytInterpreterRole 是 SAM 内嵌 Python 解释器的单例入口，不会启动外部 python.exe。
    pytInterpreterRole& interpreter = pytInterpreterRole::Instance();
    try {
        interpreter.RunCommand(command, false);
    } catch (...) {
        m_lastError = QString("%1 failed: Python interpreter raised an exception")
            .arg(stage);
        qWarning().noquote() << "[PythonFiniteElementBuilder]" << m_lastError;
        return false;
    }

    // 有些解释器错误不会以 C++ 异常越过边界，所以还必须检查 Python traceback。
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

bool PythonFiniteElementBuilder::partExists(bool* querySucceeded) const
{
    try {
        basMdb mdb = basBasis::Instance()->Fetch();
        ptoKPartRepository& parts = ptoKGetPartRepos(mdb, m_modelName);
        if (querySucceeded)
            *querySucceeded = true;
        return parts.IsMember(m_partName);
    } catch (...) {
        if (querySucceeded)
            *querySucceeded = false;
        return false;
    }
}

bool PythonFiniteElementBuilder::removeOwnedPart()
{
    bool querySucceeded = false;
    if (!partExists(&querySucceeded)) {
        if (querySucceeded)
            return true;
        m_lastError = QString("rollback failed: cannot inspect model '%1'")
            .arg(m_modelName);
        return false;
    }

    const QString command = QString("del mdb.models[%1].parts[%2]")
        .arg(pythonStringLiteral(m_modelName), pythonStringLiteral(m_partName));
    if (!runCommand(command, QStringLiteral("remove partial Part")))
        return false;

    if (partExists(&querySucceeded)) {
        m_lastError = QString("rollback failed: Part '%1' still exists in model '%2'")
            .arg(m_partName, m_modelName);
        return false;
    }
    if (!querySucceeded) {
        m_lastError = QString("rollback failed: cannot verify removal of Part '%1'")
            .arg(m_partName);
        return false;
    }
    return true;
}

bool PythonFiniteElementBuilder::isWriting() const
{
    return m_transaction.state() == ImportTransactionState::Writing;
}

ImportBuildResult PythonFiniteElementBuilder::beginImport(
    const QString& modelName, const QString& partName)
{
    // auto 由右侧推导时间点类型；steady_clock 不受系统校时影响，适合测耗时。
    const auto startedAt = std::chrono::steady_clock::now();
    m_lastError.clear();
    m_modelName = modelName;
    m_partName = partName;
    m_createdNodeCount = 0;
    m_createdTrussCount = 0;

    if (!m_transaction.begin()) {
        m_lastError = QStringLiteral("beginImport: transaction is already active or committed");
        return ImportBuildResult::failure(
            ImportBuildStatus::BeginFailed, m_lastError);
    }

    if (modelName.isEmpty() || partName.isEmpty()) {
        m_lastError = QStringLiteral("modelName and partName must not be empty");
        m_transaction.rollback([]() { return true; });
        return ImportBuildResult::failure(
            ImportBuildStatus::BeginFailed, m_lastError);
    }

    try {
        basMdb mdb = basBasis::Instance()->Fetch();
        ptoKPartRepository& parts = ptoKGetPartRepos(mdb, modelName);
        if (parts.IsMember(partName)) {
            m_lastError = QString("Part '%1' already exists in model '%2'")
                .arg(partName, modelName);
            m_transaction.rollback([]() { return true; });
            return ImportBuildResult::failure(
                ImportBuildStatus::BeginFailed, m_lastError);
        }
    } catch (...) {
        m_lastError = QString("model '%1' was not found").arg(modelName);
        m_transaction.rollback([]() { return true; });
        return ImportBuildResult::failure(
            ImportBuildStatus::BeginFailed, m_lastError);
    }

    // 先创建 Part；只有命令成功后才进入 Writing。markResourceOwned 放在命令前，
    // 即使解释器执行到一半后报错，rollback 也会尝试清理可能已产生的半成品。
    m_partExpression = QString("mdb.models[%1].parts[%2]")
        .arg(pythonStringLiteral(modelName), pythonStringLiteral(partName));
    const QString command = QString("import samConstants\n"
        "_example1_fe_model = mdb.models[%1]\n"
        "_example1_fe_part = _example1_fe_model.Part(name=%2)")
        .arg(pythonStringLiteral(modelName), pythonStringLiteral(partName));
    m_transaction.markResourceOwned();
    if (!runCommand(command, QStringLiteral("create Part"))) {
        const QString createError = m_lastError;
        ImportBuildResult failure = ImportBuildResult::failure(
            ImportBuildStatus::BeginFailed, createError);
        const ImportBuildResult cleanup = rollback();
        failure.recordRollback(cleanup);
        m_lastError = createError;
        return failure;
    }

    if (!m_transaction.startWriting()) {
        m_lastError = QStringLiteral("create Part succeeded but transaction transition failed");
        const QString transitionError = m_lastError;
        ImportBuildResult failure = ImportBuildResult::failure(
            ImportBuildStatus::BeginFailed, transitionError);
        const ImportBuildResult cleanup = rollback();
        failure.recordRollback(cleanup);
        m_lastError = transitionError;
        return failure;
    }
    qInfo().noquote() << "[PythonFiniteElementBuilder] beginImport completed in"
                      << std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - startedAt).count()
                      << "ms";
    return ImportBuildResult::success();
}

ImportBuildResult PythonFiniteElementBuilder::createNodes(
    const std::vector<FeNode>& nodes)
{
    const auto startedAt = std::chrono::steady_clock::now();
    if (!isWriting()) {
        m_lastError = QStringLiteral("createNodes: no active import context");
        return ImportBuildResult::failure(
            ImportBuildStatus::CreateFailed, m_lastError);
    }

    const int total = static_cast<int>(nodes.size());
    for (const FeNode& node : nodes) {
        if (!DxfNumeric::areFinite(node.x, node.y, node.z)) {
            m_lastError = QString("createNodes: invalid coordinate for node %1").arg(node.id);
            return ImportBuildResult::failure(
                ImportBuildStatus::CreateFailed, m_lastError,
                m_createdNodeCount);
        }
    }

    // 分批拼命令显著减少 C++↔Python 往返；std::max 保证误设为 0 时仍至少一条/批。
    const int batchSize = std::max(1, m_batchSize);
    for (int first = 0; first < total; first += batchSize) {
        const int last = std::min(first + batchSize, total);
        QString command;
        // reserve 只预留容量、不改变字符串内容，避免 append 时反复重新分配内存。
        command.reserve((last - first) * 72 + 128);
        command.append("for _example1_fe_x, _example1_fe_y, _example1_fe_z in (\n");
        for (int i = first; i < last; ++i) {
            // vector 下标类型是 size_t；显式转换消除有符号 int 与无符号下标的混用。
            const FeNode& node = nodes[static_cast<std::size_t>(i)];
            command.append("    (")
                // double 最多 17 位有效数字可保证往返精度，避免几何坐标被格式化截断。
                .append(QString::number(node.x, 'g', 17))
                .append(", ")
                .append(QString::number(node.y, 'g', 17))
                .append(", ")
                .append(QString::number(node.z, 'g', 17))
                .append("),\n");
        }
        command.append("):\n")
            .append("    _example1_fe_part.createNode(x=_example1_fe_x, y=_example1_fe_y, z=_example1_fe_z)\n");
        if (!runCommand(command,
                        QString("create nodes %1-%2").arg(first).arg(last - 1)))
            return ImportBuildResult::failure(
                ImportBuildStatus::CreateFailed, m_lastError,
                m_createdNodeCount);

        m_createdNodeCount += last - first;
        if (!reportProgress(QStringLiteral("Creating FE nodes"), m_createdNodeCount, total))
            return ImportBuildResult::failure(
                ImportBuildStatus::Canceled, m_lastError,
                m_createdNodeCount);
    }
    qInfo().noquote() << "[PythonFiniteElementBuilder] createNodes:"
                      << m_createdNodeCount << "nodes in"
                      << std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - startedAt).count()
                      << "ms; batch size:" << batchSize;
    return ImportBuildResult::success(m_createdNodeCount);
}

ImportBuildResult PythonFiniteElementBuilder::createTrusses(
    const std::vector<FeTruss>& trusses)
{
    const auto startedAt = std::chrono::steady_clock::now();
    if (!isWriting()) {
        m_lastError = QStringLiteral("createTrusses: no active import context");
        return ImportBuildResult::failure(
            ImportBuildStatus::CreateFailed, m_lastError);
    }

    const int total = static_cast<int>(trusses.size());
    for (const FeTruss& truss : trusses) {
        if (truss.startNodeId < 0 || truss.endNodeId < 0 ||
            truss.startNodeId >= m_createdNodeCount ||
            truss.endNodeId >= m_createdNodeCount)
        {
            m_lastError = QString("createTrusses: invalid node ID in truss %1")
                .arg(truss.id);
            return ImportBuildResult::failure(
                ImportBuildStatus::CreateFailed, m_lastError,
                m_createdTrussCount);
        }
    }

    const int batchSize = std::max(1, m_batchSize);
    for (int first = 0; first < total; first += batchSize) {
        const int last = std::min(first + batchSize, total);
        QString command;
        command.reserve((last - first) * 24 + 280);
        command.append("_example1_fe_nodes = _example1_fe_part.nodes\n");
        command.append("for _example1_fe_start, _example1_fe_end in (\n");
        for (int i = first; i < last; ++i) {
            const FeTruss& truss = trusses[static_cast<std::size_t>(i)];
            command.append("    (")
                .append(QString::number(truss.startNodeId))
                .append(", ")
                .append(QString::number(truss.endNodeId))
                .append("),\n");
        }
        command.append("):\n")
            // SAM Python 接口说明确认 Part.Element(nodes=..., elemShape=TRUSS) 创建两节点
            // 桁架单元。intersectNodes=False：节点已在 FeData 中完成合并，不让 SDK再改。
            .append("    _example1_fe_part.Element(\n")
            .append("        nodes=(_example1_fe_nodes[_example1_fe_start], _example1_fe_nodes[_example1_fe_end]),\n")
            .append("        elemShape=samConstants.TRUSS, intersectNodes=False)\n");
        if (!runCommand(command,
                        QString("create trusses %1-%2").arg(first).arg(last - 1)))
            return ImportBuildResult::failure(
                ImportBuildStatus::CreateFailed, m_lastError,
                m_createdTrussCount);

        m_createdTrussCount += last - first;
        if (!reportProgress(QStringLiteral("Creating FE trusses"), m_createdTrussCount, total))
            return ImportBuildResult::failure(
                ImportBuildStatus::Canceled, m_lastError,
                m_createdTrussCount);
    }
    qInfo().noquote() << "[PythonFiniteElementBuilder] createTrusses:"
                      << m_createdTrussCount << "trusses in"
                      << std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - startedAt).count()
                      << "ms; batch size:" << batchSize;
    return ImportBuildResult::success(m_createdTrussCount);
}

ImportBuildResult PythonFiniteElementBuilder::commit()
{
    if (!m_transaction.startCommitting()) {
        m_lastError = QStringLiteral("commit: no active import context");
        return ImportBuildResult::failure(
            ImportBuildStatus::CommitFailed, m_lastError,
            m_createdNodeCount + m_createdTrussCount);
    }

    const QString command = QString(
        "_example1_fe_vp = session.viewports[%1]\n"
        "_example1_fe_vp.setValues(displayedObject=_example1_fe_part)\n"
        "_example1_fe_vp.view.fitView()")
        .arg(pythonStringLiteral(QStringLiteral("Viewport: 1")));
    if (!runCommand(command, QStringLiteral("display FE Part")))
        return ImportBuildResult::failure(
            ImportBuildStatus::CommitFailed, m_lastError,
            m_createdNodeCount + m_createdTrussCount);

    if (!m_transaction.markCommitted()) {
        m_lastError = QStringLiteral("commit: transaction finalization failed");
        return ImportBuildResult::failure(
            ImportBuildStatus::CommitFailed, m_lastError,
            m_createdNodeCount + m_createdTrussCount);
    }
    return ImportBuildResult::success(
        m_createdNodeCount + m_createdTrussCount);
}

ImportBuildResult PythonFiniteElementBuilder::rollback()
{
    bool cleaned = false;
    try {
        cleaned = m_transaction.rollback([this]() {
            return removeOwnedPart();
        });
    } catch (...) {
        m_lastError = QStringLiteral("rollback failed with an unexpected exception");
    }
    if (!cleaned)
        qWarning().noquote() << "[PythonFiniteElementBuilder]" << m_lastError;
    if (cleaned)
        return ImportBuildResult::success();
    if (m_lastError.isEmpty())
        m_lastError = QStringLiteral(
            "rollback failed: transaction cannot be rolled back");
    return ImportBuildResult::failure(
        ImportBuildStatus::RollbackFailed, m_lastError,
        m_createdNodeCount + m_createdTrussCount);
}

// ============================================================================
// 公共错误处理与模式分发
// ============================================================================
#include "DxfImportApplication.h"
#include "FeImport.h"
#include "SketchImport.h"

#include "DxfData.h"
#include "DxfImportRuntime.h"
#include "DxfParser.h"

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

DxfImportOutcome runDxfImport(const DxfImportRequest& request)
{
	// Session 使用 RAII（对象随作用域自动析构）统一管理本次导入 ID、耗时和日志。
	DxfImportSession session(
		request.filePath, request.baseX, request.baseY, request.baseZ,
		request.curveTolerance, request.maxOutputEntities);

	const DxfImportPreflightResult preflight =
		validateDxfImportRequest(request);
	// 尽早失败：文件、数值范围、模式和输出限制不合法时，不进入昂贵解析阶段。
	if (!preflight.valid)
	{
		return failedImport(
			session,
			DxfImportOutcomeStatus::ValidationFailed,
			DxfImportErrorCode::InvalidArgument,
			preflight.stage, preflight.detail, preflight.message);
	}
	const std::set<std::string> ignoredLayers =
		parseIgnoredDxfLayers(request.ignoredLayers);

	QElapsedTimer stageTimer;
	stageTimer.start();
	DxfData dxfData;
	DxfParser parser;
	// out 参数 dxfData 由 parser 填充；bool 只表示成功/失败，具体错误保存在 dxfData。
	if (!parser.parseFile(
			request.filePath, dxfData, request.curveTolerance,
			ignoredLayers, preflight.outputLimit))
	{
		const QString errorCode =
			DxfImportFormatting::errorCodeText(dxfData.errorCode());
		const QString errorMessage = dxfData.errorMessage();
		DxfImportFeedback::showBudgetError(
			dxfData.errorCode(), request.maxOutputEntities);
		return failedImport(
			session,
			DxfImportOutcomeStatus::ParseFailed,
			dxfData.errorCode(),
			"parse",
			" error_code=" + errorCode.toStdString() +
			" error=\"" + errorMessage.toLocal8Bit().toStdString() + "\"",
			QString("[importDxf] ERROR [%1]: DXF parse failed - %2")
				.arg(errorCode, errorMessage));
	}

	if (session.logger())
	{
		session.logger()->info(
			"[import={}] parse_completed entities={} points={} lines={} circles={}"
			" arcs={} lw_polylines={} ellipses={} duration_ms={}",
			session.importId(), dxfData.entityCount(), dxfData.points().size(),
			dxfData.lines().size(), dxfData.circles().size(),
			dxfData.arcs().size(), dxfData.lwPolylines().size(),
			dxfData.ellipses().size(), stageTimer.elapsed());
	}
	logRawDxfData(session.logger(), session.importId(), dxfData);
	const DxfEntityStats entityStats = dxfData.entityStats();

	// 这里是唯一模式分叉点。前面的输入校验和 DXF 解析完全复用，后面的转换器和
	// Builder 各自适配 Sketch 或有限元 Part。
	if (preflight.mode == DxfImportMode::FiniteElement)
	{
		return runFiniteElementImport(
			request, dxfData, preflight.outputLimit, session, stageTimer);
	}
	return runSketchImport(
		request, dxfData, entityStats,
		preflight.outputLimit, session, stageTimer);
}

// ============================================================================
// Qt 提示、格式化和进度窗口
// ============================================================================


#include <QCoreApplication>
#include <QEventLoop>
#include <QMessageBox>
#include <QStringList>

namespace {

bool isBudgetError(DxfImportErrorCode code)
{
    return code == DxfImportErrorCode::ExpansionLimit
        || code == DxfImportErrorCode::ConversionLimit;
}

QString boolText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString splineKindText(const SplineKind& kind)
{
    const QString construction =
        kind.construction == SplineConstruction::ControlBased
        ? QStringLiteral("ControlBased")
        : QStringLiteral("FitBased");
    return QStringLiteral("%1|rational=%2|periodic=%3|closed=%4")
        .arg(construction)
        .arg(boolText(kind.rational))
        .arg(boolText(kind.periodic))
        .arg(boolText(kind.closed));
}

constexpr int kMaxEventProcessingTimeMs = 25;

void processPendingUiEvents()
{
    // 导入在 GUI 线程中同步执行；短暂处理事件，窗口才能重绘并响应取消按钮。
    QCoreApplication::processEvents(
        QEventLoop::AllEvents, kMaxEventProcessingTimeMs);
}

} // namespace

namespace DxfImportFeedback {

void showBudgetError(
    DxfImportErrorCode code,
    int maxOutputEntities)
{
    if (!isBudgetError(code))
        return;
    if (maxOutputEntities < 0)
    {
        QMessageBox::warning(
            nullptr,
            QStringLiteral("DXF Import - Expansion Safety Limit"),
            QStringLiteral(
                "The drawing exceeded an INSERT expansion safety limit.\n\n"
                "Unlimited mode only removes the final output limit. "
                "Please simplify the block structure or reduce the array size."));
        return;
    }
    if (maxOutputEntities <= static_cast<int>(
            DxfImportValidation::kSmallDrawingEntityLimit))
    {
        QMessageBox::warning(
            nullptr,
            QStringLiteral("DXF Import - Drawing Too Large"),
            QStringLiteral(
                "The drawing exceeds the small drawing limit of 100,000 entities.\n\n"
                "Please reopen the DXF Import dialog and select Large drawing."));
        return;
    }

    QMessageBox::warning(
        nullptr,
        QStringLiteral("DXF Import - Drawing Too Large"),
        QStringLiteral(
            "The drawing still exceeds the selected large drawing limit of %1 entities.\n\n"
            "Increase the curve tolerance or simplify the drawing before importing.")
            .arg(maxOutputEntities));
}

void showSmallDrawingRecommendation(
    std::size_t outputEntities,
    int maxOutputEntities)
{
    if (maxOutputEntities == static_cast<int>(
            DxfImportValidation::kSmallDrawingEntityLimit)
        || outputEntities > DxfImportValidation::kSmallDrawingEntityLimit)
    {
        return;
    }

    QMessageBox::information(
        nullptr,
        QStringLiteral("DXF Import Recommendation"),
        QStringLiteral(
            "The converted result contains %1 entities, which is within the small drawing limit.\n\n"
            "For lower memory usage, consider selecting Small drawing next time.")
            .arg(static_cast<qulonglong>(outputEntities)));
}

} // namespace DxfImportFeedback

namespace DxfImportFormatting {

QString errorCodeText(DxfImportErrorCode code)
{
    switch (code)
    {
    case DxfImportErrorCode::InvalidArgument: return QStringLiteral("INVALID_ARGUMENT");
    case DxfImportErrorCode::ReadFailed: return QStringLiteral("READ_FAILED");
    case DxfImportErrorCode::ExpansionLimit: return QStringLiteral("EXPANSION_LIMIT");
    case DxfImportErrorCode::ConversionLimit: return QStringLiteral("CONVERSION_LIMIT");
    case DxfImportErrorCode::NoSupportedEntities: return QStringLiteral("NO_SUPPORTED_ENTITIES");
    case DxfImportErrorCode::ConversionFailed: return QStringLiteral("CONVERSION_FAILED");
    case DxfImportErrorCode::Canceled: return QStringLiteral("CANCELED");
    case DxfImportErrorCode::BeginFailed: return QStringLiteral("BEGIN_FAILED");
    case DxfImportErrorCode::CreateFailed: return QStringLiteral("CREATE_FAILED");
    case DxfImportErrorCode::CommitFailed: return QStringLiteral("COMMIT_FAILED");
    case DxfImportErrorCode::RollbackFailed: return QStringLiteral("ROLLBACK_FAILED");
    case DxfImportErrorCode::None: break;
    }
    return QStringLiteral("NONE");
}

const char* buildStage(ImportBuildStatus status)
{
    switch (status)
    {
    case ImportBuildStatus::Canceled: return "canceled";
    case ImportBuildStatus::BeginFailed: return "begin_import";
    case ImportBuildStatus::CreateFailed: return "create";
    case ImportBuildStatus::CommitFailed: return "commit";
    case ImportBuildStatus::RollbackFailed: return "rollback";
    case ImportBuildStatus::Success: break;
    }
    return "build";
}

QString summaryText(int created, const DxfEntityStats& stats)
{
    QStringList splineCategories;
    for (const auto& entry : stats.splineKinds)
    {
        if (entry.second == 0)
            continue;
        splineCategories.append(
            QStringLiteral("%1=%2")
            .arg(splineKindText(entry.first))
            .arg(static_cast<qulonglong>(entry.second)));
    }

    return QStringLiteral(
        "[importDxf] Import complete: imported entities=%1; source=%2, accepted=%3, rejected=%4, generated=%5; "
        "types (after layer filter and block expansion): points=%6 "
        "(Sketch ignored, FE standalone nodes), lines=%7, polylines=%8, curves=%9 "
        "(circles=%10, arcs=%11, ellipses=%12, splines=%13; spline categories=[%14])")
        .arg(created)
        .arg(static_cast<qulonglong>(stats.sourceEntities))
        .arg(static_cast<qulonglong>(stats.acceptedEntities))
        .arg(static_cast<qulonglong>(stats.rejectedEntities))
        .arg(static_cast<qulonglong>(stats.generatedEntities))
        .arg(static_cast<qulonglong>(stats.points))
        .arg(static_cast<qulonglong>(stats.lines))
        .arg(static_cast<qulonglong>(stats.lwPolylines))
        .arg(static_cast<qulonglong>(stats.curveCount()))
        .arg(static_cast<qulonglong>(stats.circles))
        .arg(static_cast<qulonglong>(stats.arcs))
        .arg(static_cast<qulonglong>(stats.ellipses))
        .arg(static_cast<qulonglong>(stats.splineCount()))
        .arg(splineCategories.join(QStringLiteral(", ")));
}

} // namespace DxfImportFormatting

DxfImportProgress::DxfImportProgress(
    DxfImportProgressMode mode,
    std::size_t sketchLineCount,
    std::size_t sketchCircleCount)
    : m_mode(mode),
      m_sketchLineCount(sketchLineCount),
      m_totalSketchEntities(static_cast<int>(
          sketchLineCount + sketchCircleCount)),
      m_dialog(
          mode == DxfImportProgressMode::FiniteElement
              ? QStringLiteral("Importing DXF as FE Part...")
              : QStringLiteral("Importing DXF..."),
          QStringLiteral("Cancel"), 0, 100)
{
    m_dialog.setWindowTitle(
        mode == DxfImportProgressMode::FiniteElement
            ? QStringLiteral("DXF Import (FE)")
            : QStringLiteral("DXF Import"));
    m_dialog.setWindowModality(Qt::ApplicationModal);
    m_dialog.setMinimumDuration(0);
    m_dialog.show();
    processPendingUiEvents();
}

DxfImportProgress::~DxfImportProgress()
{
    close();
}

bool DxfImportProgress::update(
    const QString& stage,
    int current,
    int total)
{
    if (m_canceled || m_closed)
        return false;

    int value = 20;
    if (m_mode == DxfImportProgressMode::FiniteElement)
    {
        int offset = 60;
        int span = 40;
        if (stage == QStringLiteral("Merging FE points"))
        {
            offset = 0;
            span = 10;
        }
        else if (stage == QStringLiteral("Merging FE lines"))
        {
            offset = 10;
            span = 10;
        }
        else if (stage == QStringLiteral("Creating FE nodes"))
        {
            offset = 20;
            span = 40;
        }
        value = offset;
        if (total > 0)
        {
            value = offset + static_cast<int>(
                static_cast<double>(span) * current / total);
        }
    }
    else if (stage == QStringLiteral("Creating lines"))
    {
        if (m_totalSketchEntities > 0)
        {
            value = 20 + static_cast<int>(
                79.0 * current / m_totalSketchEntities);
        }
    }
    else if (stage == QStringLiteral("Creating circles"))
    {
        if (m_totalSketchEntities > 0)
        {
            value = 20 + static_cast<int>(
                79.0 * (m_sketchLineCount + current)
                / m_totalSketchEntities);
        }
    }
    else if (stage == QStringLiteral("Finalizing import"))
    {
        value = 99;
    }

    m_dialog.setLabelText(
        QStringLiteral("%1: %2 / %3").arg(stage).arg(current).arg(total));
    m_dialog.setValue(value);
    processPendingUiEvents();
    if (!m_dialog.wasCanceled())
        return true;

    m_canceledStage = stage;
    m_canceledCurrent = current;
    m_canceledTotal = total;
    m_canceled = true;
    close();
    return false;
}

void DxfImportProgress::complete()
{
    if (m_closed)
        return;
    m_dialog.setValue(100);
    close();
}

void DxfImportProgress::close()
{
    if (m_closed)
        return;
    m_dialog.close();
    m_closed = true;
}

#include "DxfImportLogger.h"

#include "DxfData.h"
#include "SamData.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QStandardPaths>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>

#include <algorithm>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

// ============================================================================
// Internal helpers
// ============================================================================

static const int kMaxImportLogs = 50;

static QString dxfLogRootDirectory()
{
	// Prefer SAM's own directory for user-friendly access, but fall back
	// to a user-writable location when SAM is installed under a protected
	// folder such as "Program Files".
	const QString appDir = QCoreApplication::applicationDirPath();
	const QString preferredPath = QDir(appDir).filePath("logs");

	// Create the directory first so we can test writability.
	if (!QDir().mkpath(preferredPath))
	{
		qWarning() << "[importDxf] Cannot create log directory:"
			<< preferredPath;
	}

	// Verify writability by creating a temporary test file.
	const QString testFilePath =
		QDir(preferredPath).filePath(".dzf_log_test");
	QFile testFile(testFilePath);
	const bool writable = testFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
	if (writable)
	{
		testFile.close();
		testFile.remove();
		return preferredPath;
	}

	const QString fallbackPath =
		QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
		+ "/logs";
	qWarning() << "[importDxf] Log directory" << preferredPath
		<< "is not writable, using" << fallbackPath << "instead.";
	return fallbackPath;
}

static void retainRecentImportLogs(
	const QString& importLogDirectory,
	const QString& currentLogPath)
{
	QDir directory(importLogDirectory);
	QFileInfoList logFiles = directory.entryInfoList(
		QStringList() << "dxf_import_*.log",
		QDir::Files | QDir::NoSymLinks,
		QDir::NoSort);

	std::sort(logFiles.begin(), logFiles.end(),
		[](const QFileInfo& left, const QFileInfo& right) {
			if (left.lastModified() != right.lastModified())
				return left.lastModified() > right.lastModified();
			return left.fileName() > right.fileName();
		});

	const QString currentAbsolutePath = QFileInfo(currentLogPath).absoluteFilePath();
	int retainedCount = 1;
	for (QFileInfoList::const_iterator it = logFiles.constBegin();
		 it != logFiles.constEnd(); ++it)
	{
		if (it->absoluteFilePath() == currentAbsolutePath)
			continue;

		if (retainedCount < kMaxImportLogs)
		{
			++retainedCount;
			continue;
		}

		if (!QFile::remove(it->absoluteFilePath()))
		{
			qWarning() << "[importDxf] Failed to remove old import log:"
				<< it->absoluteFilePath();
		}
	}
}

// ============================================================================
// Public API
// ============================================================================

std::shared_ptr<spdlog::logger> createDxfImportLogger(
	const std::string& importId)
{
	try
	{
		const QString importLogDirectory =
			QDir(dxfLogRootDirectory()).filePath("imports");
		if (!QDir().mkpath(importLogDirectory))
		{
			qWarning() << "[importDxf] Failed to create import log directory:"
				<< importLogDirectory;
			return nullptr;
		}

		const QString baseFileName = QString("dxf_import_%1")
			.arg(QString::fromStdString(importId));
		QString logPath = QDir(importLogDirectory).filePath(baseFileName + ".log");
		int duplicateIndex = 1;
		while (QFileInfo::exists(logPath))
		{
			logPath = QDir(importLogDirectory).filePath(
				QString("%1_%2.log").arg(baseFileName).arg(duplicateIndex++));
		}

		const std::string nativeLogPath =
			QDir::toNativeSeparators(logPath).toLocal8Bit().toStdString();
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
			nativeLogPath, true);
		std::shared_ptr<spdlog::logger> logger =
			std::make_shared<spdlog::logger>(
				"dxf_import_" + importId, sink);
		logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] %v");
		logger->set_level(spdlog::level::trace);
		logger->flush_on(spdlog::level::warn);

		retainRecentImportLogs(importLogDirectory, logPath);
		return logger;
	}
	catch (const spdlog::spdlog_ex& error)
	{
		qWarning() << "[importDxf] Failed to initialize import log:"
			<< error.what();
		return nullptr;
	}
}

std::shared_ptr<spdlog::logger> dxfErrorLogger()
{
	static std::shared_ptr<spdlog::logger> logger = []() {
		try
		{
			if (std::shared_ptr<spdlog::logger> existing =
				spdlog::get("dxf_import_errors"))
				return existing;

			const QString logDirectory = dxfLogRootDirectory();
			if (!QDir().mkpath(logDirectory))
			{
				qWarning() << "[importDxf] Failed to create log directory:"
					<< logDirectory;
				return std::shared_ptr<spdlog::logger>();
			}

			const QString logPath =
				QDir(logDirectory).filePath("dxf_import_errors.log");
			const std::string nativeLogPath =
				QDir::toNativeSeparators(logPath).toLocal8Bit().toStdString();

			std::shared_ptr<spdlog::logger> result = spdlog::rotating_logger_mt(
				"dxf_import_errors", nativeLogPath, 10 * 1024 * 1024, 5);
			result->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] %v");
			result->set_level(spdlog::level::err);
			result->flush_on(spdlog::level::err);
			return result;
		}
		catch (const spdlog::spdlog_ex& error)
		{
			qWarning() << "[importDxf] Failed to initialize error log:"
				<< error.what();
			return std::shared_ptr<spdlog::logger>();
		}
	}();

	return logger;
}

void dropDxfImportLogger(const std::string& importId)
{
	spdlog::drop("dxf_import_" + importId);
}

// ============================================================================
// Data-logging helpers (template eliminates ~125 lines of duplicate loops)
// ============================================================================

template <typename Entity, typename LogFunc>
static void logEntities(
	const std::shared_ptr<spdlog::logger>& logger,
	const std::string& importId,
	const std::string& tag,
	const std::vector<Entity>& entities,
	LogFunc formatter)
{
	if (!logger)
		return;

	for (size_t i = 0; i < entities.size(); ++i)
	{
		formatter(logger, importId, tag, i, entities[i]);
	}
}

void logRawDxfData(
	const std::shared_ptr<spdlog::logger>& logger,
	const std::string& importId,
	const DxfData& data)
{
	if (!logger)
		return;

	logEntities(logger, importId, "raw", data.points(),
		[](const std::shared_ptr<spdlog::logger>& log, const std::string& id,
		   const std::string& tag, size_t, const DxfPoint& pt) {
			log->trace(
				"[import={}] {} POINT id={} position=({}, {}, {})",
				id, tag, pt.getId(), pt.x(), pt.y(), pt.z());
		});

	logEntities(logger, importId, "raw", data.lines(),
		[](const std::shared_ptr<spdlog::logger>& log, const std::string& id,
		   const std::string& tag, size_t, const DxfLine& line) {
			log->trace(
				"[import={}] {} LINE id={} start=({}, {}, {}) end=({}, {}, {})",
				id, tag, line.getId(),
				line.start().x(), line.start().y(), line.start().z(),
				line.end().x(), line.end().y(), line.end().z());
		});

	logEntities(logger, importId, "raw", data.circles(),
		[](const std::shared_ptr<spdlog::logger>& log, const std::string& id,
		   const std::string& tag, size_t, const DxfCircle& circle) {
			log->trace(
				"[import={}] {} CIRCLE id={} center=({}, {}, {}) radius={}",
				id, tag, circle.getId(),
				circle.center().x(), circle.center().y(), circle.center().z(),
				circle.radius());
		});

	logEntities(logger, importId, "raw", data.arcs(),
		[](const std::shared_ptr<spdlog::logger>& log, const std::string& id,
		   const std::string& tag, size_t, const DxfArc& arc) {
			log->trace(
				"[import={}] {} ARC id={} center=({}, {}, {}) radius={}"
				" start_angle={} end_angle={} ccw={}",
				id, tag, arc.getId(),
				arc.center().x(), arc.center().y(), arc.center().z(),
				arc.radius(), arc.startAngle(), arc.endAngle(), arc.isCCW());
		});

	for (size_t i = 0; i < data.lwPolylines().size(); ++i)
	{
		const DxfLWPolyline& polyline = data.lwPolylines()[i];
		logger->trace(
			"[import={}] raw LWPOLYLINE id={} vertices={} closed={} const_z={}",
			importId, polyline.getId(), polyline.vertices().size(),
			polyline.isClosed(), polyline.constZ());

		for (size_t vi = 0; vi < polyline.vertices().size(); ++vi)
		{
			const DxfPoint& vertex = polyline.vertices()[vi];
			logger->trace(
				"[import={}] raw LWPOLYLINE id={} VERTEX index={} position=({}, {}, {})",
				importId, polyline.getId(), vi,
				vertex.x(), vertex.y(), vertex.z());
		}

		for (size_t si = 0; si < polyline.bulges().size(); ++si)
		{
			const size_t endIdx = (si + 1) % polyline.vertices().size();
			logger->trace(
				"[import={}] raw LWPOLYLINE id={} SEGMENT index={}"
				" start_vertex={} end_vertex={} bulge={}",
				importId, polyline.getId(), si, si, endIdx,
				polyline.bulges()[si]);
		}
	}

	logEntities(logger, importId, "raw", data.ellipses(),
		[](const std::shared_ptr<spdlog::logger>& log, const std::string& id,
		   const std::string& tag, size_t, const DxfEllipse& ellipse) {
			log->trace(
				"[import={}] {} ELLIPSE id={} center=({}, {}, {})"
				" major_axis=({}, {}, {}) ratio={} start_param={} end_param={} ccw={}",
				id, tag, ellipse.getId(),
				ellipse.center().x(), ellipse.center().y(), ellipse.center().z(),
				ellipse.majorAxisEnd().x(), ellipse.majorAxisEnd().y(),
				ellipse.majorAxisEnd().z(), ellipse.ratio(),
				ellipse.startParam(), ellipse.endParam(), ellipse.isCCW());
		});
}

void logConvertedSamData(
	const std::shared_ptr<spdlog::logger>& logger,
	const std::string& importId,
	const SamData& data)
{
	if (!logger)
		return;

	logEntities(logger, importId, "converted", data.points(),
		[](const std::shared_ptr<spdlog::logger>& log, const std::string& id,
		   const std::string& tag, size_t, const DxfPoint& pt) {
			log->trace(
				"[import={}] {} POINT id={} position=({}, {}, {})",
				id, tag, pt.getId(), pt.x(), pt.y(), pt.z());
		});

	logEntities(logger, importId, "converted", data.lines(),
		[](const std::shared_ptr<spdlog::logger>& log, const std::string& id,
		   const std::string& tag, size_t, const DxfLine& line) {
			log->trace(
				"[import={}] {} LINE id={} start=({}, {}, {}) end=({}, {}, {})",
				id, tag, line.getId(),
				line.start().x(), line.start().y(), line.start().z(),
				line.end().x(), line.end().y(), line.end().z());
		});

	logEntities(logger, importId, "converted", data.circles(),
		[](const std::shared_ptr<spdlog::logger>& log, const std::string& id,
		   const std::string& tag, size_t, const DxfCircle& circle) {
			log->trace(
				"[import={}] {} CIRCLE id={} center=({}, {}, {}) radius={}",
				id, tag, circle.getId(),
				circle.center().x(), circle.center().y(), circle.center().z(),
				circle.radius());
		});
}

// ============================================================================
// Error-reporting helper
// ============================================================================

void reportImportError(
	const std::shared_ptr<spdlog::logger>& logger,
	const std::shared_ptr<spdlog::logger>& errorLogger,
	const std::string& importId,
	const std::string& pathText,
	const std::string& stage,
	const std::string& detail,
	long long elapsedMs)
{
	if (logger)
	{
		logger->error(
			"[import={}] {}_failed{} elapsed_ms={}",
			importId, stage, detail, elapsedMs);
		logger->flush();
	}
	if (errorLogger)
	{
		errorLogger->error(
			"[import={}] {}_failed file=\"{}\"{} elapsed_ms={}",
			importId, stage, pathText, detail, elapsedMs);
	}
}

// ============================================================================
// Fail-import helper
// ============================================================================

omuPrimitive* failImport(
	const std::shared_ptr<spdlog::logger>& logger,
	const std::shared_ptr<spdlog::logger>& errorLogger,
	const std::string& importId,
	const std::string& pathText,
	const std::string& stage,
	const std::string& detail,
	long long elapsedMs,
	const QString& qWarningMsg)
{
	reportImportError(logger, errorLogger, importId, pathText, stage, detail, elapsedMs);
	qWarning() << qWarningMsg;
	dropDxfImportLogger(importId);
	return nullptr;
}

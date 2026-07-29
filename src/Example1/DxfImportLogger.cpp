#include "DxfImportLogger.h"

#include "DxfData.h"
#include "SamData.h"

#include <QCoreApplication>
#include <QDateTime>
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

static QString dxfLogRootDirectory()
{
	return QCoreApplication::applicationDirPath() + "/logs";
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

		if (retainedCount < 50)
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
// Data-logging helpers
// ============================================================================

void logRawDxfData(
	const std::shared_ptr<spdlog::logger>& logger,
	const std::string& importId,
	const DxfData& data)
{
	if (!logger)
		return;

	for (size_t i = 0; i < data.points().size(); ++i)
	{
		const DxfPoint& point = data.points()[i];
		logger->trace(
			"[import={}] raw POINT id={} position=({}, {}, {})",
			importId, point.getId(), point.x(), point.y(), point.z());
	}

	for (size_t i = 0; i < data.lines().size(); ++i)
	{
		const DxfLine& line = data.lines()[i];
		logger->trace(
			"[import={}] raw LINE id={} start=({}, {}, {}) end=({}, {}, {})",
			importId, line.getId(),
			line.start().x(), line.start().y(), line.start().z(),
			line.end().x(), line.end().y(), line.end().z());
	}

	for (size_t i = 0; i < data.circles().size(); ++i)
	{
		const DxfCircle& circle = data.circles()[i];
		logger->trace(
			"[import={}] raw CIRCLE id={} center=({}, {}, {}) radius={}",
			importId, circle.getId(),
			circle.center().x(), circle.center().y(), circle.center().z(),
			circle.radius());
	}

	for (size_t i = 0; i < data.arcs().size(); ++i)
	{
		const DxfArc& arc = data.arcs()[i];
		logger->trace(
			"[import={}] raw ARC id={} center=({}, {}, {}) radius={}"
			" start_angle={} end_angle={} ccw={}",
			importId, arc.getId(),
			arc.center().x(), arc.center().y(), arc.center().z(),
			arc.radius(), arc.startAngle(), arc.endAngle(), arc.isCCW());
	}

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

	for (size_t i = 0; i < data.ellipses().size(); ++i)
	{
		const DxfEllipse& ellipse = data.ellipses()[i];
		logger->trace(
			"[import={}] raw ELLIPSE id={} center=({}, {}, {})"
			" major_axis=({}, {}, {}) ratio={} start_param={} end_param={} ccw={}",
			importId, ellipse.getId(),
			ellipse.center().x(), ellipse.center().y(), ellipse.center().z(),
			ellipse.majorAxisEnd().x(), ellipse.majorAxisEnd().y(),
			ellipse.majorAxisEnd().z(), ellipse.ratio(),
			ellipse.startParam(), ellipse.endParam(), ellipse.isCCW());
	}
}

void logConvertedSamData(
	const std::shared_ptr<spdlog::logger>& logger,
	const std::string& importId,
	const SamData& data)
{
	if (!logger)
		return;

	for (size_t i = 0; i < data.points().size(); ++i)
	{
		const DxfPoint& point = data.points()[i];
		logger->trace(
			"[import={}] converted POINT id={} position=({}, {}, {})",
			importId, point.getId(), point.x(), point.y(), point.z());
	}

	for (size_t i = 0; i < data.lines().size(); ++i)
	{
		const DxfLine& line = data.lines()[i];
		logger->trace(
			"[import={}] converted LINE id={} start=({}, {}, {}) end=({}, {}, {})",
			importId, line.getId(),
			line.start().x(), line.start().y(), line.start().z(),
			line.end().x(), line.end().y(), line.end().z());
	}

	for (size_t i = 0; i < data.circles().size(); ++i)
	{
		const DxfCircle& circle = data.circles()[i];
		logger->trace(
			"[import={}] converted CIRCLE id={} center=({}, {}, {}) radius={}",
			importId, circle.getId(),
			circle.center().x(), circle.center().y(), circle.center().z(),
			circle.radius());
	}
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

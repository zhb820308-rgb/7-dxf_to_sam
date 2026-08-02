#include "DxfImportSession.h"

#include "DxfImportLogger.h"

#include <QDateTime>
#include <QDebug>

#include <exception>

DxfImportSession::DxfImportSession(
	const QString& filePath,
	double baseX,
	double baseY,
	double baseZ,
	double curveTolerance,
	int maxOutputEntities)
	: m_importId(QDateTime::currentDateTimeUtc()
		.toString("yyyyMMdd_HHmmss_zzz")
		.toStdString()),
	  m_logger(createDxfImportLogger(m_importId)),
	  m_errorLogger(dxfErrorLogger())
{
	m_totalTimer.start();

	// Keep paths in UTF-8 so diagnostics preserve the original Unicode path.
	m_pathText = filePath.toUtf8().toStdString();
	if (!m_logger && m_errorLogger)
	{
		m_errorLogger->error(
			"[import={}] import_log_initialization_failed file=\"{}\"",
			m_importId, m_pathText);
	}
	if (!m_logger && !m_errorLogger)
	{
		qWarning() << "[importDxf] ERROR: log system unavailable for import"
			<< QString::fromStdString(m_importId);
	}

	if (m_logger)
	{
		m_logger->info(
			"[import={}] started file=\"{}\" base=({}, {}, {}) curve_tolerance={} max_output_entities={}",
			m_importId, m_pathText, baseX, baseY, baseZ, curveTolerance,
			maxOutputEntities);
	}
}

DxfImportSession::~DxfImportSession() noexcept
{
	finish();
}

void DxfImportSession::finish() noexcept
{
	if (m_finished)
		return;
	m_finished = true;
	if (m_logger)
	{
		try
		{
			m_logger->flush();
		}
		catch (const std::exception& error)
		{
			qWarning() << "[importDxf] Failed to flush import log:"
				<< error.what();
		}
	}
	try
	{
		dropDxfImportLogger(m_importId);
	}
	catch (const std::exception& error)
	{
		qWarning() << "[importDxf] Failed to release import logger:"
			<< error.what();
	}
	m_logger.reset();
}

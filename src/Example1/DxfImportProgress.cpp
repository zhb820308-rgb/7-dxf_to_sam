#include "DxfImportProgress.h"

#include <QCoreApplication>
#include <QEventLoop>

namespace {

const int kMaxEventProcessingTimeMs = 25;

void processPendingUiEvents()
{
	QCoreApplication::processEvents(
		QEventLoop::AllEvents, kMaxEventProcessingTimeMs);
}

} // namespace

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
			value = offset + static_cast<int>(
				static_cast<double>(span) * current / total);
	}
	else if (stage == QStringLiteral("Creating lines"))
	{
		if (m_totalSketchEntities > 0)
			value = 20 + static_cast<int>(
				79.0 * current / m_totalSketchEntities);
	}
	else if (stage == QStringLiteral("Creating circles"))
	{
		if (m_totalSketchEntities > 0)
			value = 20 + static_cast<int>(
				79.0 * (m_sketchLineCount + current)
				/ m_totalSketchEntities);
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

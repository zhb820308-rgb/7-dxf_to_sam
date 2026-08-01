#pragma once

#include <QProgressDialog>
#include <QString>

#include <cstddef>

enum class DxfImportProgressMode
{
	Sketch,
	FiniteElement
};

class DxfImportProgress
{
public:
	DxfImportProgress(
		DxfImportProgressMode mode,
		std::size_t sketchLineCount = 0,
		std::size_t sketchCircleCount = 0);

	bool update(const QString& stage, int current, int total);
	void complete();
	void close();

	const QString& canceledStage() const { return m_canceledStage; }
	int canceledCurrent() const { return m_canceledCurrent; }
	int canceledTotal() const { return m_canceledTotal; }

private:
	DxfImportProgressMode m_mode;
	std::size_t m_sketchLineCount;
	int m_totalSketchEntities;
	QProgressDialog m_dialog;
	QString m_canceledStage;
	int m_canceledCurrent = 0;
	int m_canceledTotal = 0;
};

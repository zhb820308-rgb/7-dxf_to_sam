#include "DxfImportFeedback.h"

#include "DxfImportValidation.h"

#include <QMessageBox>

namespace {

bool isBudgetError(DxfImportErrorCode code)
{
	return code == DxfImportErrorCode::ExpansionLimit
		|| code == DxfImportErrorCode::ConversionLimit;
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

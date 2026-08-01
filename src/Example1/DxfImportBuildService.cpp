#include "DxfImportBuildService.h"

#include "FeData.h"
#include "PythonFiniteElementBuilder.h"
#include "SamBuilder.h"
#include "SamData.h"

namespace {

template <typename Builder>
ImportBuildResult rollbackAfterFailure(
	Builder& builder,
	ImportBuildResult failure,
	int previouslyCreated = 0)
{
	failure.createdCount += previouslyCreated;
	const ImportBuildResult cleanup = builder.rollback();
	if (cleanup.succeeded())
		return failure;

	const QString combined = failure.message.isEmpty()
		? cleanup.message
		: failure.message + QStringLiteral("; ") + cleanup.message;
	return ImportBuildResult::failure(
		ImportBuildStatus::RollbackFailed,
		combined,
		failure.createdCount);
}

} // namespace

namespace DxfImportBuildService {

ImportBuildResult buildSamSketch(
	const SamData& samData,
	SamBuilder& builder)
{
	ImportBuildResult result = builder.beginImport();
	if (!result.succeeded())
		return result;

	int created = 0;
	result = builder.createLines(samData.lines());
	if (!result.succeeded())
		return rollbackAfterFailure(builder, result);
	created += result.createdCount;

	result = builder.createCircles(samData.circles());
	if (!result.succeeded())
		return rollbackAfterFailure(builder, result, created);
	created += result.createdCount;

	result = builder.commit();
	if (!result.succeeded())
		return rollbackAfterFailure(builder, result);

	return ImportBuildResult::success(created);
}

ImportBuildResult buildFePart(
	FeData& feData,
	const QString& modelName,
	const QString& partName,
	PythonFiniteElementBuilder& builder)
{
	ImportBuildResult result = builder.beginImport(modelName, partName);
	if (!result.succeeded())
		return result;

	result = builder.createNodes(feData.nodes());
	if (!result.succeeded())
		return rollbackAfterFailure(builder, result);
	const int nodeCount = result.createdCount;

	result = builder.createTrusses(feData.trusses());
	if (!result.succeeded())
		return rollbackAfterFailure(builder, result, nodeCount);
	const int trussCount = result.createdCount;

	result = builder.commit();
	if (!result.succeeded())
		return rollbackAfterFailure(builder, result);

	return ImportBuildResult::success(nodeCount + trussCount);
}

} // namespace DxfImportBuildService

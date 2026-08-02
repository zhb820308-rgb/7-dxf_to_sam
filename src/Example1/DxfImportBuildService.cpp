#include "DxfImportBuildService.h"

#include "FeData.h"
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
	failure.recordRollback(cleanup);
	return failure;
}

} // namespace

namespace DxfImportBuildService {

ImportBuildResult buildSamSketch(
	const SamData& samData,
	ISamImportBuilder& builder)
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
	IFeImportBuilder& builder)
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

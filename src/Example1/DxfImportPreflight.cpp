#include "DxfImportPreflight.h"

#include "DxfImportValidation.h"

DxfImportPreflightResult validateDxfImportRequest(
    const DxfImportRequest& request)
{
    const DxfImportValidation::Result parameters =
        DxfImportValidation::validate(
            request.baseX,
            request.baseY,
            request.baseZ,
            request.curveTolerance,
            request.nodeMergeTolerance,
            request.maxOutputEntities);
    if (!parameters.valid)
    {
        return DxfImportPreflightResult{
            false,
            DxfImportMode::Sketch,
            0,
            "validate_params",
            parameters.detail,
            parameters.message};
    }

    const DxfImportModeResult mode = selectDxfImportMode(
        request.importMode, request.modelName, request.partName);
    if (!mode.valid)
    {
        return DxfImportPreflightResult{
            false,
            mode.mode,
            0,
            mode.stage,
            mode.detail,
            mode.message};
    }

    return DxfImportPreflightResult{
        true,
        mode.mode,
        parameters.outputLimit,
        std::string(),
        std::string(),
        QString()};
}

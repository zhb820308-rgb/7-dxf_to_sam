#include "DxfParser.h"

#include "DxfBlockExpansion.h"
#include "DxfInputFile.h"
#include "DxfReaderCallbacks.h"

#include <libdxfrw.h>

#include <cmath>

bool DxfParser::parseFile(
    const QString& filePath,
    DxfData& outData,
    double curveTolerance,
    const std::set<std::string>& ignoredLayers,
    std::size_t maxOutputEntities)
{
    outData = DxfData();
    if (filePath.isEmpty()) {
        outData.setError(
            DxfImportErrorCode::InvalidArgument,
            QStringLiteral("DXF file is empty"));
        return false;
    }
    if (!std::isfinite(curveTolerance) || curveTolerance <= 0.0) {
        outData.setError(
            DxfImportErrorCode::InvalidArgument,
            QStringLiteral(
                "curveTolerance must be finite and greater than zero"));
        return false;
    }
    if (maxOutputEntities == 0) {
        outData.setError(
            DxfImportErrorCode::InvalidArgument,
            QStringLiteral("maxOutputEntities must be greater than zero"));
        return false;
    }

    DxfInputFile inputFile(filePath);
    if (!inputFile.prepare()) {
        outData.setError(
            DxfImportErrorCode::ReadFailed,
            inputFile.errorMessage());
        return false;
    }

    dxfRW dxf(inputFile.encodedPath().constData());
    DxfReaderCallbacks reader;
    reader.setIgnoredLayers(ignoredLayers);
    if (!dxf.read(&reader, true)) {
        outData.setError(
            DxfImportErrorCode::ReadFailed,
            QStringLiteral("Failed to read DXF file. Error code: %1")
                .arg(static_cast<int>(dxf.getError())));
        return false;
    }

    const std::size_t initialEntities = reader.data().entityCount();
    if (initialEntities > maxOutputEntities) {
        outData.setError(
            DxfImportErrorCode::ExpansionLimit,
            QStringLiteral("DXF output limit exceeded: more than %1 entities")
                .arg(static_cast<qulonglong>(maxOutputEntities)));
        return false;
    }

    outData = reader.takeData();
    QString expansionError;
    if (!expandDxfBlocks(
            outData, reader.blocks(), reader.modelSpaceInserts(),
            ignoredLayers, curveTolerance, initialEntities,
            maxOutputEntities, expansionError)) {
        if (expansionError.isEmpty()) {
            expansionError = QStringLiteral("INSERT expansion failed");
        }
        outData.clear();
        outData.setError(
            DxfImportErrorCode::ExpansionLimit,
            expansionError);
        return false;
    }

    const DxfEntityStats& stats = outData.entityStats();
    const std::size_t usable =
        stats.acceptedEntities + stats.generatedEntities;
    if (usable == 0 || outData.entityCount() == 0) {
        outData.setValid(false);
        outData.setError(
            DxfImportErrorCode::NoSupportedEntities,
            QStringLiteral(
                "DXF was read successfully, but no valid supported entities were found (%1 rejected).")
                .arg(stats.rejectedEntities));
        return false;
    }

    outData.setValid(true);
    return true;
}

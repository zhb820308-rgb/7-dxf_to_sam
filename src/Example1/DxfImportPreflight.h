#pragma once

#include "DxfImportMode.h"
#include "DxfImportRequest.h"

#include <QString>

#include <cstddef>
#include <string>

struct DxfImportPreflightResult
{
    bool valid = false;
    DxfImportMode mode = DxfImportMode::Sketch;
    std::size_t outputLimit = 0;
    std::string stage;
    std::string detail;
    QString message;
};

DxfImportPreflightResult validateDxfImportRequest(
    const DxfImportRequest& request);

#pragma once
#ifndef DxfParser_h
#define DxfParser_h

#include "DxfData.h"
#include <QString>

class DxfParser {
public:
    /// Parse a DXF file into a DxfData container.
    /// Block expansion does coordinate transform only; discretization
    /// is deferred to ConversionEngine (which uses the user's tolerance).
    /// @param  filePath       absolute or relative path to .dxf file
    /// @param  outData        [out] parsed entity data; cleared on failure
    /// @param  curveTolerance discretization tolerance for non-uniform scaled block entities
    /// @return true on success; on failure outData.errorMessage() is set
    bool parseFile(const QString& filePath, DxfData& outData,
                   double curveTolerance = 0.01);
};

#endif
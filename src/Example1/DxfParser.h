#pragma once
#ifndef DxfParser_h
#define DxfParser_h

#include "DxfData.h"
#include <QString>

class DxfParser {
public:
    /// Parse a DXF file into a DxfData container.
    /// @param  filePath  absolute or relative path to .dxf file
    /// @param  outData   [out] parsed entity data; cleared on failure
    /// @return true on success; on failure outData.errorMessage() is set
    bool parseFile(const QString& filePath, DxfData& outData);
};

#endif
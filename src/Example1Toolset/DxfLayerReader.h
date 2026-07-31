#ifndef DxfLayerReader_h
#define DxfLayerReader_h

#include <QString>
#include <QStringList>

/// @brief Extract layer names from a DXF file.
/// Opens the file with libdxfrw and collects all layer definitions.
/// @param filePath Path to the .dxf file.
/// @param layers   [out] List of layer names found in the file.
/// @return true if the file was successfully read.
bool collectDxfLayers(const QString& filePath, QStringList& layers);

#endif

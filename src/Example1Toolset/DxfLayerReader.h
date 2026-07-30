#ifndef DxfLayerReader_h
#define DxfLayerReader_h

#include <QString>
#include <QStringList>

bool collectDxfLayers(const QString& filePath, QStringList& layers);

#endif

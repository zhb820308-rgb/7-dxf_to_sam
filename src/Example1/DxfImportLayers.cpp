#include "DxfImportLayers.h"

#include <QStringList>

std::set<std::string> parseIgnoredDxfLayers(const QString& layerText)
{
	std::set<std::string> ignoredLayers;
	if (layerText.isEmpty())
		return ignoredLayers;

	const QStringList parts = layerText.split(',', QString::SkipEmptyParts);
	for (const QString& part : parts)
	{
		const std::string layer = part.trimmed().toStdString();
		if (!layer.empty())
			ignoredLayers.insert(layer);
	}
	return ignoredLayers;
}

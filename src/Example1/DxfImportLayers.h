#pragma once

#include <QString>

#include <set>
#include <string>

std::set<std::string> parseIgnoredDxfLayers(const QString& layerText);

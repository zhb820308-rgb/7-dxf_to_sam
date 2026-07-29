#pragma once

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

class DxfData;
class SamData;

// ============================================================================
// Logger factory functions
// ============================================================================

// Create a per-import logger that writes to logs/imports/dxf_import_<id>.log.
// Returns nullptr on failure (caller must handle gracefully).
std::shared_ptr<spdlog::logger> createDxfImportLogger(const std::string& importId);

// Return the shared error logger (lazy-init, thread-safe).
// Writes to logs/dxf_import_errors.log with rotation (10 MiB × 5 files).
std::shared_ptr<spdlog::logger> dxfErrorLogger();

// Remove a per-import logger from spdlog's global registry.
// Call after the import completes (success or failure) to avoid registry leaks.
void dropDxfImportLogger(const std::string& importId);

// ============================================================================
// Data-logging helpers (trace level, no-op when logger is null)
// ============================================================================

void logRawDxfData(const std::shared_ptr<spdlog::logger>& logger,
	const std::string& importId, const DxfData& data);

void logConvertedSamData(const std::shared_ptr<spdlog::logger>& logger,
	const std::string& importId, const SamData& data);

// ============================================================================
// Error-reporting helper – replaces the 5× repeated block:
//   if (logger)     { logger->error(...);      logger->flush(); }
//   if (errorLogger) { errorLogger->error(...); }
// ============================================================================

void reportImportError(
	const std::shared_ptr<spdlog::logger>& logger,
	const std::shared_ptr<spdlog::logger>& errorLogger,
	const std::string& importId,
	const std::string& pathText,
	const std::string& stage,
	const std::string& detail,
	long long elapsedMs);

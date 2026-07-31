#pragma once

#include <memory>
#include <string>

#include <QString>
#include <spdlog/spdlog.h>

class DxfData;
class SamData;
class omuPrimitive;

// ============================================================================
// Logger factory functions
// ============================================================================

/// @brief Create a per-import logger that writes to logs/imports/dxf_import_<id>.log.
/// Returns nullptr on failure (caller must handle gracefully).
std::shared_ptr<spdlog::logger> createDxfImportLogger(const std::string& importId);

/// @brief Return the shared error logger (lazy-init, thread-safe).
/// Writes to logs/dxf_import_errors.log with rotation (10 MiB x 5 files).
std::shared_ptr<spdlog::logger> dxfErrorLogger();

/// @brief Remove a per-import logger from spdlog's global registry.
/// Call after each import completes (success or failure) to avoid registry leaks.
void dropDxfImportLogger(const std::string& importId);

// ============================================================================
// Data-logging helpers (trace level, no-op when logger is null)
// ============================================================================

/// @brief Log raw DXF parse data at trace level (no-op when logger is null).
/// @param importId Unique import session identifier.
void logRawDxfData(const std::shared_ptr<spdlog::logger>& logger,
	const std::string& importId, const DxfData& data);

/// @brief Log converted SAM data at trace level (no-op when logger is null).
void logConvertedSamData(const std::shared_ptr<spdlog::logger>& logger,
	const std::string& importId, const SamData& data);

// ============================================================================
// Error-reporting helper – replaces the 5× repeated block:
//   if (logger)     { logger->error(...);      logger->flush(); }
//   if (errorLogger) { errorLogger->error(...); }
// ============================================================================

/// @brief Report an import error to both per-import and global error loggers.
///
/// Replaces the 5x repeated block:
///   if (logger)     { logger->error(...);      logger->flush(); }
///   if (errorLogger) { errorLogger->error(...); }
///
/// @param elapsedMs Total elapsed milliseconds since import start.
void reportImportError(
	const std::shared_ptr<spdlog::logger>& logger,
	const std::shared_ptr<spdlog::logger>& errorLogger,
	const std::string& importId,
	const std::string& pathText,
	const std::string& stage,
	const std::string& detail,
	long long elapsedMs);

// ============================================================================
// Fail-import helper — convenience wrapper that chains report + qWarning +
// drop + return nullptr into a single call site. Use inside importDxf only.
// ============================================================================

/// @brief Convenience wrapper — chains reportImportError + qWarning + drop + return nullptr.
/// Use inside importDxf only.
omuPrimitive* failImport(
	const std::shared_ptr<spdlog::logger>& logger,
	const std::shared_ptr<spdlog::logger>& errorLogger,
	const std::string& importId,
	const std::string& pathText,
	const std::string& stage,
	const std::string& detail,
	long long elapsedMs,
	const QString& qWarningMsg);

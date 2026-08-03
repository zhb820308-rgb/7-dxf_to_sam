#pragma once

// ============================================================================
// 通用日志接口
// ============================================================================

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

// ============================================================================
// RAII 导入会话
// ============================================================================

#include <QElapsedTimer>


namespace spdlog {
class logger;
}

class DxfImportSession
{
public:
	DxfImportSession(
		const QString& filePath,
		double baseX,
		double baseY,
		double baseZ,
		double curveTolerance,
		int maxOutputEntities);
	~DxfImportSession() noexcept;

	DxfImportSession(const DxfImportSession&) = delete;
	DxfImportSession& operator=(const DxfImportSession&) = delete;

	const std::string& importId() const { return m_importId; }
	const std::string& pathText() const { return m_pathText; }
	const std::shared_ptr<spdlog::logger>& logger() const { return m_logger; }
	const std::shared_ptr<spdlog::logger>& errorLogger() const
	{
		return m_errorLogger;
	}
	long long elapsed() const { return m_totalTimer.elapsed(); }

	void finish() noexcept;

private:
	std::string m_importId;
	std::string m_pathText;
	std::shared_ptr<spdlog::logger> m_logger;
	std::shared_ptr<spdlog::logger> m_errorLogger;
	QElapsedTimer m_totalTimer;
	bool m_finished = false;
};

// ============================================================================
// 公共事务状态机
// ============================================================================
#ifndef ImportTransaction_h
#define ImportTransaction_h

// 事务状态机：状态比几个互相独立的 bool 更不容易表达出矛盾组合。
enum class ImportTransactionState {
    Idle,
    Preparing,
    Writing,
    Committing,
    Committed,
    RollingBack,
    RolledBack
};

/**
 * @brief 两种 Builder 共用的轻量事务状态机。
 *
 * 它不懂 Sketch 或 Part，只约束合法顺序。`ownsResource` 表示已经创建了尚未提交的
 * SAM 对象；此时任何失败/析构都必须清理。rollback 对 Idle/RolledBack 返回成功，
 * 因而可以安全重试（幂等）；Committed 明确禁止回滚。
 */
class ImportTransaction {
public:
    ImportTransactionState state() const { return m_state; }
    bool ownsResource() const { return m_ownsResource; }

    bool begin()
    {
        if (m_state != ImportTransactionState::Idle &&
            m_state != ImportTransactionState::RolledBack)
            return false;
        m_state = ImportTransactionState::Preparing;
        m_ownsResource = false;
        return true;
    }

    bool markResourceOwned()
    {
        if (m_state != ImportTransactionState::Preparing)
            return false;
        m_ownsResource = true;
        return true;
    }

    bool startWriting()
    {
        if (m_state != ImportTransactionState::Preparing || !m_ownsResource)
            return false;
        m_state = ImportTransactionState::Writing;
        return true;
    }

    bool startCommitting()
    {
        if (m_state != ImportTransactionState::Writing)
            return false;
        m_state = ImportTransactionState::Committing;
        return true;
    }

    bool markCommitted()
    {
        if (m_state != ImportTransactionState::Committing)
            return false;
        m_ownsResource = false;
        m_state = ImportTransactionState::Committed;
        return true;
    }

    // 模板让调用者传入任意可调用清理函数（通常是 lambda），无需让事务类依赖
    // SamBuilder/PythonFiniteElementBuilder。编译器会为实际 Cleanup 类型生成版本。
    template <typename Cleanup>
    bool rollback(Cleanup cleanup)
    {
        if (m_state == ImportTransactionState::Idle ||
            m_state == ImportTransactionState::RolledBack)
            return true;
        if (m_state == ImportTransactionState::Committed)
            return false;

        m_state = ImportTransactionState::RollingBack;
        // `&&` 短路：没有资源时绝不会调用 cleanup；清理失败则保留 ownsResource，
        // 允许上层再次尝试，而不是谎称已经回滚。
        if (m_ownsResource && !cleanup())
            return false;

        m_ownsResource = false;
        m_state = ImportTransactionState::RolledBack;
        return true;
    }

private:
    ImportTransactionState m_state = ImportTransactionState::Idle;
    bool m_ownsResource = false;
};

#endif // ImportTransaction_h

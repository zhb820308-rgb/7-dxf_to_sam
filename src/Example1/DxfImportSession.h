#pragma once

#include <QElapsedTimer>
#include <QString>

#include <memory>
#include <string>

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

/**
 * test_dxf_import_logger.cpp — 测试 DxfImportLogger 日志模块
 *
 * 覆盖:
 *   LoggerRegistry  — create / drop / singleton
 *   NullSafety      — 所有函数对 nullptr logger 安全
 *   DataLogging     — logRawDxfData / logConvertedSamData 输出验证
 *   ErrorReporting  — reportImportError 双通道写入
 *   LogFormat       — 时间戳 / 级别 / import 标签 / 键值对格式
 */
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>

#include "DxfImportLogger.h"
#include "DxfData.h"
#include "SamData.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// 辅助函数
// ============================================================================

static std::string readFileContent(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream in(&f);
    return in.readAll().toStdString();
}

static QString perImportLogPath(const std::string& importId)
{
    return QCoreApplication::applicationDirPath()
           + "/logs/imports/dxf_import_" + QString::fromStdString(importId) + ".log";
}

static QString errorLogPath()
{
    return QCoreApplication::applicationDirPath()
           + "/logs/dxf_import_errors.log";
}

// 用一个唯一的后缀让每次测试运行的日志文件不冲突
static std::string uniqueSuffix()
{
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::to_string(now.count());
}

// ============================================================================
// Test fixture — 管理 importId 和临时日志清理
// ============================================================================

class DxfImportLoggerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        suffix = uniqueSuffix();
        importId = "test_" + suffix;
    }

    void TearDown() override
    {
        dropDxfImportLogger(importId);
        // 删除本次测试产生的日志文件
        QFile::remove(perImportLogPath(importId));
    }

    std::string suffix;
    std::string importId;
};

// ============================================================================
// Group 1: LoggerRegistryTest — 日志注册表管理
// ============================================================================

TEST_F(DxfImportLoggerTest, create_import_logger_returns_valid)
{
    auto logger = createDxfImportLogger(importId);
    ASSERT_NE(logger, nullptr);
    EXPECT_EQ(logger->name(), "dxf_import_" + importId);
}

TEST_F(DxfImportLoggerTest, error_logger_is_singleton)
{
    auto e1 = dxfErrorLogger();
    auto e2 = dxfErrorLogger();
    ASSERT_NE(e1, nullptr);
    EXPECT_EQ(e1.get(), e2.get());
}

TEST_F(DxfImportLoggerTest, drop_removes_from_registry)
{
    {
        auto logger = createDxfImportLogger(importId);
        ASSERT_NE(logger, nullptr);
    }
    dropDxfImportLogger(importId);
    EXPECT_EQ(spdlog::get("dxf_import_" + importId), nullptr);
}

// ============================================================================
// Group 2: NullSafetyTest — 空指针安全
// ============================================================================

TEST_F(DxfImportLoggerTest, log_raw_dxf_data_null_logger)
{
    DxfData data;
    data.addPoint(DxfPoint(1.0, 2.0, 3.0));
    EXPECT_NO_THROW(logRawDxfData(nullptr, importId, data));
}

TEST_F(DxfImportLoggerTest, log_converted_sam_data_null_logger)
{
    SamData data;
    data.addLine(DxfLine(DxfPoint(0, 0, 0), DxfPoint(1, 1, 1)));
    EXPECT_NO_THROW(logConvertedSamData(nullptr, importId, data));
}

TEST_F(DxfImportLoggerTest, report_import_error_null_loggers)
{
    EXPECT_NO_THROW(
        reportImportError(nullptr, nullptr, importId, "test.dxf",
                          "parse", " error=\"fail\"", 42));
}

TEST_F(DxfImportLoggerTest, report_import_error_null_per_import_logger)
{
    auto errorLogger = dxfErrorLogger();
    ASSERT_NE(errorLogger, nullptr);
    EXPECT_NO_THROW(
        reportImportError(nullptr, errorLogger, importId, "test.dxf",
                          "parse", " error=\"fail\"", 42));
}

// ============================================================================
// Group 3: DataLoggingTest — 图元数据日志
// ============================================================================

TEST_F(DxfImportLoggerTest, log_raw_dxf_data_all_entities)
{
    auto logger = createDxfImportLogger(importId);
    ASSERT_NE(logger, nullptr);

    DxfData data;
    data.addPoint(DxfPoint(1.0, 2.0, 3.0));
    data.addLine(DxfLine(DxfPoint(0, 0, 0), DxfPoint(10, 20, 0)));
    data.addCircle(DxfCircle(DxfPoint(5, 5, 0), 3.0));
    data.addArc(DxfArc(DxfPoint(0, 0, 0), 1.0, 0.0, M_PI, true));
    {
        std::vector<DxfPoint> verts = { DxfPoint(0, 0, 0), DxfPoint(1, 1, 0) };
        std::vector<double> bulges = { 0.5 };
        data.addLWPolyline(DxfLWPolyline(verts, bulges, false, 0.0));
    }
    {
        DxfEllipse e(DxfPoint(0, 0, 0), DxfPoint(2, 0, 0), 0.5, 0.0, M_PI, true);
        data.addEllipse(e);
    }

    logRawDxfData(logger, importId, data);
    logger->flush();

    std::string content = readFileContent(perImportLogPath(importId));

    EXPECT_NE(content.find("raw POINT"), std::string::npos)   << content;
    EXPECT_NE(content.find("raw LINE"), std::string::npos)    << content;
    EXPECT_NE(content.find("raw CIRCLE"), std::string::npos)  << content;
    EXPECT_NE(content.find("raw ARC"), std::string::npos)     << content;
    EXPECT_NE(content.find("raw LWPOLYLINE"), std::string::npos) << content;
    EXPECT_NE(content.find("raw ELLIPSE"), std::string::npos) << content;
    EXPECT_NE(content.find("position=(1, 2, 3)"), std::string::npos);
}

TEST_F(DxfImportLoggerTest, log_converted_sam_data_lines)
{
    auto logger = createDxfImportLogger(importId);
    ASSERT_NE(logger, nullptr);

    SamData data;
    data.addPoint(DxfPoint(100.0, 200.0, 0.0));
    data.addLine(DxfLine(DxfPoint(10, 20, 0), DxfPoint(30, 40, 0)));
    data.addCircle(DxfCircle(DxfPoint(50, 60, 0), 5.0));

    logConvertedSamData(logger, importId, data);
    logger->flush();

    std::string content = readFileContent(perImportLogPath(importId));

    EXPECT_NE(content.find("converted POINT"), std::string::npos)  << content;
    EXPECT_NE(content.find("converted LINE"), std::string::npos)   << content;
    EXPECT_NE(content.find("converted CIRCLE"), std::string::npos) << content;
    EXPECT_NE(content.find("start=(10, 20, 0)"), std::string::npos);
    EXPECT_NE(content.find("end=(30, 40, 0)"), std::string::npos);
}

// ============================================================================
// Group 4: ErrorReportingTest — 错误报告
// ============================================================================

TEST_F(DxfImportLoggerTest, report_import_error_writes_to_both)
{
    auto logger = createDxfImportLogger(importId);
    auto errLogger = dxfErrorLogger();
    ASSERT_NE(logger, nullptr);
    ASSERT_NE(errLogger, nullptr);

    reportImportError(logger, errLogger, importId,
                      "C:\\fake.dxf", "parse",
                      " error=\"bad entity\"", 123);

    logger->flush();
    errLogger->flush();

    std::string perLog = readFileContent(perImportLogPath(importId));
    std::string errLog = readFileContent(errorLogPath());

    EXPECT_NE(perLog.find("parse_failed"), std::string::npos) << perLog;
    EXPECT_NE(perLog.find("bad entity"), std::string::npos)   << perLog;
    EXPECT_NE(perLog.find("elapsed_ms=123"), std::string::npos) << perLog;

    EXPECT_NE(errLog.find("parse_failed"), std::string::npos)  << errLog;
    EXPECT_NE(errLog.find("bad entity"), std::string::npos)    << errLog;
}

TEST_F(DxfImportLoggerTest, error_logger_has_file_field)
{
    auto logger = createDxfImportLogger(importId);
    auto errLogger = dxfErrorLogger();
    ASSERT_NE(errLogger, nullptr);

    reportImportError(logger, errLogger, importId,
                      "C:\\fake.dxf", "parse",
                      " error=\"bad\"", 0);
    errLogger->flush();

    std::string errLog = readFileContent(errorLogPath());
    EXPECT_NE(errLog.find("file=\""), std::string::npos) << errLog;
}

TEST_F(DxfImportLoggerTest, per_import_logger_no_file_field)
{
    auto logger = createDxfImportLogger(importId);
    auto errLogger = dxfErrorLogger();
    ASSERT_NE(logger, nullptr);

    reportImportError(logger, errLogger, importId,
                      "C:\\fake.dxf", "conversion",
                      " error=\"oops\"", 0);
    logger->flush();

    std::string perLog = readFileContent(perImportLogPath(importId));
    // 每次导入日志不应该包含 file= 字段
    EXPECT_EQ(perLog.find("file=\""), std::string::npos) << perLog;
}

// ============================================================================
// Group 5: LogFormatTest — 日志格式
// ============================================================================

TEST_F(DxfImportLoggerTest, info_log_contains_import_tag)
{
    auto logger = createDxfImportLogger(importId);
    ASSERT_NE(logger, nullptr);

    logger->info("[import={}] started file=\"test.dxf\" base=(0, 0, 0)",
                 importId);
    logger->flush();

    std::string content = readFileContent(perImportLogPath(importId));
    EXPECT_NE(content.find("[import=" + importId + "]"), std::string::npos);
    EXPECT_NE(content.find("[info]"), std::string::npos);
}

TEST_F(DxfImportLoggerTest, error_log_contains_stage)
{
    auto logger = createDxfImportLogger(importId);
    auto errLogger = dxfErrorLogger();
    ASSERT_NE(logger, nullptr);

    reportImportError(logger, errLogger, importId,
                      "test.dxf", "commit",
                      " sketch=\"S1\" submitted=5 error=\"OOM\"", 250);
    logger->flush();

    std::string content = readFileContent(perImportLogPath(importId));
    EXPECT_NE(content.find("commit_failed"), std::string::npos) << content;
    EXPECT_NE(content.find("sketch=\"S1\""), std::string::npos) << content;
    EXPECT_NE(content.find("elapsed_ms=250"), std::string::npos) << content;
}

// ============================================================================
// main — 提供 QCoreApplication 供日志模块使用
// ============================================================================

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

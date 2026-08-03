#pragma once

// ============================================================================
// Qt 插件入口与菜单控制器
// ============================================================================

#include <QtPlugin>
#include <SAMToolsetGuiInterface.h>
#include <SAMToolsetGui.h>
#include <omiSingleton.h>
#include <omiSingleton.T>

class Example1Form;

/**
 * @file Example1Toolset.h
 * @brief Toolset DLL 的插件入口和菜单控制器。
 *
 * 本文件只保留 Toolset 的“外壳”：SAM 如何发现插件、菜单如何创建、点击菜单后
 * 如何启动 Form。注意：只有这个头文件包含 omiSingleton.T；调用 SAM 命令系统
 * 的 Form/Dialog 文件不得包含本头文件，否则会在插件内实例化假的 SAM 单例。
 */

class Example1ToolsetPlugin : public QObject, public SAMToolsetGuiInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SAMToolsetGuiPlugin_iid)
    Q_INTERFACES(SAMToolsetGuiInterface)

public:
    Example1ToolsetPlugin();
    ~Example1ToolsetPlugin() override;
    void registerToolset() override;
};

class Example1ToolsetGui
    : public SAMToolsetGui,
      public omiSingleton<Example1ToolsetGui>
{
    Q_OBJECT
    friend class omiSingleton<Example1ToolsetGui>;

public:
    Example1ToolsetGui();
    ~Example1ToolsetGui() override;

protected:
    void activate() override;
    void deactivate() override;
    virtual void createMenuItems();
    virtual void createToolboxItems();

private slots:
    void onImportDxf();
    void onViewDxfLogs();

private:
    Example1ToolsetGui(const Example1ToolsetGui&) = delete;
    Example1ToolsetGui& operator=(const Example1ToolsetGui&) = delete;
};

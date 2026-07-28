#ifndef SamBuilder_h
#define SamBuilder_h

#include "SamData.h"
#include <basBasis.h>
#include <QString>
#include <vector>

class skcSketch;
class skcGeomFactory;
class gmlSketchRepository;

/// SAM 对接模块 —— 负责建草图画几何、提交数据库、刷新场景
///
/// 用法:
///   SamBuilder builder;
///   if (!builder.beginImport())  return;
///   builder.createLines(samData.lines());
///   if (!builder.commit()) { builder.rollback(); return; }
class SamBuilder {
public:
    SamBuilder();
    ~SamBuilder();

    // ========== 生命周期 ==========

    /// 创建新草图，准备绘制
    /// @param modelName 模型名，默认 "Model-1"
    /// @return true 表示成功
    bool beginImport(const QString& modelName = "Model-1");

    /// 提交到数据库 + 刷新场景
    /// @return true 表示成功
    bool commit();

    /// 发生不可恢复错误时撤销本次导入
    void rollback();

    // ========== 几何创建 ==========

    /// 创建点（接口待确认）
    int createPoints(const std::vector<DxfPoint>& points);

    /// 创建直线
    int createLines(const std::vector<DxfLine>& lines);

    /// 创建圆（接口待确认）
    int createCircles(const std::vector<DxfCircle>& circles);

    // ========== 查询 ==========

    int   createdCount() const { return m_createdCount; }
    const QString& sketchName() const { return m_sketchName; }
    const QString& lastError() const { return m_lastError; }

private:
    void buildSketchPath();

    QString m_modelName;
    QString m_sketchName;
    QString m_sketchPath;
    QString m_lastError;

    gmlSketchRepository* m_repos  = nullptr;
    skcSketch*          m_sketch  = nullptr;
    skcGeomFactory*     m_factory = nullptr;

    int m_createdCount = 0;
    bool m_active = false;
};

#endif

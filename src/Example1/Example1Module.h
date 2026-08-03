#pragma once

// ============================================================================
// Example1PytModule
// ============================================================================
/* -*- mode: c++ -*- */
#ifndef Example1PytModule_h
#define Example1PytModule_h

// Includes

// Begin local includes

#include <ptsKPartFragment.h>
#include <pyoModule.h>

// Class definition

/**
 * @brief SAM/Python 可调用的 DXF 导入门面（Facade）。
 *
 * 它继承 SAM SDK 的 `pyoModule`，只负责协议适配：把 Python/SAM 参数读成
 * `DxfImportRequest`，交给 `runDxfImport()`，再把 C++ 结果包装回 SAM 的
 * `omuPrimitive*`。解析 DXF、离散曲线、写数据库都不应塞到这个入口类中。
 */
class Example1PytModule : public pyoModule
{
public:

	Example1PytModule();
	virtual ~Example1PytModule();

	virtual void DefineConstants();

public:
	/// @brief Import a DXF file as a SAM sketch or finite-element part.
	/// @param args Arguments: filePath (str), baseX/Y/Z (float),
	///             curveTolerance, ignoreLayers, importMode, modelName,
	///             partName, nodeMergeTolerance and maxOutputEntities
	///             (all optional after baseX/Y/Z).
	/// @return Number of created entities, or nullptr on failure.
	omuPrimitive* importDxf(omuArguments& args);

private:
	// 只声明、不实现拷贝构造和赋值运算符，是旧式 C++ 的“禁止复制”写法。
	// 原因是模块注册对象具有唯一身份，复制会导致重复注册/重复释放。
	Example1PytModule(const Example1PytModule&);
	Example1PytModule& operator=(const Example1PytModule&);
};

#endif  // #ifndef Example1PytModule_h

// ============================================================================
// Example1Utils
// ============================================================================
#ifndef Example1Utils_h
#define Example1Utils_h

/// 向 SAM 运行时注册 Example1 Python 模块和 Fragment。
/// 使用引用计数，因此重复初始化不会重复 new 同一批全局对象。
/// @param count [in/out] SAM 维护的总模块计数；引用表示函数可修改调用者的变量。
void Example1Initialize(int& count);

/// 注销模块；最后一个使用者退出时释放注册对象。
/// @param count [in/out] SAM 维护的总模块计数。
void Example1Finalize(int& count);

#endif

// ============================================================================
// kefKLine
// ============================================================================
/* -*- mode: c++ -*- */
#ifndef kefKLine_h
#define kefKLine_h

//
// Begin local includes
//
#include <gdyNonPageGeomEditor.h>
#include <g3dVector.h>
#include <cowListInt.h>

#include <array>
#include <cowList.T>
#include <omiBtree.T>

class kefKLine : public gdyNonPageGeomEditor
{
public:
    /// @name Constructors
    /// @{
    kefKLine();
    virtual ~kefKLine();
    /// @}

    virtual gdyGeomEditor* Copy() const;

    /// @brief Returns the type atom for this editor.
    const omuAtom& Type() const { return type_atom; }

    /// @name Render callbacks (called from gdyEditor)
    /// @{
    virtual void Load(const gdyDisplayRep* r, g3dSphere*);
    virtual void Unload();
    virtual void Draft(gdrRenderer&) const;
    virtual void Paint(gdrRenderer&) const;
    virtual void ASelect(gdrASelector&) const;
    virtual void Highlight(gdrRenderer&) const;
    /// @}

    /// @brief Find the index of an object by its segment ID.
    /// @return Object index, or -1 if not found.
    int findObject(int segID) const;

    /// @brief Create a new object or modify an existing one.
    /// @param segID [in/out] Segment ID — if found in existing objects the
    ///        entry is updated; otherwise a new object is created and segID
    ///        receives the new ID.
    /// @param vertexXYZ Optional vertex list for new objects.
    void createOneObject(int& segID, const cowList<g3dVector>& vertexXYZ = cowList<g3dVector>());

private:
    void Draw(gdrRenderer& drafter, bool paintMode) const;

    double timeStamp;
    omuAtom type_atom;

    cowList<cowList<g3dVector>> _geomVertexXYZs;
    cowListInt _segID;
    int _maxSegID;

};

#endif /* kefKLine_h */

// ============================================================================
// SAMExample1Fragment
// ============================================================================
/* -*- mode: c++ -*- */
#ifndef SAMExample1Fragment_h
#define SAMExample1Fragment_h


class omuArguments;

/// @brief Example SAM fragment demonstrating basic line drawing.
///
/// Demonstrates: custom geometry editor registration, line creation,
/// and node location queries. KLine is a line editor registered with the scene.
class SAMExample1Fragment : public ptsKPartFragment
{
public:
	SAMExample1Fragment();
	virtual ~SAMExample1Fragment();

	omuPrimitive* Copy() const;

	/// @brief Draw example geometry — demonstrates creating a rectangle.
	omuPrimitive* drawExample(omuArguments& args);

	/// @brief Query the location of a specified node.
	omuPrimitive* getNodeLocation(omuArguments& args);

	/// @brief Draw a line segment and register it with the scene editor.
	void DrawLine(int& segID, g3dVector startPoint, g3dVector endPoint);
};

#endif  // #ifndef SAMExample1Fragment_h

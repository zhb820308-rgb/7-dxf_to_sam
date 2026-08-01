# libdxfrw 完整 API 文档 (v0.6.3)

libdxfrw 是一个 C++ 库，用于读取和写入 DXF/DWG 文件（ASCII 和二进制格式）。主要公开头文件有 5 个：

| 头文件 | 作用 |
|--------|------|
| `drw_base.h` | 基础类型定义、枚举、坐标类和变体类 |
| `drw_entities.h` | 所有实体类（点、线、圆等图形元素） |
| `drw_header.h` | DXF 头部变量类 |
| `drw_objects.h` | DXF 对象类（图层、线型、标注样式等） |
| `drw_interface.h` | 读写回调接口（纯虚类，需被继承实现） |
| `libdxfrw.h` | 主入口类 `dxfRW`，负责读写文件 |

---

## 一、主入口类 `dxfRW`

**文件**: `src/libdxfrw.h`

```cpp
class dxfRW
```

### 构造与析构

| 签名 | 说明 |
|------|------|
| `dxfRW(const char* name)` | 构造函数，传入 DXF 文件路径 |
| `~dxfRW()` | 析构函数 |

### 读写操作

| 签名 | 说明 |
|------|------|
| `bool read(DRW_Interface *interface_, bool ext)` | 读取文件。`ext`=true 时应用挤出转换为 2D |
| `bool write(DRW_Interface *interface_, DRW::Version ver, bool bin)` | 写入文件（含头部和所有内容） |

### 写入单个实体

| 签名 | 说明 |
|------|------|
| `bool writeLineType(DRW_LType *ent)` | 写入线型 |
| `bool writeLayer(DRW_Layer *ent)` | 写入图层 |
| `bool writeDimstyle(DRW_Dimstyle *ent)` | 写入标注样式 |
| `bool writeTextstyle(DRW_Textstyle *ent)` | 写入文字样式 |
| `bool writeVport(DRW_Vport *ent)` | 写入视口表 |
| `bool writeAppId(DRW_AppId *ent)` | 写入应用程序 ID |
| `bool writePoint(DRW_Point *ent)` | 写入点 |
| `bool writeLine(DRW_Line *ent)` | 写入直线 |
| `bool writeRay(DRW_Ray *ent)` | 写入射线 |
| `bool writeXline(DRW_Xline *ent)` | 写入构造线 |
| `bool writeCircle(DRW_Circle *ent)` | 写入圆 |
| `bool writeArc(DRW_Arc *ent)` | 写入圆弧 |
| `bool writeEllipse(DRW_Ellipse *ent)` | 写入椭圆 |
| `bool writeTrace(DRW_Trace *ent)` | 写入迹线 |
| `bool writeSolid(DRW_Solid *ent)` | 写入实体填充 |
| `bool write3dface(DRW_3Dface *ent)` | 写入三维面 |
| `bool writeLWPolyline(DRW_LWPolyline *ent)` | 写入轻量多段线 |
| `bool writePolyline(DRW_Polyline *ent)` | 写入多段线 |
| `bool writeSpline(DRW_Spline *ent)` | 写入样条曲线 |
| `bool writeBlock(DRW_Block *ent)` | 写入块 |
| `bool writeBlockRecord(std::string name)` | 写入块记录 |
| `bool writeInsert(DRW_Insert *ent)` | 写入插入引用 |
| `bool writeMText(DRW_MText *ent)` | 写入多行文字 |
| `bool writeText(DRW_Text *ent)` | 写入单行文字 |
| `bool writeHatch(DRW_Hatch *ent)` | 写入填充图案 |
| `bool writeViewport(DRW_Viewport *ent)` | 写入视口 |
| `DRW_ImageDef* writeImage(DRW_Image *ent, std::string name)` | 写入图像，返回图像定义 |
| `bool writeLeader(DRW_Leader *ent)` | 写入引线 |
| `bool writeDimension(DRW_Dimension *ent)` | 写入尺寸标注 |
| `bool writePlotSettings(DRW_PlotSettings *ent)` | 写入打印设置 |

### 状态与配置

| 签名 | 说明 |
|------|------|
| `void setDebug(DRW::DebugLevel lvl)` | 设置调试级别 (`None` 或 `Debug`) |
| `void setBinary(bool b)` | 设置是否为二进制格式文件 |
| `void setEllipseParts(int parts)` | 设置椭圆转多段线时的分段数 |
| `DRW::Version getVersion() const` | 获取当前 DXF 版本 |
| `DRW::error getError() const` | 获取最近一次错误码 |

---

## 二、回调接口 `DRW_Interface`

**文件**: `src/drw_interface.h` — 纯虚类，使用 libdxfrw 必须实现此接口。

### 表格回调（读完 HEADER 后调用）

| 纯虚方法 | 说明 |
|----------|------|
| `void addHeader(const DRW_Header* data)` | 头部解析完毕时调用 |
| `void addLType(const DRW_LType& data)` | 每个线型 |
| `void addLayer(const DRW_Layer& data)` | 每个图层 |
| `void addDimStyle(const DRW_Dimstyle& data)` | 每个标注样式 |
| `void addVport(const DRW_Vport& data)` | 每个视口表 |
| `void addTextStyle(const DRW_Textstyle& data)` | 每个文字样式 |
| `void addAppId(const DRW_AppId& data)` | 每个应用程序 ID |

### 块回调

| 纯虚方法 | 说明 |
|----------|------|
| `void addBlock(const DRW_Block& data)` | 块开始（之后实体属于该块直到 `endBlock()`） |
| `void setBlock(const int handle)` | 切换到指定 handle 的块（DWG 专用） |
| `void endBlock()` | 块结束 |

### 实体回调

| 纯虚方法 | 说明 |
|----------|------|
| `void addPoint(const DRW_Point& data)` | 点 |
| `void addLine(const DRW_Line& data)` | 直线 |
| `void addRay(const DRW_Ray& data)` | 射线 |
| `void addXline(const DRW_Xline& data)` | 构造线 |
| `void addArc(const DRW_Arc& data)` | 圆弧 |
| `void addCircle(const DRW_Circle& data)` | 圆 |
| `void addEllipse(const DRW_Ellipse& data)` | 椭圆 |
| `void addLWPolyline(const DRW_LWPolyline& data)` | 轻量多段线 |
| `void addPolyline(const DRW_Polyline& data)` | 多段线 |
| `void addSpline(const DRW_Spline* data)` | 样条曲线 |
| `void addKnot(const DRW_Entity& data)` | 样条曲线节点值 |
| `void addInsert(const DRW_Insert& data)` | 插入引用 |
| `void addTrace(const DRW_Trace& data)` | 迹线 |
| `void add3dFace(const DRW_3Dface& data)` | 三维面 |
| `void addSolid(const DRW_Solid& data)` | 实体填充 |
| `void addMText(const DRW_MText& data)` | 多行文字 |
| `void addText(const DRW_Text& data)` | 单行文字 |
| `void addDimAlign(const DRW_DimAligned *data)` | 对齐标注 |
| `void addDimLinear(const DRW_DimLinear *data)` | 线性/旋转标注 |
| `void addDimRadial(const DRW_DimRadial *data)` | 半径标注 |
| `void addDimDiametric(const DRW_DimDiametric *data)` | 直径标注 |
| `void addDimAngular(const DRW_DimAngular *data)` | 角度标注（2 线版本） |
| `void addDimAngular3P(const DRW_DimAngular3p *data)` | 角度标注（3 点版本） |
| `void addDimOrdinate(const DRW_DimOrdinate *data)` | 坐标标注 |
| `void addLeader(const DRW_Leader *data)` | 引线 |
| `void addHatch(const DRW_Hatch *data)` | 填充图案 |
| `void addViewport(const DRW_Viewport& data)` | 视口 |
| `void addImage(const DRW_Image *data)` | 图像实体 |
| `void linkImage(const DRW_ImageDef *data)` | 图像定义 |
| `void addComment(const char* comment)` | 注释（DXF code 999） |
| `void addPlotSettings(const DRW_PlotSettings *data)` | 打印设置对象 |

### 写入回调（写文件时使用）

| 纯虚方法 | 说明 |
|----------|------|
| `void writeHeader(DRW_Header& data)` | 填充头部数据 |
| `void writeBlocks()` | 调用写块方法的地方 |
| `void writeBlockRecords()` | 调用写块记录的地方 |
| `void writeEntities()` | 调用写实体的地方 |
| `void writeLTypes()` | 调用写线型的地方 |
| `void writeLayers()` | 调用写图层的地方 |
| `void writeTextstyles()` | 调用写文字样式的地方 |
| `void writeVports()` | 调用写视口表的地方 |
| `void writeDimstyles()` | 调用写标注样式的地方 |
| `void writeObjects()` | 调用写对象的地方 |
| `void writeAppId()` | 调用写应用程序 ID 的地方 |

---

## 三、实体类继承体系

**文件**: `src/drw_entities.h`

所有实体都继承自 `DRW_Entity`。

### 3.1 基类 `DRW_Entity`（所有实体的公共字段）

| 字段 | 类型 | 说明 (DXF code) |
|------|------|----------------|
| `eType` | `enum DRW::ETYPE` | 实体类型，code 0 |
| `handle` | `duint32` | 实体标识符，code 5 |
| `parentHandle` | `duint32` | 所有者 BLOCK_RECORD 的软指针，code 330 |
| `space` | `DRW::Space` | 空间标志，code 67 (ModelSpace=0, PaperSpace=1) |
| `layer` | `UTF8STRING` | 图层名，code 8，默认 "0" |
| `lineType` | `UTF8STRING` | 线型名，code 6，默认 "BYLAYER" |
| `material` | `duint32` | 材质 ID，code 347 |
| `color` | `int` | 颜色号，code 62 |
| `lWeight` | `enum DRW_LW_Conv::lineWidth` | 线宽，code 370 |
| `ltypeScale` | `double` | 线型比例，code 48，默认 1.0 |
| `visible` | `bool` | 可见性，code 60，默认 true |
| `numProxyGraph` | `int` | 代理图形字节数，code 92 |
| `proxyGraphics` | `std::string` | 代理图形数据，code 310 |
| `color24` | `int` | 24 位颜色，code 420，默认 -1 |
| `colorName` | `std::string` | 颜色名称，code 430 |
| `transparency` | `int` | 透明度，code 440 |
| `plotStyle` | `int` | 打印样式 ID，code 390 |
| `shadow` | `DRW::ShadowMode` | 阴影模式，code 284 |
| `haveExtrusion` | `bool` | 是否有挤出矢量 |
| `extData` | `vector<shared_ptr<DRW_Variant>>` | 扩展数据列表，codes 1000-1071 |
| `appData` | `list<list<DRW_Variant>>` | 应用数据，code 102 |

### 3.2 实体类层次结构

| 类 | 继承自 | DXF 类型 | 特有成员 |
|----|--------|----------|----------|
| **`DRW_Point`** | `DRW_Entity` | POINT | `basePoint`(DRW_Coord, 10/20/30), `thickness`(double, 39), `extPoint`(DRW_Coord, 210/220/230) |
| **`DRW_Line`** | `DRW_Point` | LINE | `secPoint`(DRW_Coord, 11/21/31) |
| **`DRW_Ray`** | `DRW_Line` | RAY | （无额外成员） |
| **`DRW_Xline`** | `DRW_Ray` | XLINE | （无额外成员） |
| **`DRW_Circle`** | `DRW_Point` | CIRCLE | `radious`(double, 40)、方法 `applyExtrusion()` |
| **`DRW_Arc`** | `DRW_Circle` | ARC | `staangle`(double, 50), `endangle`(double, 51), `isccw`(int, 73)、便捷方法: `center()`, `radius()`, `startAngle()`, `endAngle()`, `thick()`, `extrusion()` |
| **`DRW_Ellipse`** | `DRW_Line` | ELLIPSE | `ratio`(double, 40), `staparam`(double, 41), `endparam`(double, 42), `isccw`(int, 73)、方法 `toPolyline(DRW_Polyline*, int parts)` |
| **`DRW_Trace`** | `DRW_Line` | TRACE | `thirdPoint`(DRW_Coord, 12/22/32), `fourPoint`(DRW_Coord, 13/23/33) |
| **`DRW_Solid`** | `DRW_Trace` | SOLID | 便捷方法: `firstCorner()`~`fourthCorner()`, `thick()`, `elevation()`, `extrusion()` |
| **`DRW_3Dface`** | `DRW_Trace` | 3DFACE | `invisibleflag`(int, 70)、枚举 `InvisibleEdgeFlags`、便捷方法: `firstCorner()`~`fourthCorner()`, `edgeFlags()` |
| **`DRW_Block`** | `DRW_Point` | BLOCK | `name`(UTF8STRING, 2), `flags`(int, 70) |
| **`DRW_Insert`** | `DRW_Point` | INSERT | `name`(UTF8STRING, 2), `xscale`(double, 41), `yscale`(double, 42), `zscale`(double, 43), `angle`(double, 50), `colcount`(int, 70), `rowcount`(int, 71), `colspace`(double, 44), `rowspace`(double, 45) |
| **`DRW_LWPolyline`** | `DRW_Entity` | LWPOLYLINE | `vertexnum`(int, 90), `flags`(int, 70), `width`(double, 43), `elevation`(double, 38), `thickness`(double, 39), `extPoint`(DRW_Coord, 210/220/230), `vertlist`(vector)、方法 `addVertex()`(两种重载), `applyExtrusion()` |
| **`DRW_Text`** | `DRW_Line` | TEXT | `height`(double, 40), `text`(UTF8STRING, 1), `angle`(double, 50), `widthscale`(double, 41), `oblique`(double, 51), `style`(UTF8STRING, 7), `textgen`(int, 71), `alignH`(enum HAlign, 72), `alignV`(enum VAlign, 73)。枚举 `VAlign`(VBaseLine/VBottom/VMiddle/VTop), `HAlign`(HLeft/HCenter/HRight/HAligned/HMiddle/HFit) |
| **`DRW_MText`** | `DRW_Text` | MTEXT | `interlin`(double, 44)、枚举 `Attach`(附着点 1-9: TopLeft~BottomRight) |
| **`DRW_Vertex`** | `DRW_Point` | VERTEX | `stawidth`(double, 40), `endwidth`(double, 41), `bulge`(double, 42), `flags`(int, 70), `tgdir`(double, 50), `vindex1~4`(int, 71-74), `identifier`(int, 91) |
| **`DRW_Polyline`** | `DRW_Point` | POLYLINE | `flags`(int, 70), `defstawidth`(double, 40), `defendwidth`(double, 41), `vertexcount`(int, 71), `facecount`(int, 72), `smoothM`(int, 73), `smoothN`(int, 74), `curvetype`(int, 75), `vertlist`(vector)、方法 `addVertex(DRW_Vertex)`, `appendVertex(shared_ptr<DRW_Vertex>)` |
| **`DRW_Spline`** | `DRW_Entity` | SPLINE | `normalVec`(DRW_Coord, 210/220/230), `tgStart`(DRW_Coord, 12/22/32), `tgEnd`(DRW_Coord, 13/23/33), `flags`(int, 70), `degree`(int, 71), `nknots`(dint32, 72), `ncontrol`(dint32, 73), `nfit`(dint32, 74), `tolknot`(double, 42), `tolcontrol`(double, 43), `tolfit`(double, 44), `knotslist`(vector\<double\>), `weightlist`(vector\<double\>), `controllist`(vector), `fitlist`(vector) |
| **`DRW_Hatch`** | `DRW_Point` | HATCH | `name`(UTF8STRING, 2), `solid`(int, 70), `associative`(int, 71), `hstyle`(int, 75), `hpattern`(int, 76), `doubleflag`(int, 77), `loopsnum`(int, 91), `angle`(double, 52), `scale`(double, 41), `deflines`(int, 78), `looplist`(vector)、方法 `appendLoop()` |
| **`DRW_Image`** | `DRW_Line` | IMAGE | `ref`(duint32, 340), `vVector`(DRW_Coord, 12/22/32), `sizeu`(double, 13), `sizev`(double, 23), `dz`(double, 33), `clip`(int, 280), `brightness`(int, 281), `contrast`(int, 282), `fade`(int, 283) |
| **`DRW_Dimension`** | `DRW_Entity` | DIMENSION | 见下方标注类详述 |
| **`DRW_DimAligned`** | `DRW_Dimension` | DIMALIGNED | get/set: `Clonepoint`, `DimPoint`, `Def1Point`, `Def2Point` |
| **`DRW_DimLinear`** | `DRW_DimAligned` | DIMLINEAR | get/set: `Angle`, `Oblique` |
| **`DRW_DimRadial`** | `DRW_Dimension` | DIMRADIAL | get/set: `CenterPoint`, `DiameterPoint`, `LeaderLength` |
| **`DRW_DimDiametric`** | `DRW_Dimension` | DIMDIAMETRIC | get/set: `Diameter1Point`, `Diameter2Point`, `LeaderLength` |
| **`DRW_DimAngular`** | `DRW_Dimension` | DIMANGULAR | get/set: `FirstLine1`, `FirstLine2`, `SecondLine1`, `SecondLine2`, `DimPoint` |
| **`DRW_DimAngular3p`** | `DRW_Dimension` | DIMANGULAR3P | get/set: `FirstLine`, `SecondLine`, `VertexPoint`, `DimPoint` |
| **`DRW_DimOrdinate`** | `DRW_Dimension` | DIMORDINATE | get/set: `OriginPoint`, `FirstLine`, `SecondLine` |
| **`DRW_Leader`** | `DRW_Entity` | LEADER | `style`(UTF8STRING, 3), `arrow`(int, 71), `leadertype`(int, 72), `flag`(int, 73), `hookline`(int, 74), `hookflag`(int, 75), `textheight`(double, 40), `textwidth`(double, 41), `vertnum`(int, 76), `coloruse`(int, 77), `annotHandle`(duint32, 340), `extrusionPoint`(DRW_Coord, 210/220/230), `horizdir`(DRW_Coord, 211/221/231), `offsetblock`(DRW_Coord, 212/222/232), `offsettext`(DRW_Coord, 213/223/233), `vertexlist`(vector) |
| **`DRW_Viewport`** | `DRW_Point` | VIEWPORT | `pswidth`(double, 40), `psheight`(double, 41), `vpstatus`(int, 68), `vpID`(int, 69), `centerPX/PY`(double, 12/22), `snapPX/PY`(double, 13/23), `snapSpPX/PY`(double, 14/24), `viewDir`(DRW_Coord, 16/26/36), `viewTarget`(DRW_Coord, 17/27/37), `viewLength`(double, 42), `frontClip`(double, 43), `backClip`(double, 44), `viewHeight`(double, 45), `snapAngle`(double, 50), `twistAngle`(double, 51) |

### 3.3 标注基类 `DRW_Dimension` — getter/setter

| getter | setter | 说明 (DXF code) |
|--------|--------|----------------|
| `getDefPoint()` | `setDefPoint(DRW_Coord)` | 定义点，10/20/30 (WCS) |
| `getTextPoint()` | `setTextPoint(DRW_Coord)` | 文字中点，11/21/31 (OCS) |
| `getStyle()` | `setStyle(std::string)` | 标注样式名，3 |
| `getAlign()` | `setAlign(int)` | 附着点，71 |
| `getTextLineStyle()` | `setTextLineStyle(int)` | 文字行距样式，72 |
| `getText()` | `setText(std::string)` | 用户显式输入的文字，1 |
| `getTextLineFactor()` | `setTextLineFactor(double)` | 文字行距因子，41 |
| `getDir()` | `setDir(double)` | 文字旋转角度，53 |
| `getExtrusion()` | `setExtrusion(DRW_Coord)` | 挤出法向量，210/220/230 |
| `getName()` | `setName(std::string)` | 块名，2 |
| `getMeasureValue()` | — | 真实测量值（只读），42 |
| `type` | — | 标注类型，70（公开字段） |

### 3.4 辅助类

| 类 | 说明 | 成员 |
|----|------|------|
| **`DRW_HatchLoop`** | 填充边界环 | `type`(int, 92), `numedges`(int, 93), `objlist`(vector\<shared_ptr\<DRW_Entity\>\>)、方法 `update()` |
| **`DRW_Vertex2D`** | LWPolyline 顶点 | `x, y`(double, 10/20), `stawidth`(double, 40), `endwidth`(double, 41), `bulge`(double, 42) |

---

## 四、基础类型与枚举

**文件**: `src/drw_base.h`

### 4.1 类型别名

| 别名 | 实际类型 |
|------|----------|
| `UTF8STRING` | `std::string` |

### 4.2 `DRW::Version` 枚举（DXF/DWG 版本）

| 枚举值 | 对应 AutoCAD 版本 |
|--------|-------------------|
| `UNKNOWNV` | 未知 |
| `MC00` | DWG Release 1.1 |
| `AC12` | DWG Release 1.2 |
| `AC14` | DWG Release 1.4 |
| `AC150` | DWG Release 2.0 |
| `AC210` | DWG Release 2.10 |
| `AC1002` | DWG Release 2.5 |
| `AC1003` | DWG Release 2.6 |
| `AC1004` | DWG Release 9 |
| `AC1006` | DWG Release 10 |
| `AC1009` | DWG Release 11/12 (LT R1/R2) |
| `AC1012` | DWG Release 13 (LT95) |
| `AC1014` | DWG Release 14/14.01 (LT97/LT98) |
| `AC1015` | AutoCAD 2000/2000i/2002 |
| `AC1018` | AutoCAD 2004/2005/2006 |
| `AC1021` | AutoCAD 2007/2008/2009 |
| `AC1024` | AutoCAD 2010/2011/2012 |
| `AC1027` | AutoCAD 2013~2017 |
| `AC1032` | AutoCAD 2018~2020 |

### 4.3 `DRW::error` 枚举（错误码）

| 枚举值 | 说明 |
|--------|------|
| `BAD_NONE` | 无错误 |
| `BAD_UNKNOWN` | 未知错误 |
| `BAD_OPEN` | 文件打开错误 |
| `BAD_VERSION` | 不支持的版本 |
| `BAD_READ_METADATA` | 元数据读取错误 |
| `BAD_READ_FILE_HEADER` | 文件头读取错误 |
| `BAD_READ_HEADER` | HEADER 段读取错误 |
| `BAD_READ_HANDLES` | 对象映射读取错误 |
| `BAD_READ_CLASSES` | CLASSES 段读取错误 |
| `BAD_READ_TABLES` | TABLES 段读取错误 |
| `BAD_READ_BLOCKS` | BLOCKS 段读取错误 |
| `BAD_READ_ENTITIES` | ENTITIES 段读取错误 |
| `BAD_READ_OBJECTS` | OBJECTS 段读取错误 |
| `BAD_READ_SECTION` | SECTION 读取错误 |
| `BAD_CODE_PARSED` | parseCode() 解析错误 |

### 4.4 `DRW::DebugLevel` 枚举

| 枚举值 | 说明 |
|--------|------|
| `DebugLevel::None` | 无调试输出 |
| `DebugLevel::Debug` | 输出调试信息 |

### 4.5 `DRW::ETYPE` 枚举（实体类型）

```
E3DFACE, ARC, BLOCK, CIRCLE, DIMENSION, DIMALIGNED, DIMLINEAR,
DIMRADIAL, DIMDIAMETRIC, DIMANGULAR, DIMANGULAR3P, DIMORDINATE,
ELLIPSE, HATCH, IMAGE, INSERT, LEADER, LINE, LWPOLYLINE, MTEXT,
POINT, POLYLINE, RAY, SOLID, SPLINE, TEXT, DXF_TRACE,
UNDERLAY, VERTEX, VIEWPORT, XLINE, UNKNOWN
```

### 4.6 其他枚举

| 枚举 | 值 |
|------|-----|
| `DRW::Space` | `ModelSpace = 0`, `PaperSpace = 1` |
| `DRW::ColorCodes` | `ColorByLayer = 256`, `ColorByBlock = 0` |
| `DRW::HandleCodes` | `NoHandle = 0` |
| `DRW::ShadowMode` | `CastAndReceieveShadows = 0`, `CastShadows = 1`, `ReceiveShadows = 2`, `IgnoreShadows = 3` |
| `DRW::MaterialCodes` | `MaterialByLayer = 0` |
| `DRW::PlotStyleCodes` | `DefaultPlotStyle = 0` |
| `DRW::TransparencyCodes` | `Opaque = 0`, `Transparent = -1` |

### 4.7 `DRW_LW_Conv::lineWidth` 枚举（线宽）

| 枚举值 | 实际宽度 | DXF 值 |
|--------|----------|--------|
| `width00` | 0.00mm | 0 |
| `width01` | 0.05mm | 5 |
| `width02` | 0.09mm | 9 |
| `width03` | 0.13mm | 13 |
| `width04` | 0.15mm | 15 |
| `width05` | 0.18mm | 18 |
| `width06` | 0.20mm | 20 |
| `width07` | 0.25mm | 25 |
| `width08` | 0.30mm | 30 |
| `width09` | 0.35mm | 35 |
| `width10` | 0.40mm | 40 |
| `width11` | 0.50mm | 50 |
| `width12` | 0.53mm | 53 |
| `width13` | 0.60mm | 60 |
| `width14` | 0.70mm | 70 |
| `width15` | 0.80mm | 80 |
| `width16` | 0.90mm | 90 |
| `width17` | 1.00mm | 100 |
| `width18` | 1.06mm | 106 |
| `width19` | 1.20mm | 120 |
| `width20` | 1.40mm | 140 |
| `width21` | 1.58mm | 158 |
| `width22` | 2.00mm | 200 |
| `width23` | 2.11mm | 211 |
| `widthByLayer` | ByLayer | -1 |
| `widthByBlock` | ByBlock | -2 |
| `widthDefault` | Default | -3 |

---

## 五、`DRW_Coord` — 3D 坐标

| 成员 | 类型 | 说明 |
|------|------|------|
| `x, y, z` | `double` | 坐标分量，默认 0 |
| `DRW_Coord()` | 构造函数 | 默认为 (0, 0, 0) |
| `DRW_Coord(double ix, double iy, double iz)` | 构造函数 | 指定坐标 |
| `void unitize()` | 方法 | 转换为单位向量 |

---

## 六、`DRW_Variant` — 变体类型

| 成员/方法 | 说明 |
|-----------|------|
| `enum TYPE {STRING, INTEGER, DOUBLE, COORD, INVALID}` | 数据类型枚举 |
| `DRW_Variant(int c, dint32 i)` | 整数构造 |
| `DRW_Variant(int c, duint32 i)` | 无符号整数构造 |
| `DRW_Variant(int c, double d)` | 浮点构造 |
| `DRW_Variant(int c, UTF8STRING s)` | 字符串构造 |
| `DRW_Variant(int c, DRW_Coord crd)` | 坐标构造 |
| `void addString(int c, UTF8STRING s)` | 重新赋值为字符串 |
| `void addInt(int c, int i)` | 重新赋值为整数 |
| `void addDouble(int c, double d)` | 重新赋值为浮点数 |
| `void addCoord(int c, DRW_Coord v)` | 重新赋值为坐标 |
| `void setCoordX/Y/Z(double d)` | 设置坐标分量 |
| `enum TYPE type() const` | 返回当前数据类型 |
| `int code()` | 返回 DXF group code |
| `content` 联合体 | `s`(string*), `i`(dint32), `d`(double), `v`(DRW_Coord*) |

---

## 七、`DRW_LW_Conv` — 线宽转换工具类（纯静态方法）

| 静态方法 | 说明 |
|----------|------|
| `static int lineWidth2dxfInt(enum lineWidth lw)` | 线宽枚举 → DXF code 370 整数值 |
| `static int lineWidth2dwgInt(enum lineWidth lw)` | 线宽枚举 → DWG 整数值 |
| `static enum lineWidth dxfInt2lineWidth(int i)` | DXF 整数值 → 线宽枚举 |
| `static enum lineWidth dwgInt2lineWidth(int i)` | DWG 整数值 → 线宽枚举 |

---

## 八、对象类（表格项）

**文件**: `src/drw_objects.h` 和 `src/drw_header.h`

| 类 | 说明 | 主要字段 |
|----|------|----------|
| **`DRW_LType`** | 线型定义 | `name`, `descr`, `flags`, `path`, 线段描述列表 |
| **`DRW_Layer`** | 图层定义 | `name`, `flags`, `color`, `lineType`, `plotF`, `lWeight`, `handle` |
| **`DRW_Dimstyle`** | 标注样式 | 大量标注系统变量（DIMSCALE, DIMTXT, DIMASZ 等） |
| **`DRW_Vport`** | 视口表 | 视口配置参数 |
| **`DRW_Textstyle`** | 文字样式 | `name`, `height`, `width`, `obliquing`, `genFlag`, `font` |
| **`DRW_AppId`** | 应用程序 ID | `name`, `flags` |
| **`DRW_Header`** | DXF 头部 | 数十个 DXF 系统变量 |
| **`DRW_ImageDef`** | 图像定义 | 图像路径、尺寸等信息 |
| **`DRW_PlotSettings`** | 打印设置 | 打印页面配置参数 |

---

## 九、典型使用流程

### 9.1 读取 DXF 文件

```cpp
#include "libdxfrw.h"

// 1. 继承 DRW_Interface，实现所有纯虚回调方法
class MyDxfReader : public DRW_Interface {
public:
    void addHeader(const DRW_Header* data) override { /* 处理头部 */ }
    void addLType(const DRW_LType& data) override { /* 处理线型 */ }
    void addLayer(const DRW_Layer& data) override { /* 处理图层 */ }
    void addDimStyle(const DRW_Dimstyle& data) override {}
    void addVport(const DRW_Vport& data) override {}
    void addTextStyle(const DRW_Textstyle& data) override {}
    void addAppId(const DRW_AppId& data) override {}
    void addBlock(const DRW_Block& data) override { /* 块开始 */ }
    void setBlock(const int handle) override {}
    void endBlock() override { /* 块结束 */ }
    void addPoint(const DRW_Point& data) override {}
    void addLine(const DRW_Line& data) override { /* 处理直线 */ }
    void addRay(const DRW_Ray& data) override {}
    void addXline(const DRW_Xline& data) override {}
    void addArc(const DRW_Arc& data) override { /* 处理圆弧 */ }
    void addCircle(const DRW_Circle& data) override { /* 处理圆 */ }
    void addEllipse(const DRW_Ellipse& data) override {}
    void addLWPolyline(const DRW_LWPolyline& data) override { /* 处理轻量多段线 */ }
    void addPolyline(const DRW_Polyline& data) override {}
    void addSpline(const DRW_Spline* data) override {}
    void addKnot(const DRW_Entity& data) override {}
    void addInsert(const DRW_Insert& data) override { /* 处理块插入 */ }
    void addTrace(const DRW_Trace& data) override {}
    void add3dFace(const DRW_3Dface& data) override {}
    void addSolid(const DRW_Solid& data) override {}
    void addMText(const DRW_MText& data) override { /* 处理多行文字 */ }
    void addText(const DRW_Text& data) override { /* 处理单行文字 */ }
    void addDimAlign(const DRW_DimAligned *data) override {}
    void addDimLinear(const DRW_DimLinear *data) override {}
    void addDimRadial(const DRW_DimRadial *data) override {}
    void addDimDiametric(const DRW_DimDiametric *data) override {}
    void addDimAngular(const DRW_DimAngular *data) override {}
    void addDimAngular3P(const DRW_DimAngular3p *data) override {}
    void addDimOrdinate(const DRW_DimOrdinate *data) override {}
    void addLeader(const DRW_Leader *data) override {}
    void addHatch(const DRW_Hatch *data) override {}
    void addViewport(const DRW_Viewport& data) override {}
    void addImage(const DRW_Image *data) override {}
    void linkImage(const DRW_ImageDef *data) override {}
    void addComment(const char* comment) override {}
    void addPlotSettings(const DRW_PlotSettings *data) override {}
    void writeHeader(DRW_Header& data) override {}
    void writeBlocks() override {}
    void writeBlockRecords() override {}
    void writeEntities() override {}
    void writeLTypes() override {}
    void writeLayers() override {}
    void writeTextstyles() override {}
    void writeVports() override {}
    void writeDimstyles() override {}
    void writeObjects() override {}
    void writeAppId() override {}
};

// 2. 创建 reader 并读取
MyDxfReader myReader;
dxfRW dxf("path/to/file.dxf");
if (dxf.read(&myReader, true)) {
    // 读取成功，数据已通过回调传入 myReader
} else {
    // 读取失败，检查 dxf.getError()
}
```

### 9.2 写入 DXF 文件

```cpp
class MyDxfWriter : public DRW_Interface {
    dxfRW* m_dxf;
public:
    MyDxfWriter(dxfRW* dxf) : m_dxf(dxf) {}

    void writeHeader(DRW_Header& data) override {
        // 设置头部变量，例如：
        // data["$ACADVER"] = DRW_Variant(1, std::string("AC1027"));
    }
    void writeEntities() override {
        // 写入实体
        DRW_Line line;
        line.basePoint = DRW_Coord(0, 0, 0);
        line.secPoint = DRW_Coord(10, 10, 0);
        line.layer = "0";
        m_dxf->writeLine(&line);

        DRW_Circle circle;
        circle.basePoint = DRW_Coord(5, 5, 0);
        circle.radious = 3.0;
        m_dxf->writeCircle(&circle);
    }
    // ... 其他写入回调（writeLayers 等）
};

dxfRW dxf("output.dxf");
MyDxfWriter writer(&dxf);
dxf.write(&writer, DRW::AC1027, false);  // DXF 2013 格式，ASCII
```

---

## 十、宏常量

| 宏 | 值 | 说明 |
|----|-----|------|
| `DRW_VERSION` | `"0.6.3"` | 库版本号 |
| `M_PIx2` | 6.283185307179586 | 2π |
| `ARAD` | 57.29577951308232 | 弧度转角度系数 (180/π) |
| `UTF8STRING` | `std::string` | 字符串类型别名 |
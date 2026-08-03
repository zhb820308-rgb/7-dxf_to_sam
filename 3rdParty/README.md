# 第三方依赖

本目录保存项目构建和运行所需的实际第三方开发包，而不是额外的 CMake
检索脚本。根目录 `CMakeLists.txt` 默认从这里查找依赖，同时仍允许通过
CMake 缓存变量或环境变量覆盖本地路径。

> 仓库只跟踪本说明文件，不上传第三方库的实际内容。首次克隆后，请按照
> 下列版本和目录约定自行复制或安装依赖；不要对这些本地文件执行强制暂存。

## 已固定的依赖

| 目录 | 版本 | 用途 | 上游来源 | 许可证 |
| --- | --- | --- | --- | --- |
| `libdxfrw` | 0.6.3 | 读取 DXF/DWG 数据 | <https://github.com/LibreCAD/libdxfrw> | GPL-2.0-or-later；源码文件中保留原始声明 |
| `spdlog` | 1.15.1 | 导入过程日志 | <https://github.com/gabime/spdlog> | MIT |
| `OCCT` | 7.6.0 | 曲线、拓扑与几何计算 | <https://github.com/Open-Cascade-SAS/OCCT> | LGPL-2.1，附带 OCCT 特别例外 |
| `googletest` | 1.15.2 | C++ 单元测试框架 | <https://github.com/google/googletest> | BSD-3-Clause |

## 目录约定

```text
3rdParty/
├── libdxfrw/
│   ├── src/             # 头文件和对应源码
│   ├── bin/dxfrw.dll    # VC14 x64 运行库
│   └── lib/dxfrw.lib    # VC14 x64 导入库
├── spdlog/
│   ├── include/
│   ├── bin/
│   ├── lib/
│   └── share/spdlog/    # spdlogConfig.cmake
├── googletest/          # 离线构建 C++ 测试所需的固定源码
└── OCCT/
    ├── inc/
    └── win64/vc14/
        ├── bin/
        └── lib/
```

`build/`、PDB、EXP、OBJ、崩溃转储和运行日志属于本机生成物，不应提交。
升级依赖时，应同时更新本文件中的版本、来源、许可证说明以及对应的 CMake
路径，并重新执行完整构建和测试。

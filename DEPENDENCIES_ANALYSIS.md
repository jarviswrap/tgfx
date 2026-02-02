# TGFX 第三方依赖分析报告

本文档根据 `CMakeLists.txt`、`DEPS` 文件以及源代码引用情况，对 TGFX 项目的所有第三方依赖进行了全面的梳理和分析。

## 1. 依赖管理机制

TGFX 采用混合的依赖管理策略：
*   **源码同步依赖 (Synced Dependencies)**: 通过 `DEPS` 文件定义，使用 `sync_deps.sh` 脚本从 Git 仓库拉取源码。存放于 `third_party/` 目录。
*   **预编译依赖 (Vendored Dependencies)**: 直接以头文件和预编译库的形式提交在代码仓库中。存放于 `vendor/` 目录。
*   **构建工具依赖**: 用于辅助编译和依赖管理的工具链。

## 2. 源码同步依赖列表 (Managed by DEPS)

这些依赖位于 `third_party/` 目录，需通过 `depsync` 工具同步。

| 依赖库 | 版本 (Commit ID) | 主要用途 | 关键使用位置 | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| **pathkit** | `6dc44ae` | 路径操作核心库 (基于 Skia SkPath) | `src/core/PathRef.h`<br>`src/core/PathTriangulator.cpp` | **核心依赖**，静态链接 |
| **skcms** | `0a4e501` | 色彩空间管理与转换 | `src/core/ColorSpace.cpp`<br>`src/core/codecs/*` | **核心依赖**，静态链接 |
| **highway** | `a523516` | SIMD 指令集优化库 | `src/core/MatrixSIMD.cpp`<br>`src/core/RectSIMD.cpp` | **核心依赖**，性能优化 |
| **concurrentqueue** | `6dd38b8` | 无锁并发队列 | `src/core/utils/TaskGroup.h`<br>`src/core/utils/ReturnQueue.h` | 仅头文件，多线程渲染 |
| **expat (libexpat)** | `88b3ed5` | XML 解析 | `src/svg/xml/XMLParser.cpp` | **SVG 模块**依赖 |
| **freetype** | `42608f7` | 字体解析与光栅化 | `src/core/vectors/freetype/*` | 矢量图形后端 (非 Apple 平台) |
| **harfbuzz** | `f1f2be7` | 文本塑形 (Shaping) | `src/pdf/fontSubset/PDFSubsetFont.cpp` | **PDF 模块**依赖 |
| **libpng** | `872555f` | PNG 图片编解码 | `src/core/codecs/png/PngCodec.cpp` | 图片解码模块 |
| **libjpeg-turbo** | `0a9b972` | JPEG 图片编解码 | `src/core/codecs/jpeg/JpegCodec.cpp` | 图片解码模块 |
| **libwebp** | `233960a` | WebP 图片编解码 | `src/core/codecs/webp/WebpCodec.cpp` | 图片解码模块 |
| **zlib** | `51b7f2a` | 数据压缩 | `src/pdf/DeflateStream.cpp` | PDF 生成及 PNG 依赖 |
| **flatbuffers** | `1c51462` | 高效序列化库 | `src/inspect/serialization/SerializationUtils.h` | **Inspector** 工具依赖 |
| **lz4** | `cacca37` | 极速压缩算法 | `src/inspect/LZ4CompressionHandler.cpp` | **Inspector** 工具依赖 |
| **googletest** | `6910c9d` | 单元测试框架 | `test/src/**/*` | 仅用于测试 |
| **json** (nlohmann) | `fec56a1` | JSON 解析 | `test/src/utils/Baseline.cpp` | 仅用于测试基准对比 |

## 3. 预编译依赖列表 (Located in vendor/)

这些依赖以预编译形式存在于 `vendor/` 目录，通常用于特定的图形后端支持。

| 依赖库 | 包含内容 | 主要用途 | 关键使用位置 | 触发条件 |
| :--- | :--- | :--- | :--- | :--- |
| **swiftshader** | 头文件 + 动态库 (`.so`/`.dll`/`.dylib`) | CPU 软件光栅化 OpenGL 实现 | `CMakeLists.txt` (Line 343) | `TGFX_USE_SWIFTSHADER=ON` |
| **angle** | 头文件 + 静态库/动态库 | OpenGL ES 到其他图形 API (D3D/Metal/Vk) 的转换层 | `CMakeLists.txt` (Line 348) | `TGFX_USE_ANGLE=ON` |

## 4. 构建配置依赖分析

### 核心模块
*   `pathkit`, `skcms`, `highway` 是 TGFX 运行的基础，默认始终编译链接。

### 可选模块开关
*   **SVG 模块** (`TGFX_BUILD_SVG=ON`): 强依赖 **expat** 进行 XML 解析。
*   **PDF 模块** (`TGFX_BUILD_PDF=ON`): 强依赖 **harfbuzz** (字体子集化) 和 **zlib** (流压缩)。
*   **Inspector** (`TGFX_USE_INSPECTOR=ON`): 强依赖 **flatbuffers** 和 **lz4** 用于性能数据传输。

### 图形后端选择
TGFX 支持多种 OpenGL 后端，通过 `vendor/` 下的库支持：
1.  **Native GL** (默认): 使用系统提供的 OpenGL。
2.  **SwiftShader**: 纯软件渲染，用于无 GPU 环境。
3.  **ANGLE**: 跨平台 OpenGL ES 兼容层，常用于 Windows (转 D3D) 或 macOS (转 Metal)。

## 5. 工具链与构建环境

### 依赖管理工具 (depsync)
*   **定义**: 一个基于 Node.js 的轻量级多仓库管理工具，类似于 Chromium 的 `gclient`。
*   **功能**:
    *   读取根目录 `DEPS` 文件。
    *   自动同步第三方库源码。
    *   支持执行 post-sync 钩子（如清理操作）。
*   **集成**: 集成在 `vendor.cmake` 中，CMake 配置阶段会自动触发检查。

### Web 平台构建工具 (Emscripten)
*   **定义**: 一个基于 LLVM 的编译器工具链，用于将 C/C++ 代码编译为 WebAssembly (Wasm) 和 JavaScript。
*   **在 TGFX 中的作用**:
    *   **核心编译**: 将 TGFX C++ 核心编译为 Wasm，使其能在浏览器运行。
    *   **图形映射**: 将 TGFX 的 OpenGL 调用自动转换为 WebGL 调用。
    *   **JS 绑定**: 通过 Embind 暴露 C++ 接口 (Matrix, Rect 等) 给前端 JavaScript。
    *   **多线程**: 支持编译 `wasm-mt` 版本，利用 Web Workers 实现高性能渲染。

### 跨平台构建辅助 (vendor_tools)
*   **vendor_tools**: 位于 `third_party/vendor_tools`，是一套基于 Node.js 和 CMake 的构建辅助工具，用于跨平台管理上述依赖的下载、编译和合并。它不直接参与运行时，但在构建过程中至关重要。

## 6. 系统级依赖 (External)
*   **Qt6**: 当开启 `TGFX_USE_QT=ON` 时，需要系统安装 Qt6 环境 (`Core`, `Widgets`, `OpenGL`, `Quick` 组件)。

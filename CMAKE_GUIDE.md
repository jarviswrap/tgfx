# TGFX CMake 构建指南

本文档详细解释 `CMakeLists.txt` 的配置逻辑，帮助开发者理解 TGFX 的编译模块、选项开关及构建流程。

## 1. 项目基础配置

*   **CMake 版本**: 最低要求 `3.13`。
*   **C++ 标准**: 强制使用 `C++17` (`CMAKE_CXX_STANDARD 17`)。
*   **符号可见性**: 默认为 `hidden` (`CMAKE_CXX_VISIBILITY_PRESET hidden`)，仅导出必要的 API，有助于减小库体积并避免符号冲突。
*   **Vendor 工具链**: 引入 `third_party/vendor_tools/vendor.cmake`，这是 TGFX 自定义的依赖管理和构建脚本入口。

## 2. 编译选项 (Options)

TGFX 提供了一系列 `option` 开关，用于定制编译产物的功能。可以通过 `cmake -D<OPTION>=ON/OFF` 进行控制。

### 核心功能开关

| 选项名 | 默认值 | 说明 |
| :--- | :--- | :--- |
| **TGFX_BUILD_SVG** | `OFF` | 是否编译 SVG 解析与渲染模块。 |
| **TGFX_BUILD_PDF** | `OFF` | 是否编译 PDF 生成模块。 |
| **TGFX_BUILD_LAYERS** | `OFF` | 是否编译 Layer 模块（用于 Hello2D 示例等）。 |
| **TGFX_USE_OPENGL** | `ON` | 是否启用 OpenGL GPU 后端。 |
| **TGFX_USE_THREADS** | `ON` | 是否启用多线程渲染支持（Web 平台默认关闭）。 |
| **TGFX_USE_TEXT_GAMMA_CORRECTION** | `OFF` | 渲染文本时是否启用 Gamma 校正。 |

### 图形后端与第三方库开关

| 选项名 | 默认值 | 说明 |
| :--- | :--- | :--- |
| **TGFX_USE_QT** | `OFF` | 是否使用 Qt 框架进行构建（用于 Qt Demo）。 |
| **TGFX_USE_SWIFTSHADER** | `OFF` | 是否使用 SwiftShader (CPU 软件 OpenGL) 库。 |
| **TGFX_USE_ANGLE** | `OFF` | 是否使用 ANGLE (GL 转 Metal/D3D) 库。 |
| **TGFX_USE_FREETYPE** | 平台相关 | 是否使用 Freetype 进行字体渲染（非 Apple 平台默认开启）。 |

### 图片编解码开关

这些选项控制是否在 TGFX 内部集成特定的图片编解码库。

| 选项名 | 说明 |
| :--- | :--- |
| **TGFX_USE_PNG_DECODE** | 启用 PNG 解码支持 (libpng)。 |
| **TGFX_USE_PNG_ENCODE** | 启用 PNG 编码支持 (libpng)。 |
| **TGFX_USE_JPEG_DECODE** | 启用 JPEG 解码支持 (libjpeg-turbo)。 |
| **TGFX_USE_JPEG_ENCODE** | 启用 JPEG 编码支持 (libjpeg-turbo)。 |
| **TGFX_USE_WEBP_DECODE** | 启用 WebP 解码支持 (libwebp)。 |
| **TGFX_USE_WEBP_ENCODE** | 启用 WebP 编码支持 (libwebp)。 |

> **注意**: 在 Android 和 OHOS (OpenHarmony) 平台上，通常使用系统自带的能力，因此默认关闭这些内置解码库。在 Linux/Windows 等平台上默认开启。

### 调试与测试开关

| 选项名 | 默认值 | 说明 |
| :--- | :--- | :--- |
| **TGFX_BUILD_TESTS** | `OFF` | 是否编译测试用例 (`TGFXFullTest`)。 |
| **TGFX_BUILD_HELLO2D** | `OFF` | 是否编译 Hello2D 示例库。 |
| **TGFX_USE_INSPECTOR** | `OFF` | 是否启用性能分析器 (Inspector) 支持 (Debug 模式下默认开启)。 |

## 3. 平台特定配置

CMake 脚本根据目标平台 (`APPLE`, `ANDROID`, `WEB`, `WIN32`, `LINUX`, `OHOS`) 自动配置源文件和依赖库。

*   **Android**:
    *   链接 `log`, `android`, `jnigraphics`, `EGL`, `GLESv2` 等系统库。
    *   开启编译优化选项 (`-Os`, `-fno-exceptions`, `-fno-rtti`) 以减小包体积。
*   **iOS/macOS (Apple)**:
    *   链接 `CoreGraphics`, `QuartzCore`, `CoreText`, `ImageIO` 等系统框架。
    *   根据配置选择 `OpenGLES` (iOS) 或 `OpenGL` (macOS) 库。
    *   支持构建为 Framework (`TGFX_BUILD_FRAMEWORK`)。
*   **Web (Emscripten)**:
    *   禁用 RTTI。
    *   链接 WebGL 相关库。
*   **Windows (WIN32)**:
    *   链接 `DWrite`, `WindowsCodecs` 等系统库。
    *   支持 WGL (Windows OpenGL)。

## 4. 依赖管理

脚本通过 `add_vendor_target` 和 `find_vendor_libraries` 函数（来自 `vendor.cmake`）管理第三方依赖。

*   **静态依赖 (`TGFX_STATIC_VENDORS`)**:
    *   `pathkit`, `skcms`, `highway` (核心依赖)。
    *   `libpng`, `libjpeg-turbo`, `libwebp`, `zlib`, `freetype`, `harfbuzz`, `expat` (根据开关按需加入)。
*   **共享依赖 (`TGFX_SHARED_VENDORS`)**:
    *   `swiftshader` 等（如果是动态库形式）。

`merge_libraries_into` 函数用于将这些静态依赖库合并到最终的 `tgfx` 静态库中，简化使用者的链接过程。

## 5. 构建目标 (Targets)

### `tgfx` (Static Library)
这是项目的主产物。
*   **源文件**: 包含 `include/` 头文件和 `src/` 下的核心代码，以及根据平台和模块开关筛选后的文件。
*   **包含路径**: `include` (公开), `src` (私有), 以及第三方库的 include 路径。
*   **依赖合并**: 所有启用的静态第三方库都会被合并进来。

### `tgfx-hello2d` (Static Library)
*   **条件**: `TGFX_BUILD_HELLO2D=ON`。
*   **用途**: 一个简单的 2D 绘图示例库，用于测试和演示。

## 6. 工具链与依赖同步 (Depsync)

TGFX 使用名为 `depsync` 的工具来管理第三方依赖，类似于 Chromium 的 `gclient`。

*   **定义**: 一个基于 Node.js 的命令行工具，用于多仓库管理。
*   **配置文件**: 项目根目录下的 `DEPS` 文件定义了需要同步的依赖库及其版本（Git Commit ID）。
*   **工作流程**:
    1.  CMake 配置阶段会自动执行 `depsync`（通过 `vendor.cmake`）。
    2.  `depsync` 读取 `DEPS` 文件。
    3.  将指定的依赖库（如 `freetype`, `libpng`, `zlib` 等）下载到 `third_party` 目录下。
    4.  执行定义的 `actions`（如清理旧文件）。
*   **手动使用**: 也可以在命令行手动运行 `depsync` 来同步依赖。


## 7. Vendor Tools 详解

`vendor_tools` 是 TGFX 的**依赖构建系统**，位于 `third_party/vendor_tools` 目录。

### 与 depsync 的区别

*   **depsync**: 负责 **"下载"**。它类似于包管理器，根据 `DEPS` 文件将第三方库的源码拉取到本地。
*   **vendor_tools**: 负责 **"构建"**。它提供了一套统一的跨平台构建脚本，根据 `vendor.json` 的配置，将下载好的源码编译成静态库（.a/.lib）。

### 核心组成

1.  **vendor.cmake**:
    *   被主 `CMakeLists.txt` 引用。
    *   提供了 `add_vendor_target` 等函数，将构建好的第三方库引入到 TGFX 的构建系统中。
    *   在 CMake 配置阶段触发 `depsync`。

2.  **vendor.json**:
    *   位于项目根目录。
    *   定义了每个第三方库的构建规则（源码路径、CMake 目标、编译参数、导出的头文件等）。
    *   **示例**:
        ```json
        {
          "name": "zlib",
          "cmake": {
            "targets": ["zlibstatic"],
            "includes": ["${SOURCE_DIR}/zlib.h", "${BUILD_DIR}/zconf.h"]
          }
        }
        ```

3.  **命令行工具**:
    *   `vendor-build`: 用于手动编译 `vendor.json` 中定义的库。
    *   `lib-merge`: 用于将多个静态库合并为一个。


## 8. Web 平台支持 (Emscripten)

TGFX 使用 **Emscripten** 工具链将 C++ 核心代码编译为 WebAssembly (Wasm)，从而在浏览器中运行。

*   **角色**: 充当 C++ 到 Web 的编译器和桥梁。
*   **编译产物**: 生成 `.wasm` 文件（二进制指令）和 `.js` 文件（胶水代码）。
*   **核心功能**:
    1.  **OpenGL -> WebGL**: 将 TGFX 的 OpenGL 调用映射为浏览器的 WebGL API (`src/gpu/opengl/webgl/`)。
    2.  **JS Bindings**: 使用 `embind` (`emscripten/bind.h`) 将 C++ 类（如 `Matrix`, `Rect`）暴露给 JavaScript 调用 (`src/platform/web/TGFXWasmBindings.cpp`)。
    3.  **多线程支持**: 支持编译为 `wasm-mt` (Multithreaded) 版本，利用 Web Workers 进行渲染。
*   **构建命令**:
    在 `web/` 目录下运行 `npm run build` 会调用 Emscripten 工具链进行构建。




### `TGFXFullTest` (Executable)
*   **条件**: `TGFX_BUILD_TESTS=ON`。
*   **用途**: 运行所有单元测试。
*   **依赖**: 链接 `tgfx`, `googletest`, `harfbuzz` 等。

### `UpdateBaseline` (Executable)
*   **条件**: `TGFX_BUILD_TESTS=ON`。
*   **用途**: 用于生成或更新测试基准图像，确保渲染结果的一致性。

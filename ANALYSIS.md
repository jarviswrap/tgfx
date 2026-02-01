# TGFX 项目深度分析与架构文档

## 1. 项目概览

TGFX (Tencent Graphics) 是一个跨平台的轻量级 2D 图形渲染引擎，旨在提供高性能的图形渲染能力，支持矢量图形、图片、文本等内容的绘制。它设计上类似于 Google Skia，但体积更小，专注于现代 GPU (OpenGL/ES, WebGL, Metal 等) 渲染。

**核心特性：**
*   **跨平台支持**：Android, iOS, macOS, Windows, Linux, Web (Wasm), OpenHarmony, Qt。
*   **双层 API**：
    *   **Core API (Immediate Mode)**: 类似 HTML5 Canvas 或 Skia 的即时绘制接口 (`Canvas`, `Paint`, `Path`)。
    *   **Layer API (Retained Mode)**: 基于树状结构的场景图系统 (`Layer`, `RenderNode`)，支持复杂的层级变换、遮罩、滤镜和 3D 变换。
*   **GPU 加速**：基于 `Device` 和 `Context` 的抽象层，默认使用 OpenGL/ES 后端，设计上支持扩展到 Metal/Vulkan。
*   **高性能**：包含自动批处理 (Batching)、纹理图集 (Atlas)、缓存机制。

## 2. 目录结构说明

```
tgfx/
├── include/tgfx/           # 公共头文件 (Public API)
│   ├── core/               # 核心图形 API (Canvas, Paint, Path, Image, Matrix)
│   ├── gpu/                # GPU 后端抽象 (Device, Context, Texture)
│   ├── layers/             # 场景图系统 (Layer, TextLayer)
│   ├── svg/                # SVG 解析与渲染
│   └── pdf/                # PDF 生成支持
├── src/                    # 源代码实现 (Private Implementation)
│   ├── core/               # 核心图形算法与实现
│   ├── gpu/                # GPU 后端具体实现
│   │   ├── opengl/         # OpenGL/ES 后端实现
│   │   ├── ops/            # 绘图操作 (DrawOp) 及其批处理逻辑
│   │   ├── processors/     # Shader 处理器 (FragmentProcessor, GeometryProcessor)
│   │   └── tasks/          # 渲染任务 (RenderTask)
│   ├── layers/             # Layer 系统实现
│   ├── platform/           # 平台相关实现 (Android, iOS, Web, etc.)
│   ├── svg/                # SVG 模块实现
│   └── pdf/                # PDF 模块实现
├── third_party/            # 第三方依赖 (Freetype, libpng, zlib, etc.)
├── hello2d/                # 示例工程
├── CMakeLists.txt          # 主构建脚本
├── DEPS                    # 依赖定义文件 (depsync 工具使用)
└── vendor.json             # 依赖版本管理
```

## 3. 核心架构分析

### 3.1 核心绘图层 (Core Graphics)

这是 TGFX 的基础层，提供了类似 Skia 的绘图 API。

*   **`tgfx::Canvas`**: 绘图上下文。维护了状态栈 (Matrix, Clip)，提供了 `drawRect`, `drawPath`, `drawImage`, `drawText` 等方法。
    *   `save()` / `restore()`: 保存和恢复变换矩阵与裁剪区域。
    *   `saveLayer()`: 创建离屏渲染层（用于复杂混合或滤镜）。
*   **`tgfx::Paint`**: 绘制风格描述。
    *   `Color`, `Alpha`, `BlendMode`: 颜色与混合模式。
    *   `Style`: Fill (填充) 或 Stroke (描边)。
    *   `Shader`: 渐变 (Gradient) 或纹理 (Pattern)。
    *   `MaskFilter` / `ColorFilter` / `ImageFilter`: 各种滤镜效果。
*   **`tgfx::Path`**: 矢量路径描述。支持直线、贝塞尔曲线 (Quad/Cubic)、圆弧等。
*   **`tgfx::Image`**: 位图图像。可以是纯像素数据 (`PixelImage`) 或纹理图像 (`TextureImage`)。

### 3.2 GPU 渲染管线 (GPU Backend)

TGFX 将高层绘图指令转换为 GPU 命令。

1.  **`DrawContext`**: `Canvas` 将调用转发给 `DrawContext`。
2.  **`OpsTask` / `OpsRenderTask`**: 渲染任务管理器。它负责收集绘图操作。
3.  **`DrawOp`**: 原子绘图操作。例如 `RectDrawOp`, `AtlasTextOp`。
    *   每个 `Canvas.drawXxx` 调用通常对应一个或多个 `DrawOp`。
    *   `DrawOp` 包含了绘制所需的几何信息 (Geometry) 和着色信息 (Processors)。
4.  **`OpsCompositor`**: 负责优化和执行 `DrawOp`。它会尝试合并（Batching）可以一起绘制的操作以减少 Draw Call。
5.  **`RenderPass`**: 渲染通道抽象。
    *   定义了渲染目标 (RenderTarget)、清除操作 (Clear)、加载/存储动作 (Load/Store Action)。
    *   提供了 `setPipeline`, `setVertexBuffer`, `draw` 等底层指令。
6.  **`GLDevice` / `GLRenderPass`**: OpenGL 具体实现。将 `RenderPass` 指令转换为 `glUseProgram`, `glBindBuffer`, `glDrawArrays` 等 GL 调用。

**流程图解：**
`Canvas` -> `DrawContext` -> `OpsRenderTask` -> `DrawOp` (List) -> `OpsCompositor` (Batching) -> `RenderPass` -> `GPU API (OpenGL)`

### 3.3 场景图系统 (Layer System)

构建在 Core Graphics 之上的保留模式 (Retained Mode) 系统，适合构建 UI 或动画。

*   **`tgfx::Layer`**: 所有图层的基类。
    *   **属性**: `position`, `scale`, `rotation`, `alpha`, `visible`。
    *   **层级**: `addChild`, `removeChild`, `parent`。
    *   **高级特性**:
        *   `matrix3D`: 支持 3D 透视变换。
        *   `mask`: 支持图层遮罩。
        *   `filters`: 支持实时滤镜 (Blur, DropShadow 等)。
*   **绘制流程**:
    *   `Layer::draw(Canvas* canvas)`: 递归绘制子图层。
    *   如果图层有特殊效果 (如滤镜或非默认混合模式)，会自动调用 `canvas->saveLayer()` 进行离屏渲染。

## 4. 构建系统与依赖管理

TGFX 使用 **CMake** 作为构建系统，并配合 Node.js 工具链管理依赖。

### 4.1 依赖管理
*   配置文件：`DEPS` 和 `vendor.json`。
*   工具：`depsync` (npm install -g depsync)。
*   流程：运行 `depsync` 或 `./sync_deps.sh` 会根据 `DEPS` 文件下载第三方库源码到 `third_party/` 目录。
*   主要依赖：
    *   `freetype`: 字体渲染。
    *   `libpng`, `libjpeg-turbo`, `libwebp`: 图片编解码。
    *   `pathkit`: 路径操作。
    *   `harfbuzz`: 文本整形 (Shaping)。

### 4.2 编译配置 (CMake)
主要 CMake 选项 (在 `CMakeLists.txt` 中定义):
*   `TGFX_USE_OPENGL`: 启用 OpenGL 后端 (默认 ON)。
*   `TGFX_BUILD_LAYERS`: 启用 Layer 模块。
*   `TGFX_BUILD_SVG`: 启用 SVG 支持。
*   `TGFX_BUILD_PDF`: 启用 PDF 导出。
*   `TGFX_USE_QT`: 启用 Qt 集成。

### 4.3 平台适配
`src/platform` 目录下包含各平台的特定实现：
*   **Web**: 使用 Emscripten 编译为 WebAssembly。
*   **Android**: JNI 绑定和 AHardwareBuffer 支持。
*   **Apple (iOS/macOS)**: CoreText 字体支持，CGL/EAGL 上下文管理。

## 5. 关键代码导读

*   **绘图入口**: `include/tgfx/core/Canvas.h` - 查看 `drawRect`, `drawPath` 等方法。
*   **GPU 设备**: `src/gpu/opengl/GLDevice.cpp` - 查看 OpenGL 上下文如何创建和管理。
*   **渲染操作**: `src/gpu/ops/DrawOp.h` - 理解绘图操作的基类。
*   **图层实现**: `src/layers/Layer.cpp` - 理解图层属性更新和绘制逻辑。
*   **路径实现**: `include/tgfx/core/Path.h` - 理解矢量路径的构建方式。

## 6. 示例代码 (C++)

```cpp
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Paint.h"
#include "tgfx/gpu/Surface.h"

void DrawDemo(tgfx::Surface* surface) {
    auto canvas = surface->getCanvas();
    
    // 1. 清屏
    canvas->clear(tgfx::Color::White());
    
    // 2. 绘制红色矩形
    tgfx::Paint paint;
    paint.setColor(tgfx::Color::Red());
    canvas->drawRect(tgfx::Rect::MakeXYWH(10, 10, 100, 100), paint);
    
    // 3. 绘制蓝色描边圆
    paint.setColor(tgfx::Color::Blue());
    paint.setStyle(tgfx::PaintStyle::Stroke);
    paint.setStrokeWidth(5);
    canvas->drawCircle(200, 60, 50, paint);
    
    // 4. 提交绘制
    surface->flush();
}
```

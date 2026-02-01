# TGFX 深度技术指南

## 1. 架构设计全景

TGFX 采用分层架构设计，自下而上分为：**平台适配层 (Platform)**、**GPU 抽象层 (GPU Backend)**、**核心图形层 (Core Graphics)** 和 **场景图层 (Layer System)**。

### 核心架构图解
```mermaid
graph TD
    A[Layer System] --> B[Core Graphics]
    B --> C[GPU Backend]
    C --> D[Platform]
    D --> E[OS / Graphics API]

    subgraph "Layer System"
    L1[Layer / RenderNode]
    L2[LayerFilter / LayerStyle]
    end

    subgraph "Core Graphics"
    C1[Canvas / Paint / Path]
    C2[Image / TextBlob]
    end

    subgraph "GPU Backend"
    G1[OpsCompositor (Batching)]
    G2[DrawOp / RenderPass]
    G3[ResourceCache]
    G4[ShaderBuilder]
    end

    subgraph "Platform"
    P1[Window / Surface]
    P2[Device / Context]
    P3[ImageCodec]
    end
```

## 2. 图形渲染管线 (Rendering Pipeline)

TGFX 的渲染管线采用了**延迟渲染 (Deferred Rendering)** 和 **流式批处理 (Streaming Batching)** 机制，以最大化 GPU 吞吐量。

### 2.1 渲染流程
1.  **记录 (Record)**: 上层调用 `Canvas` API (如 `drawRect`)。
2.  **转换 (Translate)**: `Canvas` 将调用转发给 `DrawContext`，后者根据当前的 Matrix 和 Clip 生成中间数据结构。
3.  **批处理 (Batch)**:
    *   `OpsCompositor` 接收绘制请求，尝试将新的操作与 `PendingOps` 合并。
    *   **合并条件**: 相同的 Pipeline State (Shader, BlendMode, AA) 和 Clip。
    *   **不中断批次**: 仅颜色 (Color) 不同**不会**中断批次，颜色作为顶点属性传输。
4.  **生成 (Generate)**:
    *   当批次中断时，生成具体的 `DrawOp` (如 `RectDrawOp`, `AtlasTextOp`)。
    *   `DrawOp` 包含几何处理器 (`GeometryProcessor`) 和片段处理器 (`FragmentProcessor`)。
5.  **提交 (Submit)**:
    *   `DrawingManager` 将 `DrawOp` 转换为 `RenderPass` 指令。
    *   `ShaderBuilder` 动态生成 GLSL 代码并编译 Program。
    *   `GLDevice` 执行最终的 OpenGL 指令 (`glDrawArrays` 等)。

### 2.2 Shader 动态生成
TGFX 不使用预编译 Shader，而是通过 **Processor Tree** 动态构建：
*   **FragmentProcessor**: 逻辑节点（如 Texture 采样、Gradient 计算、Blend 混合）。
*   **ShaderBuilder**: 遍历 Processor Tree，调用 `emitCode` 拼接 GLSL 字符串。
*   **优势**: 极大减少了 Shader 变体数量，仅生成当前绘制所需的最小代码集。

## 3. 跨平台适配层实现

| 功能模块 | Android | iOS | Windows | Web (Wasm) |
| :--- | :--- | :--- | :--- | :--- |
| **Window** | `EGLWindow`<br>(基于 `SurfaceView`) | `EAGLWindow`<br>(基于 `CAEAGLLayer`) | `WGLWindow`<br>(基于 `HWND`) | `WebGLWindow`<br>(基于 Canvas ID) |
| **Context** | `EGLDevice`<br>(`eglMakeCurrent`) | `EAGLDevice`<br>(`EAGLContext`) | `WGLDevice`<br>(`wglMakeCurrent`) | `WebGLDevice`<br>(WebGL Context) |
| **ImageCodec** | `JNI` -> `BitmapFactory` | `CoreGraphics`<br>(`CGImageSource`) | `WIC`<br>(`IWICBitmapDecoder`) | `Emscripten` -> `HTMLImage` |
| **Font** | `FreeType` + `HarfBuzz` | `CoreText` | `DirectWrite` | `FreeType` / Canvas |

### 3.1 关键类说明
*   **`Device`**: 抽象了图形设备的上下文管理（Lock/Unlock Context）。
*   **`Window`**: 抽象了原生窗口的生命周期和 SwapBuffer 操作。
*   **`NativeCodec`**: 提供了访问系统原生图片解码器的标准接口。

## 4. 性能优化策略

### 4.1 自动图集 (Auto Atlas)
针对文本渲染，TGFX 使用 `AtlasManager` 自动维护纹理图集。
*   **算法**: `RectPackSkyline` (天际线算法) 实现高效的矩形装箱。
*   **机制**: 将多个小字形 (Glyph) 合并到一个大纹理中，极大减少纹理切换和 Draw Call。

### 4.2 智能缓存 (Caching)
*   **`ResourceCache`**: GPU 资源缓存。使用引用计数 + LRU 策略。
*   **`GlobalCache`**: 缓存 Shader Program、Gradient Texture 等通用资源。
*   **`SubtreeCache`**: 针对复杂静态图层，将其渲染结果缓存为纹理（Texture），后续帧直接绘制纹理。

### 4.3 零拷贝更新 (Zero-Copy)
在 Android 平台上，利用 `AHardwareBuffer` 实现纹理与原生 Bitmap 的零拷贝共享，大幅提升视频帧上传和图片渲染效率。

## 5. 内存管理机制

TGFX 使用了一套精细的内存管理方案，结合了 C++ `shared_ptr` 和自定义回收队列。

### 5.1 引用计数与延迟回收
*   **机制**: 所有 `Resource` 均由 `std::shared_ptr` 管理。
*   **ReturnQueue**: 当引用计数归零时，自定义 Deleter **不会立即 delete** 对象，而是将其放入 `ReturnQueue`。
*   **复用**: 系统定期检查 `ReturnQueue`，将资源移入 `purgeableResources` (可复用列表)。如果此时有新的请求匹配该资源 (Scratch Key)，则直接复用，避免重新分配显存。

### 5.2 显存配额
*   **`maxBytes`**: 设置最大显存占用（默认 512MB）。
*   **驱逐 (Purge)**: 当显存超标时，LRU 算法会优先清理最久未使用的 `purgeable` 资源。

## 6. 使用场景示例

### 场景一：基础绘图 (Canvas API)
适用于绘制简单的几何图形、图片或文字。

```cpp
void DrawSimpleShape(tgfx::Surface* surface) {
    auto canvas = surface->getCanvas();
    canvas->clear(tgfx::Color::White());

    tgfx::Paint paint;
    paint.setColor(tgfx::Color::Red());
    paint.setStyle(tgfx::PaintStyle::Stroke);
    paint.setStrokeWidth(5);

    // 绘制一个圆
    canvas->drawCircle(100, 100, 50, paint);
    
    // 绘制文字
    auto font = tgfx::Font(tgfx::Typeface::MakeDefault());
    paint.setStyle(tgfx::PaintStyle::Fill);
    canvas->drawSimpleText("Hello TGFX", 50, 200, font, paint);
    
    surface->flush();
}
```

### 场景二：复杂层级与遮罩 (Layer API)
适用于构建 UI 树或动画，需要层级管理和遮罩效果。

```cpp
std::shared_ptr<tgfx::Layer> CreateMaskedLayer() {
    // 1. 创建内容层 (红色矩形)
    auto content = tgfx::ShapeLayer::Make();
    tgfx::Path rectPath;
    rectPath.addRect(0, 0, 200, 200);
    content->updateShape(rectPath, tgfx::Color::Red());

    // 2. 创建遮罩层 (圆形)
    auto mask = tgfx::ShapeLayer::Make();
    tgfx::Path circlePath;
    circlePath.addCircle(100, 100, 80);
    mask->updateShape(circlePath, tgfx::Color::Black());

    // 3. 应用遮罩
    content->setMask(mask);
    return content;
}
```

### 场景三：自定义 Shader (Effect)
适用于实现特殊的视觉效果，如灰度滤镜。

```cpp
// 灰度 Shader 的 FragmentProcessor 实现
class GrayscaleEffect : public tgfx::FragmentProcessor {
public:
    void emitCode(EmitArgs& args) const override {
        // 生成 GLSL 代码：将输入颜色的 RGB 转为 Luminance
        std::string inputColor = args.inputColor;
        args.fragBuilder->codeAppendf("float gray = dot(%s.rgb, vec3(0.299, 0.587, 0.114));", inputColor.c_str());
        args.fragBuilder->codeAppendf("%s = vec4(gray, gray, gray, %s.a);", args.outputColor.c_str(), inputColor.c_str());
    }
};

void DrawWithEffect(tgfx::Canvas* canvas) {
    tgfx::Paint paint;
    // 设置自定义 Shader
    auto shader = tgfx::Shader::Make(std::make_shared<GrayscaleEffect>());
    paint.setShader(shader);
    
    // 绘制图片，将应用灰度效果
    canvas->drawImage(myImage, paint);
}
```

## 7. 学习难点与建议

1.  **OpsCompositor 的批处理逻辑**: 理解 `canAppend` 的判断条件是优化渲染性能的关键。建议调试 `OpsCompositor.cpp` 中的 `record` 方法。
2.  **FragmentProcessor 树**: 理解 Shader 是如何通过节点嵌套生成的。建议阅读 `src/gpu/glsl/GLSLFragmentShaderBuilder.cpp`。
3.  **资源生命周期**: 熟悉 `ResourceCache` 和 `ReturnQueue` 的交互，避免显存泄漏。建议关注 `src/core/utils/ReturnQueue.cpp`。

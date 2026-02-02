# TGFX 深度剖析文档

本文档在 [TECHNICAL_GUIDE.md](TECHNICAL_GUIDE.md) 的基础上，对 TGFX 的核心模块进行源码级深度分析，重点关注实现细节、设计决策、调试技巧及兼容性安全策略。

## 1. 核心模块源码分析

### 1.1 渲染管线核心：OpsCompositor

`OpsCompositor` 是 tgfx 将高层绘图指令转换为 GPU `DrawOp` 的核心调度器，其核心职责是**批处理 (Batching)** 和 **Shader 构建准备**。

#### 1.1.1 关键函数：`canAppend` 与 `flushPendingOps`

源码位置：`src/gpu/OpsCompositor.cpp`

**`canAppend` 实现逻辑：**
该函数决定了当前的绘制操作是否可以合并到上一个批次中。这是减少 Draw Call 的关键。

```cpp
bool OpsCompositor::canAppend(PendingOpType type, const Path& clip, const Brush& brush) const {
  // 1. 基础状态检查：操作类型、Clip、Brush 必须一致
  if (pendingType != type || !pendingClip.isSame(clip) || !CompareBrush(pendingBrush, brush)) {
    return false;
  }
  // 2. 数量限制检查：避免单次 Draw Call 顶点过多
  switch (pendingType) {
    case PendingOpType::Rect:
    case PendingOpType::Image:
    case PendingOpType::Atlas:
      return pendingRects.size() < RectDrawOp::MaxNumRects; // 限制矩形数量
    // ...
  }
  return true;
}
```

**`CompareBrush` 的巧妙之处：**
```cpp
bool OpsCompositor::CompareBrush(const Brush& a, const Brush& b) {
  // 忽略 Color 的差异！
  // 因为 Color 是作为顶点属性 (Attribute) 传递的，而不是 Uniform。
  // 这意味着不同颜色的矩形可以在同一个 Draw Call 中绘制。
  if (a.antiAlias != b.antiAlias || a.blendMode != b.blendMode) {
    return false;
  }
  // 比较 Shader, MaskFilter, ColorFilter 指针或内容
  // ...
  return true;
}
```

**`flushPendingOps` 流程：**
当 `canAppend` 返回 false 时调用。它将暂存的几何数据 (`pendingRects`) 打包成一个 `DrawOp`。

1.  **计算包围盒**：遍历所有 `pendingRects`，计算 `localBounds` 和 `deviceBounds`。
2.  **创建 VertexProvider**：将矩形数据转换为顶点数据提供者（如 `RectsVertexProvider`）。
3.  **创建 DrawOp**：根据类型创建 `RectDrawOp` 或 `AtlasTextOp`。
4.  **构建 Processor 树**：
    *   将 `Brush` 中的 `Shader` 转换为 `FragmentProcessor`。
    *   如果有 `ColorFilter` 或 `MaskFilter`，也转换为 Processor 并添加到 `DrawOp` 的 `colors` 或 `coverages` 列表。
    *   **关键点**：`DrawOp` 持有了所有生成 Shader 所需的逻辑节点。

### 1.2 内存管理核心：ResourceCache & ReturnQueue

TGFX 使用了一套独特的引用计数回收机制，避免了频繁的 GPU 资源分配。

#### 1.2.1 延迟回收机制 (ReturnQueue)

源码位置：`src/core/utils/ReturnQueue.cpp`

**设计核心**：`std::shared_ptr` 的自定义删除器 (Deleter)。

```cpp
// 创建资源时注册自定义删除器 NotifyReferenceReachedZero
std::shared_ptr<ReturnNode> ReturnQueue::makeShared(ReturnNode* node) {
  auto reference = std::shared_ptr<ReturnNode>(node, NotifyReferenceReachedZero);
  reference->unreferencedQueue = weakThis.lock();
  return reference;
}

// 当引用计数归零时触发
void ReturnQueue::NotifyReferenceReachedZero(ReturnNode* node) {
  // 不直接 delete，而是放入队列
  auto unreferencedQueue = std::move(node->unreferencedQueue);
  unreferencedQueue->queue.enqueue(node);
}
```

#### 1.2.2 资源复用逻辑 (ResourceCache)

源码位置：`src/gpu/ResourceCache.cpp`

**`processUnreferencedResources`**：
定期（如每一帧）从 `ReturnQueue` 取出“死亡”资源，根据是否有 Key 决定去留：
*   **有 Key (Scratch/Unique)**：放入 `purgeableResources` (LRU 列表)，等待复用。
*   **无 Key**：直接 `delete`。

**`findScratchResource`**：
当请求新资源（如临时纹理）时，优先从 LRU 列表中查找：
```cpp
std::shared_ptr<Resource> ResourceCache::findScratchResource(const ScratchKey& scratchKey) {
  // 查找匹配 Key 的列表
  auto result = scratchKeyMap.find(scratchKey);
  // ...
  // 从 LRU 列表中复活资源
  auto resource = list[index];
  return refResource(resource); // 重新创建 shared_ptr，引用计数 +1
}
```

### 1.3 纹理优化核心：AtlasManager & RectPackSkyline

#### 1.3.1 Skyline 装箱算法

源码位置：`src/core/RectPackSkyline.cpp`

该算法维护一条“天际线”（Skyline），即当前已占用区域的顶部轮廓。

**数据结构**：
```cpp
struct Node {
  int x, y, width; // 描述天际线的一个水平段
};
std::vector<Node> skyline;
```

**`addRect` 逻辑**：
1.  **查找最佳位置**：遍历所有 Skyline 节点，寻找能容纳新矩形且 `y` 值最小（最底部）的位置。如果 `y` 相同，选宽度最小的（Best Fit）。
2.  **更新天际线**：
    *   插入新节点（代表新矩形的顶部）。
    *   合并/修剪相邻节点：因为新矩形可能覆盖了后面几个节点的空间。
    *   这一步通过 `addSkylineLevel` 实现，涉及复杂的区间合并逻辑。

**设计优势**：相比二叉树装箱，Skyline 算法在处理高度不一的矩形（如不同字号的文字）时，能产生更少的碎片，且实现相对轻量。

## 2. 设计决策分析

### 2.1 为什么不使用预编译 Shader？

*   **现状**：TGFX 在运行时通过 `FragmentProcessor` 拼接 GLSL 字符串。
*   **原因**：
    *   **组合爆炸**：2D 渲染涉及 Shader (Gradient/Image) + ColorFilter + MaskFilter + Blending 的任意组合。预编译所有变体是不可能的。
    *   **平台差异**：不同平台（WebGL vs Desktop GL）的 GLSL 语法（如 precision, texture2D vs texture）有细微差异，动态生成更容易适配。
*   **对比**：
    *   **Unity/Unreal**：主要使用预编译 Shader Variant，但 2D UI 往往也需要特殊处理。
    *   **Skia**：同样采用动态生成 (SkSL)。

### 2.2 为什么引用计数归零不立即释放？

*   **现状**：放入 `ReturnQueue` 延迟处理。
*   **原因**：
    *   **线程安全**：资源可能在后台线程被释放，而 GL 上下文只能在主线程访问。延迟队列允许在主线程统一处理销毁。
    *   **复用机会**：一个纹理刚用完（计数归零），下一行代码可能又要申请同样大小的纹理。延迟释放提供了“复活”机会，避免昂贵的 `glGenTextures` / `glDeleteTextures`。

## 3. 调试和诊断

TGFX 内置了一个强大的 `Inspector` 模块，用于远程调试。

### 3.1 开启 Inspector

在 `CMakeLists.txt` 中：
```cmake
option(TGFX_USE_INSPECTOR "Enable profiling support..." ON)
```
或者 Debug 模式下默认开启。

### 3.2 使用 Inspector 宏

源码位置：`src/inspect/InspectorMark.h`

开发者可以在代码中插入宏来捕获数据：
*   **`FRAME_MARK`**：标记一帧的结束。
*   **`CAPUTRE_RENDER_TARGET(rt)`**：捕获当前渲染目标的快照。
*   **`CAPUTRE_SHAPE_MESH(...)`**：捕获矢量图形的网格数据。

### 3.3 性能分析点

1.  **Batching 效率**：在 `OpsCompositor::flushPendingOps` 打断点，观察 `drawOps.size()`。如果一帧内 DrawOp 过多，说明 Batching 失败。
    *   *常见原因*：频繁切换 Clip 或 BlendMode。
2.  **纹理上传**：关注 `TextureUploadTask`。频繁的纹理上传会阻塞 GPU。
3.  **资源泄露**：检查 `ResourceCache::totalBytes` 是否持续增长。

## 4. 兼容性考虑

### 4.1 平台差异处理

TGFX 通过 `Device` 和 `Window` 抽象屏蔽差异，但在 Shader 生成层也有特殊处理：

*   **WebGL**:
    *   不支持 `framebufferFetch`，需要通过 `DstTexture` 拷贝来实现高级混合。
    *   GLSL 版本差异：`GLSLFragmentShaderBuilder` 会根据 `ShaderCaps` 自动添加 `precision mediump float` 等修饰符。
*   **Android**:
    *   使用 `AHardwareBuffer` 实现纹理共享。
    *   处理 `EGL_ANDROID_presentation_time` 扩展。

### 4.2 向后兼容

*   **API 设计**：核心 API (`Canvas`, `Paint`) 保持稳定，新增功能通过扩展 `Shader` 或 `PathEffect` 实现。
*   **数据序列化**：`inspect` 模块中的序列化协议 (`Protocol.h`) 有版本控制，但主要用于调试，不作为持久化存储格式。

## 5. 安全性和稳定性

### 5.1 内存安全

*   **智能指针**：全面使用 `std::shared_ptr` 和 `std::unique_ptr`，几乎没有裸指针所有权传递。
*   **线程安全**：
    *   `ResourceCache` 的 `returnQueue` 是线程安全的（基于 `concurrentqueue`）。
    *   `GLDevice` 的 `lockContext()` 使用 `std::mutex` 保护 GL 上下文的并发访问。

### 5.2 错误恢复

*   **Context Loss**：虽然 TGFX 目前对 Context Loss 的处理主要依赖上层重建，但 `ResourceCache` 的设计允许资源在 Context 销毁时被清理 (`releaseAll`)，防止野指针。
*   **Shader 编译失败**：`GLSLProgramBuilder` 会捕获编译错误日志，并输出到控制台，通常会回退到空绘制或纯色绘制，避免 Crash。

## 6. 实战代码示例

### 场景一：自定义纹理绘制 (Texture)

```cpp
void DrawTexturedRect(tgfx::Canvas* canvas, const std::string& imagePath) {
    // 1. 解码图片
    auto image = tgfx::Image::MakeFromFile(imagePath);
    if (!image) return;

    // 2. 创建 Texture Shader
    // TileMode::Repeat 让纹理重复平铺
    auto shader = tgfx::Shader::MakeImageShader(image, tgfx::TileMode::Repeat, tgfx::TileMode::Repeat);
    
    tgfx::Paint paint;
    paint.setShader(shader);
    
    // 3. 绘制
    canvas->drawRect(tgfx::Rect::MakeXYWH(0, 0, 500, 500), paint);
}
```

### 场景二：高性能大量圆绘制 (Batching 验证)

```cpp
void DrawManyCircles(tgfx::Canvas* canvas) {
    tgfx::Paint paint;
    paint.setColor(tgfx::Color::Red());
    
    // 这些圆会被 OpsCompositor 合并为一个 DrawOp (RRectDrawOp)
    // 因为它们共享相同的 Paint 属性 (除了 Color，这里 Color 也一样)
    for (int i = 0; i < 1000; ++i) {
        float x = (i % 50) * 10.0f;
        float y = (i / 50) * 10.0f;
        canvas->drawCircle(x, y, 4.0f, paint);
    }
}
```

### 场景三：离屏渲染与混合 (SaveLayer)

```cpp
void DrawGroupOpacity(tgfx::Canvas* canvas) {
    // 1. 开启离屏层，设置整体透明度 0.5
    // 这会让后续的绘制先画到一个临时纹理，最后以 0.5 的 alpha 合成到画布
    canvas->saveLayerAlpha(0.5f);
    
    tgfx::Paint paint;
    paint.setColor(tgfx::Color::Blue());
    canvas->drawRect(tgfx::Rect::MakeXYWH(0, 0, 100, 100), paint);
    
    paint.setColor(tgfx::Color::Green());
    // 这里的重叠部分不会变深，因为它们是在离屏层先混合好的
    canvas->drawRect(tgfx::Rect::MakeXYWH(50, 50, 100, 100), paint);
    
    // 2. 恢复，将离屏层绘制回主画布
    canvas->restore();
}
```

### 场景四：动态修改 Path (PathEffect)

```cpp
void DrawDashedLine(tgfx::Canvas* canvas) {
    tgfx::Paint paint;
    paint.setStyle(tgfx::PaintStyle::Stroke);
    paint.setStrokeWidth(2.0f);
    paint.setColor(tgfx::Color::Black());
    
    // 创建虚线效果：10px 实线，5px 空白
    float intervals[] = {10.0f, 5.0f};
    auto effect = tgfx::PathEffect::MakeDash(intervals, 2, 0);
    // 注意：tgfx 目前核心库可能未直接暴露 PathEffect 给 Paint，
    // 通常通过 DashPathEffect::Make 直接处理 Path，或者扩展 Paint 接口。
    // 假设 tgfx 提供了 setPathEffect:
    // paint.setPathEffect(effect); 
    
    // 或者手动应用效果到 Path
    tgfx::Path path;
    path.moveTo(0, 0);
    path.lineTo(200, 0);
    // tgfx::DashPathEffect::FilterPath(&dst, path, ...); 
    
    canvas->drawPath(path, paint);
}
```

### 场景五：使用 AtlasManager (内部机制模拟)

```cpp
// 这是一个模拟内部调用的示例，展示 Atlas 如何工作
void SimulateAtlasUsage(tgfx::Context* context) {
    auto atlasManager = context->atlasManager();
    
    // 申请一个 32x32 的区域
    tgfx::Point location;
    auto atlas = atlasManager->getAtlas(tgfx::MaskFormat::A8);
    // 内部会调用 RectPackSkyline
    if (atlas->addToAtlas(32, 32, &location)) {
        // 成功分配，location.x/y 即为在图集纹理中的坐标
        // 接着可以将 Glyph 数据上传到这个位置
    }
}
```

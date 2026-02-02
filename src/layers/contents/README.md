# Layer Contents

本模块定义了图层的具体“内容”。在 TGFX 的架构中，`Layer` 类负责管理属性（变换、透明度等）和层级关系，而 `LayerContent` 负责具体的绘制逻辑和几何形状定义。这实现了属性管理与内容绘制的解耦。

## 核心接口

*   **LayerContent**: 所有内容的基类。定义了 `draw()`（绘制）和 `getBounds()`（获取边界）等核心接口。

## 主要实现类

*   **GeometryContent**: 几何形状内容的基类。
*   **PathContent**: 基于 `Path`（路径）的内容，支持任意矢量形状。
*   **RectContent / RectsContent**: 矩形和矩形集合内容，针对矩形绘制进行了优化。
*   **RRectContent / RRectsContent**: 圆角矩形及其集合的内容。
*   **TextContent**: 文本内容，负责文本的排版和绘制。
*   **ComposeContent**: 组合内容，允许将多个 `LayerContent` 组合成一个整体。
*   **ShapeContent**: 通用形状内容。

## 功能说明

每个 Content 类通常包含：
1.  **几何数据**：如 `Rect`, `Path`, `TextBlob` 等。
2.  **绘制逻辑**：调用 `Canvas` 的相关 API 将几何数据绘制出来。
3.  **命中测试**：`hitTestPoint()` 方法用于检测点击点是否在内容范围内。

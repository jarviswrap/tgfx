# Vectors

本模块实现了矢量图形系统，是 `VectorLayer` 的核心支撑。它定义了构成矢量图形的各种基本元素（形状、路径、样式、变换等），并支持这些元素的组合与嵌套。这套系统能够解析和渲染复杂的矢量数据（如来自 After Effects 的导出数据）。

## 核心接口

*   **VectorElement**: 所有矢量元素的基类。继承自 `LayerProperty`，支持属性动画。
*   **VectorGroup**: 容器元素，可以包含多个 `VectorElement`，支持层级嵌套。

## 元素分类

### 1. 几何形状 (Shapes)
定义矢量图形的几何轮廓：
*   **Rectangle**: 矩形。
*   **Ellipse**: 椭圆。
*   **Polystar**: 多边形或星形。
*   **ShapePath**: 自由路径（贝塞尔曲线）。

### 2. 样式 (Styles)
定义图形的填充和描边外观：
*   **SolidColor**: 纯色填充。
*   **Gradient**: 渐变填充（线性或径向）。
*   **FillStyle / StrokeStyle**: 填充和描边属性的具体实现。
*   **ImagePattern**: 图像纹理填充。

### 3. 路径操作 (Path Operations)
对路径进行动态修改或组合：
*   **MergePath**: 路径合并（并集、交集、差集等）。
*   **TrimPath**: 路径修剪（实现路径生长动画）。
*   **RoundCorner**: 圆角处理器。
*   **Repeater**: 中继器（复制形状并应用变换）。

### 4. 文本 (Text)
*   **Text**: 矢量文本元素。
*   **TextPath**: 沿路径排列的文本。
*   **TextModifier / TextSelector**: 文本动画修改器和选择器。

## 渲染流程

`VectorLayer` 会持有一个根 `VectorGroup`。渲染时，遍历 `VectorGroup` 及其子元素，根据几何形状生成 `Path` 对象，并应用 `Fill` 或 `Stroke` 样式，最终调用底层的 `Canvas` 接口进行绘制。

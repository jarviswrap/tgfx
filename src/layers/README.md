# Layers 模块

Layers 模块是 TGFX 的高层渲染引擎，负责构建、管理和渲染复杂的图层树结构。它基于 `tgfx/gpu` 提供的底层绘图能力，实现了一套面向对象的场景图（Scene Graph）系统，类似于 Core Animation 或 HTML DOM 渲染树。

## 功能定位

1.  **图层树管理**：支持图层的层级嵌套、父子关系维护。
2.  **属性系统**：提供位置、旋转、缩放、透明度、混合模式等基础属性的变换和动画支持。
3.  **渲染管线**：通过 `DisplayList` 管理渲染指令，支持局部刷新（Dirty Region）和瓦片缓存（Tile Cache）等优化策略。
4.  **特效支持**：内置丰富的图像滤镜（Filters）和图层样式（Layer Styles）。
5.  **矢量图形**：通过 `VectorLayer` 和 `VectorElement` 支持复杂的矢量图形绘制。
6.  **3D 合成**：支持基于 BSP 树的 3D 空间变换与正确遮挡关系的合成。

## 核心接口

### 基础类
*   **Layer**: 所有图层的基类，定义了图层的基本属性（Transform, Alpha, BlendMode, Visibility 等）和层级操作接口。
*   **LayerProperty**: 图层属性的基类，支持属性的动态更新和脏标记机制。
*   **DisplayList**: 渲染入口，负责管理图层树的绘制流程，优化渲染性能。

### 图层类型
*   **RootLayer**: 根图层，作为图层树的顶级容器。
*   **ImageLayer**: 用于显示位图图像。
*   **SolidLayer**: 用于显示纯色矩形。
*   **ShapeLayer / VectorLayer**: 用于显示矢量图形。
*   **TextLayer**: 用于显示文本内容。

### 渲染辅助
*   **LayerRecorder**: 用于构建和录制图层树的工具类。
*   **DrawArgs**: 传递渲染过程中的上下文信息（Canvas, Matrix, Alpha 等）。

## 目录结构

*   **compositing3d/**: 3D 合成相关的实现，处理 3D 变换后的深度排序和绘制。
*   **contents/**: 定义各种具体的图层内容绘制逻辑（如路径、矩形、文本内容）。
*   **filters/**: 图像滤镜效果的实现（如模糊、颜色矩阵）。
*   **layerstyles/**: 图层样式效果的实现（如投影、内阴影）。
*   **vectors/**: 矢量图形元素的实现（如形状、填充、描边）。

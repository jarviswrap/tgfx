# Layer Styles

本模块实现了图层样式（Layer Styles）。与滤镜（Filters）类似，图层样式也是对图层视觉效果的增强，但它们的实现机制和应用场景略有不同。图层样式通常模拟了设计软件（如 Photoshop, AE）中的图层样式概念。

## 核心接口

*   **LayerStyle**: 所有图层样式的基类。

## 内置样式

*   **BackgroundBlurStyle**: 背景模糊样式。模糊图层覆盖区域下方的背景内容（类似于 iOS 的毛玻璃效果）。注意这与 `BlurFilter` 不同，后者是模糊图层自身的内容。
*   **DropShadowStyle**: 投影样式。
*   **InnerShadowStyle**: 内阴影样式。

## 与 Filters 的区别

虽然 `DropShadow` 和 `InnerShadow` 在 `filters` 和 `layerstyles` 中都有出现，但设计上 `LayerStyle` 更侧重于与设计工具的对齐，且通常作为一组效果的集合应用。在 TGFX 的渲染流程中，`LayerStyle` 可能会在不同的渲染阶段介入（例如背景模糊需要在绘制图层之前处理背景）。

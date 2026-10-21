#pragma once
#include <cstdint>

namespace MOON {

	// 材质预览使用哪种网格显示材质。
	// 和 Moon 的 EPreviewMesh 对应：
	//   Sphere - 标准 UV 球，观察曲率与法线最直接
	//   Ball   - 带凹面和底座的 material ball，同时有高光曲面和较平的底面
	//   Cube   - 六个面独立的立方体，适合看 UV/平铺
	//   Plane  - 朝 +Z 的平面，适合平面材质
	enum class PreviewMesh : uint8_t {
		Sphere = 0,
		Ball,
		Cube,
		Plane,
	};

} // namespace MOON

#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace MOON {

	struct MaterialAsset {
		const char* id;
		const char* displayName;
		const uint8_t* data;
		size_t size;
		const char* matResource;
	};

	// 构建时由 CMake 的 matc + resgen 生成并嵌入的材质列表。
	const std::vector<MaterialAsset>& MaterialAssets();

	int DefaultMaterialIndex();

} // namespace MOON

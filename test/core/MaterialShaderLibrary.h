#pragma once
#include <QString>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace MOON {

	struct ShaderFeature {
		const char* name;
		const char* shortName;
		const char* description;
		uint8_t bit;
	};

	// 从应用内嵌的 material_preview.filamat 包中按 variant 提取 OpenGL GLSL。
	// 这样勾选不同 feature 时拿到的是 matc 实际编译出的源码，而不是手工拼接。
	class MaterialShaderLibrary {
	public:
		MaterialShaderLibrary(const uint8_t* packageData, size_t packageSize);
		~MaterialShaderLibrary();

		MaterialShaderLibrary(const MaterialShaderLibrary&) = delete;
		MaterialShaderLibrary& operator=(const MaterialShaderLibrary&) = delete;

		bool IsValid() const;

		QString VertexSource(int variantKey) const;
		QString FragmentSource(int variantKey) const;
		QString ComputeSource(int variantKey) const;
		bool HasVertex(int variantKey) const;
		bool HasFragment(int variantKey) const;
		bool HasCompute(int variantKey) const;

		std::vector<int> AvailableVariants() const;

		static const std::vector<ShaderFeature>& Features();
		static QString VariantName(int variantKey);
		static int DefaultVariant() { return 0x01; }

	private:
		struct Impl;
		std::unique_ptr<Impl> mImpl;
	};

} // namespace MOON

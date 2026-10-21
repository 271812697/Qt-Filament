#pragma once
#include <QString>

#include <cstdint>
#include <vector>

namespace MOON {

	enum class MaterialParameterKind {
		Float,
		Float2,
		Float3,
		Float4,
		Int,
		Bool,
		Sampler2D,
		Other
	};

	struct MaterialParameter {
		QString name;
		MaterialParameterKind kind = MaterialParameterKind::Other;
		uint32_t count = 1;
	};

} // namespace MOON

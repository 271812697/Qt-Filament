#pragma once

// The preview material's GLSL sources are generated from Filament's
// aiDefaultMat material package and stored as Qt resources:
//   :/shaders/DefaultMaterial.vert.glsl   (DIR vertex stage)
//   :/shaders/DefaultMaterial.frag.glsl   (DIR fragment stage)
//
// Regeneration:
//   matc -a opengl -p desktop -g -o aiDefaultMat.filamat aiDefaultMat.mat
//   matinfo -g 2 aiDefaultMat.filamat > DefaultMaterial.vert.glsl
//   matinfo -g 3 aiDefaultMat.filamat > DefaultMaterial.frag.glsl
//
// The preview scene uses one directional light + IBL, no shadows/fog/skinning,
// so the active surface variant is 0x01 (DIR). Compute stages are not generated
// for this surface material.

namespace MOON::ShaderSources {

inline constexpr const char* kDefaultMaterialName = "AssimpDefaultMat (aiDefaultMat)";
inline constexpr const char* kDefaultMaterialApi = "OpenGL 410 core / desktop";
inline constexpr const char* kDefaultMaterialVariant = "0x01 (DIR) - directional light";

inline constexpr const char* kDefaultMaterialVertexResource =
	":/shaders/DefaultMaterial.vert.glsl";
inline constexpr const char* kDefaultMaterialFragmentResource =
	":/shaders/DefaultMaterial.frag.glsl";

inline constexpr const char* kDefaultMaterialComputeNote =
	"// The preview material is a surface (lit) material.\n"
	"// matc generates only a vertex stage and a fragment stage for it.\n"
	"// There is no compute stage associated with this material;\n"
	"// Filament's compute work belongs to engine passes, not to the material.\n";

} // namespace MOON::ShaderSources

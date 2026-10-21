#pragma once

// The Shader panel reads the preview material's GLSL directly from the
// aiDefaultMat package embedded in the application. The variant checkboxes
// select the same filaflat chunks that the Filament renderer uses.
//
// The original .mat source consumed by matc is stored as a Qt resource:
//   :/shaders/DefaultMaterial.mat.txt
//
// Source material:
//   filament/samples/materials/aiDefaultMat.mat

namespace MOON::ShaderSources {

inline constexpr const char* kDefaultMaterialName = "AssimpDefaultMat (aiDefaultMat)";
inline constexpr const char* kDefaultMaterialApi = "OpenGL 410 core / desktop";
inline constexpr const char* kDefaultMaterialMatResource =
	":/shaders/DefaultMaterial.mat.txt";

inline constexpr const char* kDefaultMaterialComputeNote =
	"// The preview material is a surface (lit) material.\n"
	"// matc generates only a vertex stage and a fragment stage for it.\n"
	"// There is no compute stage associated with this material;\n"
	"// Filament's compute work belongs to engine passes, not to the material.\n";

} // namespace MOON::ShaderSources

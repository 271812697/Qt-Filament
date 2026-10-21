#pragma once

// The Shader panel reads the preview material's GLSL directly from the
// project-owned material package embedded in the application. The variant
// checkboxes select the same filaflat chunks that the Filament renderer uses.
//
// The original .mat source consumed by matc is stored as a Qt resource:
//   :/shaders/MaterialPreview.mat.txt
//
// Source material:
//   test/materials/material_preview.mat

namespace MOON::ShaderSources {

inline constexpr const char* kDefaultMaterialName = "MaterialPreview (material_preview.mat)";
inline constexpr const char* kDefaultMaterialApi = "OpenGL 410 core / desktop";
inline constexpr const char* kDefaultMaterialMatResource =
	":/shaders/MaterialPreview.mat.txt";

inline constexpr const char* kDefaultMaterialComputeNote =
	"// The preview material is a surface (lit) material.\n"
	"// matc generates only a vertex stage and a fragment stage for it.\n"
	"// There is no compute stage associated with this material;\n"
	"// Filament's compute work belongs to engine passes, not to the material.\n";

} // namespace MOON::ShaderSources

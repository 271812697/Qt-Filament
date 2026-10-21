#include "MaterialCatalog.h"

#include "MaterialPackage_material_preview.h"
#include "MaterialPackage_lit_opaque.h"
#include "MaterialPackage_cloth.h"
#include "MaterialPackage_unlit.h"
#include "MaterialPackage_subsurface.h"

namespace MOON {

	const std::vector<MaterialAsset>& MaterialAssets() {
		static const std::vector<MaterialAsset> assets = {
			{
				"material_preview",
				"Assimp Default PBR",
				MATERIALPACKAGE_MATERIAL_PREVIEW_MATERIAL_PREVIEW_DATA,
				MATERIALPACKAGE_MATERIAL_PREVIEW_MATERIAL_PREVIEW_SIZE,
				":/shaders/material_preview.mat.txt"
			},
			{
				"lit_opaque",
				"Lit Opaque",
				MATERIALPACKAGE_LIT_OPAQUE_LIT_OPAQUE_DATA,
				MATERIALPACKAGE_LIT_OPAQUE_LIT_OPAQUE_SIZE,
				":/shaders/lit_opaque.mat.txt"
			},
			{
				"cloth",
				"Cloth",
				MATERIALPACKAGE_CLOTH_CLOTH_DATA,
				MATERIALPACKAGE_CLOTH_CLOTH_SIZE,
				":/shaders/cloth.mat.txt"
			},
			{
				"unlit",
				"Unlit",
				MATERIALPACKAGE_UNLIT_UNLIT_DATA,
				MATERIALPACKAGE_UNLIT_UNLIT_SIZE,
				":/shaders/unlit.mat.txt"
			},
			{
				"subsurface",
				"Subsurface",
				MATERIALPACKAGE_SUBSURFACE_SUBSURFACE_DATA,
				MATERIALPACKAGE_SUBSURFACE_SUBSURFACE_SIZE,
				":/shaders/subsurface.mat.txt"
			},
		};
		return assets;
	}

	int DefaultMaterialIndex() {
		return 0;
	}

} // namespace MOON

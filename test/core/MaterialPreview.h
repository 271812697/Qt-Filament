#pragma once

#include "PreviewMesh.h"

#include <QColor>
#include <QString>

#include <math/vec3.h>
#include <math/vec4.h>
#include <utils/Entity.h>

#include <cstdint>
#include <vector>

namespace filament {
	class Engine;
	class IndexBuffer;
	class IndirectLight;
	class Material;
	class MaterialInstance;
	class Scene;
	class Skybox;
	class Texture;
	class VertexBuffer;
}

namespace MOON {

	// ----------------------------------------------------------------------------
	// 材质预览场景
	//
	// FilamentApp 持有 Engine / View / Camera / SwapChain；这个类只负责“画什么”：
	//   - 一个单位 UV 球（TANGENTS 用四元数编码，和 Filament lit 材质兼容）
	//   - 默认 PBR 材质（项目自带 material_preview.mat，构建时用 matc 编译并嵌入）
	//   - studio HDR 预滤波得到的 IBL（间接光 + 可选环境天空盒）
	//   - 一盏方向光，让金属/粗糙度在 IBL 之外还有明确的高光方向
	//
	// 参数修改全部通过 MaterialInstance 立即生效；UI 层不直接接触 Filament 对象。
	// ----------------------------------------------------------------------------
	class MaterialPreview {
	public:
		MaterialPreview() = default;
		~MaterialPreview() = default;
		MaterialPreview(const MaterialPreview&) = delete;
		MaterialPreview& operator=(const MaterialPreview&) = delete;

		bool initialize(filament::Engine* engine, filament::Scene* scene);

		bool isReady() const {
			return mEngine != nullptr && mMaterialInstance != nullptr
				&& static_cast<bool>(mEntity);
		}

		filament::MaterialInstance* materialInstance() const {
			return mMaterialInstance;
		}

		size_t triangleCount() const {
			return mIndexCount / 3u;
		}

		// ---- 预览网格（Sphere / Ball / Cube / Plane） ----
		PreviewMesh meshType() const { return mMeshType; }
		void setMeshType(PreviewMesh type);

		QString materialName() const;

		// ---- 默认 PBR 参数（baseColor 以 sRGB 保存，提交给 Filament 时做转换） ----
		QColor baseColor() const { return mBaseColor; }
		void setBaseColor(const QColor& color);
		float metallic() const { return mMetallic; }
		void setMetallic(float value);
		float roughness() const { return mRoughness; }
		void setRoughness(float value);
		float reflectance() const { return mReflectance; }
		float clearCoat() const { return mClearCoat; }
		float clearCoatRoughness() const { return mClearCoatRoughness; }
		float transmission() const { return mTransmission; }
		float ior() const { return mIor; }
		float thickness() const { return mThickness; }
		QString albedoMap() const { return mAlbedoMapPath; }
		QString metallicMap() const { return mMetallicMapPath; }
		QString roughnessMap() const { return mRoughnessMapPath; }
		QString aoMap() const { return mAoMapPath; }
		QString normalMap() const { return mNormalMapPath; }
		QString emissiveMap() const { return mEmissiveMapPath; }
		void setReflectance(float value);
		void setClearCoat(float value);
		void setClearCoatRoughness(float value);
		void setTransmission(float value);
		void setIor(float value);
		void setThickness(float value);
		void setAlbedoMap(const QString& path);
		void setMetallicMap(const QString& path);
		void setRoughnessMap(const QString& path);
		void setAoMap(const QString& path);
		void setNormalMap(const QString& path);
		void setEmissiveMap(const QString& path);
		void resetMaterial();

		// ---- 环境（IBL） ----
		float iblIntensity() const { return mIblIntensity; }
		void setIblIntensity(float value);
		float iblRotationDegrees() const { return mIblRotationDegrees; }
		void setIblRotationDegrees(float degrees);
		bool environmentVisible() const { return mEnvironmentVisible; }
		void setEnvironmentVisible(bool visible);

	private:
		bool createMaterial();
		bool rebuildMesh(PreviewMesh type);
		bool createRenderable(const std::vector<filament::math::float3>& positions,
			const std::vector<filament::math::float3>& normals,
			const std::vector<uint32_t>& indices);
		void destroyMesh();
		void createEnvironment();
		bool createLights();
		void applyMaterial();
		void updateIndirectLight();
		QString environmentPath() const;

		filament::Engine* mEngine = nullptr;
		filament::Scene* mScene = nullptr;

		filament::Material* mMaterial = nullptr;
		filament::MaterialInstance* mMaterialInstance = nullptr;

		filament::VertexBuffer* mVertexBuffer = nullptr;
		filament::IndexBuffer* mIndexBuffer = nullptr;
		utils::Entity mEntity;
		// 上传给 GPU 之前的 CPU 数据必须活到命令执行完；成员变量保证生命周期。
		std::vector<filament::math::float3> mPositions;
		std::vector<filament::math::float4> mTangents;
		std::vector<uint32_t> mIndices;
		uint32_t mIndexCount = 0;
		PreviewMesh mMeshType = PreviewMesh::Sphere;

		// IBL 资源由本类持有（IBLPrefilterContext 只负责生成）
		filament::Texture* mEnvironmentCubemap = nullptr;
		filament::Texture* mReflectionCubemap = nullptr;
		filament::IndirectLight* mIndirectLight = nullptr;
		filament::Skybox* mEnvironmentSkybox = nullptr;
		filament::Skybox* mColorSkybox = nullptr;
		utils::Entity mKeyLight;

		QColor mBaseColor = QColor(184, 184, 184);
		float mMetallic = 0.0f;
		float mRoughness = 0.40f;
		float mReflectance = 0.5f;
		float mClearCoat = 0.0f;
		float mClearCoatRoughness = 0.0f;
		float mTransmission = 0.0f;
		float mIor = 1.5f;
		float mThickness = 1.0f;
		QString mAlbedoMapPath, mMetallicMapPath, mRoughnessMapPath, mAoMapPath, mNormalMapPath, mEmissiveMapPath;
		filament::Texture* mAlbedoTexture = nullptr;
		filament::Texture* mMetallicTexture = nullptr;
		filament::Texture* mRoughnessTexture = nullptr;
		filament::Texture* mAoTexture = nullptr;
		filament::Texture* mNormalTexture = nullptr;
		filament::Texture* mEmissiveTexture = nullptr;
		filament::Texture* mWhiteTexture = nullptr;
		filament::Texture* mBlackTexture = nullptr;
		filament::Texture* mFlatNormalTexture = nullptr;

		float mIblIntensity = 20000.0f;
		float mIblRotationDegrees = 0.0f;
		bool mEnvironmentVisible = false;
	};

} // namespace MOON

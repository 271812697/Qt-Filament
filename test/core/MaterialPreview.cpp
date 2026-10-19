#include "MaterialPreview.h"

#include "log.h"

#include <filament/Color.h>
#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/IndirectLight.h>
#include <filament/LightManager.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/Texture.h>
#include <filament/VertexBuffer.h>

#include <filament-iblprefilter/IBLPrefilterContext.h>
#include <imageio/ImageDecoder.h>
#include <image/LinearImage.h>
#include <stb_image.h>

#include <math/mat3.h>
#include <math/scalar.h>
#include <utils/EntityManager.h>

#include "generated/resources/resources.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>

#ifndef QT_FILAMENT_SOURCE_DIR
#define QT_FILAMENT_SOURCE_DIR ""
#endif

namespace MOON {
	namespace {

		constexpr float kPi = 3.14159265358979f;
		constexpr float kDegToRad = kPi / 180.0f;

		// 把单位法线编码成 Filament 的 TANGENTS 四元数（把 (0,0,1) 旋转到 n 的最短弧）。
		// 和 FilamentApp 里加载 SaveDomains 网格时用的是同一套约定。
		filament::math::float4 MakeTangentFrame(const filament::math::float3& n) {
			const float denom = std::sqrt(2.0f * (1.0f + n.z));
			if (denom > 1e-4f) {
				return { -n.y / denom, n.x / denom, 0.0f, denom * 0.5f };
			}
			return { 1.0f, 0.0f, 0.0f, 0.0f }; // n ≈ (0,0,-1)
		}

		// 程序化 studio 环境：文件缺失时的兜底 IBL。
		// 顶部偏冷、地面偏暗，再叠两个柔和的“灯箱”，保证金属球有可读的反射。
		void MakeFallbackEnvironment(uint32_t width, uint32_t height,
			std::vector<float>& pixels) {
			using namespace filament::math;
			pixels.resize((size_t)width * height * 3u);

			const float3 keyDir = normalize(float3{ 0.35f, 0.55f, 0.76f });
			const float3 fillDir = normalize(float3{ -0.75f, 0.15f, 0.25f });
			const float3 rimDir = normalize(float3{ 0.15f, 0.35f, -0.9f });

			auto softbox = [](const float3& dir,
				const float3& lightDir, float power, float falloff) {
					const float cosine = std::max(0.0f, dot(dir, lightDir));
					return power * std::pow(cosine, falloff);
				};

			for (uint32_t y = 0; y < height; ++y) {
				const float theta = kPi * (y + 0.5f) / (float)height;
				const float sinTheta = std::sin(theta);
				const float cosTheta = std::cos(theta);
				for (uint32_t x = 0; x < width; ++x) {
					const float phi = 2.0f * kPi * (x + 0.5f) / (float)width - kPi;
					const filament::math::float3 dir{
						sinTheta * std::sin(phi),
						cosTheta,
						sinTheta * std::cos(phi)
					};

					const float sky = std::pow(std::max(0.0f, dir.y * 0.5f + 0.5f), 1.6f);
					filament::math::float3 color{
						0.035f + 0.42f * sky,
						0.040f + 0.46f * sky,
						0.050f + 0.55f * sky
					};
					color += softbox(dir, keyDir, 18.0f, 18.0f)
						* filament::math::float3{ 1.0f, 0.96f, 0.90f };
					color += softbox(dir, fillDir, 5.0f, 12.0f)
						* filament::math::float3{ 0.75f, 0.86f, 1.0f };
					color += softbox(dir, rimDir, 9.0f, 24.0f)
						* filament::math::float3{ 1.0f, 0.92f, 0.82f };

					const size_t index = ((size_t)y * width + x) * 3u;
					pixels[index + 0] = color.x;
					pixels[index + 1] = color.y;
					pixels[index + 2] = color.z;
				}
			}
		}

		// 把任意通道数的 LinearImage 打包成 RGB float 缓冲（IBL 预滤波只接受 RGB）。
		bool PackEnvironmentImage(const image::LinearImage& image,
			std::vector<float>& pixels) {
			const uint32_t width = image.getWidth();
			const uint32_t height = image.getHeight();
			const uint32_t channels = image.getChannels();
			if (width == 0 || height == 0 || width != height * 2u ||
				channels < 3u || channels > 4u) {
				return false;
			}

			pixels.resize((size_t)width * height * 3u);
			for (uint32_t y = 0; y < height; ++y) {
				const float* row = static_cast<const float*>(image.getPixelRef(0, y));
				float* dst = pixels.data() + (size_t)y * width * 3u;
				for (uint32_t x = 0; x < width; ++x) {
					dst[x * 3u + 0u] = row[x * channels + 0u];
					dst[x * 3u + 1u] = row[x * channels + 1u];
					dst[x * 3u + 2u] = row[x * channels + 2u];
				}
			}
			return true;
		}

		bool LoadEquirectFromFile(const QString& path,
			std::vector<float>& pixels) {
			if (path.isEmpty()) {
				return false;
			}

			// 使用 QByteArray 而不是 std::string：Qt 与应用程序的 CRT 不同，
			// 跨 DLL 传递/释放 std::string 在 Debug 下会破坏栈。
			const QByteArray nativePath = QDir::toNativeSeparators(path).toLocal8Bit();
			CORE_INFO("[Env] loading {}", nativePath.constData());

			// Radiance HDR 走 stb：和 Filament 的 IBL 样例一致。
			if (path.endsWith(QStringLiteral(".hdr"), Qt::CaseInsensitive)) {
				int width = 0, height = 0, channels = 0;
				float* data = stbi_loadf(nativePath.constData(), &width, &height, &channels, 3);
				if (!data) {
					return false;
				}
				const bool valid = width > 0 && height > 0 && width == height * 2;
				if (valid) {
					pixels.assign(data, data + (size_t)width * height * 3u);
				}
				stbi_image_free(data);
				return valid;
			}

			std::ifstream stream(nativePath.constData(), std::ios::binary);
			if (!stream.is_open()) {
				return false;
			}

			image::LinearImage image;
			try {
				image = image::ImageDecoder::decode(stream, nativePath.constData());
			}
			catch (...) {
				return false;
			}
			if (!image.isValid()) {
				return false;
			}
			return PackEnvironmentImage(image, pixels);
		}

	} // namespace

	QString MaterialPreview::materialName() const {
		return QStringLiteral("Default PBR (aiDefaultMat)");
	}

	bool MaterialPreview::initialize(filament::Engine* engine, filament::Scene* scene) {
		if (!engine || !scene) {
			return false;
		}
		if (mEngine) {
			return isReady();
		}

		mEngine = engine;
		mScene = scene;

		if (!createMaterial()) {
			CORE_ERROR("[Preview] 默认 PBR 材质创建失败");
			return false;
		}
		if (!createSphere()) {
			CORE_ERROR("[Preview] 材质球网格创建失败");
			return false;
		}

		createEnvironment();
		createLights();

		CORE_INFO("[Preview] 材质预览场景就绪: material='{}', triangles={}, IBL={}",
			materialName().toLocal8Bit().constData(), triangleCount(),
			mIndirectLight ? "on" : "off");
		return isReady();
	}

	bool MaterialPreview::createMaterial() {
		mMaterial = filament::Material::Builder()
			.package(RESOURCES_AIDEFAULTMAT_DATA, RESOURCES_AIDEFAULTMAT_SIZE)
			.build(*mEngine);
		if (!mMaterial) {
			return false;
		}

		mMaterialInstance = mMaterial->createInstance();
		if (!mMaterialInstance) {
			return false;
		}

		applyMaterial();
		return true;
	}

	bool MaterialPreview::createSphere() {
		// 经纬球：128 条经线、64 条纬线，法线/切线由解析式给出，轮廓足够平滑。
		constexpr uint32_t kSlices = 128; // 经度
		constexpr uint32_t kStacks = 64;  // 纬度
		const float radius = 1.0f;

		const uint32_t vertexCount = (kStacks + 1u) * (kSlices + 1u);
		mPositions.resize(vertexCount);
		mTangents.resize(vertexCount);

		for (uint32_t stack = 0; stack <= kStacks; ++stack) {
			const float v = (float)stack / (float)kStacks;
			const float theta = v * kPi;
			const float sinTheta = std::sin(theta);
			const float cosTheta = std::cos(theta);

			for (uint32_t slice = 0; slice <= kSlices; ++slice) {
				const float u = (float)slice / (float)kSlices;
				const float phi = u * 2.0f * kPi;
				const filament::math::float3 normal{
					sinTheta * std::cos(phi),
					cosTheta,
					sinTheta * std::sin(phi)
				};
				const size_t index = (size_t)stack * (kSlices + 1u) + slice;
				mPositions[index] = normal * radius;
				mTangents[index] = MakeTangentFrame(normal);
			}
		}

		mIndices.clear();
		mIndices.reserve((size_t)kStacks * kSlices * 6u);
		for (uint32_t stack = 0; stack < kStacks; ++stack) {
			for (uint32_t slice = 0; slice < kSlices; ++slice) {
				const uint32_t i0 = stack * (kSlices + 1u) + slice;
				const uint32_t i1 = i0 + 1u;
				const uint32_t i2 = i0 + (kSlices + 1u);
				const uint32_t i3 = i2 + 1u;
				// 参数化 (theta, phi) 的叉积朝内，因此按 (a, b, c) / (b, d, c) 绕序得到朝外的面。
				mIndices.push_back(i0);
				mIndices.push_back(i1);
				mIndices.push_back(i2);
				mIndices.push_back(i1);
				mIndices.push_back(i3);
				mIndices.push_back(i2);
			}
		}
		mIndexCount = (uint32_t)mIndices.size();

		mVertexBuffer = filament::VertexBuffer::Builder()
			.vertexCount((uint32_t)mPositions.size())
			.bufferCount(2)
			.attribute(filament::VertexAttribute::POSITION, 0,
				filament::VertexBuffer::AttributeType::FLOAT3, 0, sizeof(float) * 3)
			.attribute(filament::VertexAttribute::TANGENTS, 1,
				filament::VertexBuffer::AttributeType::FLOAT4, 0, sizeof(float) * 4)
			.build(*mEngine);
		if (!mVertexBuffer) {
			return false;
		}
		mVertexBuffer->setBufferAt(*mEngine, 0, filament::VertexBuffer::BufferDescriptor(
			mPositions.data(), mPositions.size() * sizeof(filament::math::float3), nullptr));
		mVertexBuffer->setBufferAt(*mEngine, 1, filament::VertexBuffer::BufferDescriptor(
			mTangents.data(), mTangents.size() * sizeof(filament::math::float4), nullptr));

		mIndexBuffer = filament::IndexBuffer::Builder()
			.indexCount(mIndexCount)
			.bufferType(filament::IndexBuffer::IndexType::UINT)
			.build(*mEngine);
		if (!mIndexBuffer) {
			return false;
		}
		mIndexBuffer->setBuffer(*mEngine, filament::IndexBuffer::BufferDescriptor(
			mIndices.data(), mIndices.size() * sizeof(uint32_t), nullptr));

		mEntity = utils::EntityManager::get().create();
		filament::RenderableManager::Builder(1)
			.boundingBox({ { 0.0f, 0.0f, 0.0f }, { 1.05f, 1.05f, 1.05f } })
			.material(0, mMaterialInstance)
			.geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES,
				mVertexBuffer, mIndexBuffer, 0, mIndexCount)
			.culling(true)
			.receiveShadows(false)
			.castShadows(false)
			.build(*mEngine, mEntity);

		mScene->addEntity(mEntity);
		return true;
	}

	void MaterialPreview::createEnvironment() {
		const QString path = environmentPath();
		std::vector<float> pixels;
		bool loaded = false;
		if (!path.isEmpty()) {
			loaded = LoadEquirectFromFile(path, pixels);
			if (loaded) {
				CORE_INFO("[Preview] 已加载 IBL 环境: {}", path.toLocal8Bit().constData());
			}
			else {
				CORE_WARN("[Preview] IBL 环境解码失败: {}", path.toLocal8Bit().constData());
			}
		}

		uint32_t width = 0;
		uint32_t height = 0;
		if (loaded) {
			// 2:1 等距柱状图，由 PackEnvironmentImage 保证。
			height = (uint32_t)std::lround(std::sqrt((double)pixels.size() / 6.0));
			width = height * 2u;
		}
		else {
			width = 512;
			height = 256;
			MakeFallbackEnvironment(width, height, pixels);
			CORE_WARN("[Preview] 未找到 studio HDR，使用程序化环境贴图");
		}

		if (pixels.empty()) {
			return;
		}

		auto* ownedPixels = new std::vector<float>(std::move(pixels));
		filament::Texture::PixelBufferDescriptor buffer(
			ownedPixels->data(),
			ownedPixels->size() * sizeof(float),
			filament::Texture::Format::RGB,
			filament::Texture::Type::FLOAT,
			[](void*, size_t, void* user) {
				delete reinterpret_cast<std::vector<float>*>(user);
			},
			ownedPixels);

		filament::Texture* equirect = filament::Texture::Builder()
			.width(width)
			.height(height)
			.levels(0xff)
			.format(filament::Texture::InternalFormat::R11F_G11F_B10F)
			.sampler(filament::Texture::Sampler::SAMPLER_2D)
			.usage(filament::Texture::Usage::DEFAULT | filament::Texture::Usage::GEN_MIPMAPPABLE)
			.build(*mEngine);
		if (!equirect) {
			delete ownedPixels;
			return;
		}
		equirect->setImage(*mEngine, 0, std::move(buffer));

		// GPU 预滤波：等距柱状图 → cubemap → specular mip 链
		IBLPrefilterContext context(*mEngine);
		IBLPrefilterContext::EquirectangularToCubemap toCubemap(context);
		IBLPrefilterContext::SpecularFilter specularFilter(context);

		mEnvironmentCubemap = toCubemap(equirect);
		mEngine->destroy(equirect);
		if (mEnvironmentCubemap) {
			mReflectionCubemap = specularFilter(mEnvironmentCubemap);
		}

		if (mReflectionCubemap) {
			mIndirectLight = filament::IndirectLight::Builder()
				.reflections(mReflectionCubemap)
				.intensity(mIblIntensity)
				.build(*mEngine);
			if (mIndirectLight) {
				mScene->setIndirectLight(mIndirectLight);
				updateIndirectLight();
			}
		}

		if (mEnvironmentCubemap) {
			mEnvironmentSkybox = filament::Skybox::Builder()
				.environment(mEnvironmentCubemap)
				.showSun(false)
				.build(*mEngine);
		}

		mColorSkybox = filament::Skybox::Builder()
			.color({ 0.055f, 0.060f, 0.070f, 1.0f })
			.build(*mEngine);
		if (mColorSkybox) {
			mScene->setSkybox(mColorSkybox);
		}
		else if (mEnvironmentSkybox) {
			mEnvironmentVisible = true;
			mScene->setSkybox(mEnvironmentSkybox);
		}
	}

	bool MaterialPreview::createLights() {
		auto& entityManager = utils::EntityManager::get();
		mKeyLight = entityManager.create();

		filament::LightManager::Builder(filament::LightManager::Type::SUN)
			.color(filament::Color::toLinear<filament::ACCURATE>(
				filament::sRGBColor(1.0f, 0.96f, 0.90f)))
			.intensity(40000.0f)
			// 相机在 +X/+Z 一侧，光从上前方打到球正面。
			.direction({ -0.35f, -0.85f, -0.55f })
			.sunAngularRadius(1.5f)
			.castShadows(false)
			.build(*mEngine, mKeyLight);

		mScene->addEntity(mKeyLight);
		return true;
	}

	void MaterialPreview::applyMaterial() {
		if (!mMaterialInstance) {
			return;
		}
		mMaterialInstance->setParameter("baseColor", filament::RgbType::sRGB,
			filament::math::float3{
				(float)mBaseColor.redF(),
				(float)mBaseColor.greenF(),
				(float)mBaseColor.blueF() });
		mMaterialInstance->setParameter("metallic", mMetallic);
		mMaterialInstance->setParameter("roughness", mRoughness);
		mMaterialInstance->setParameter("reflectance", mReflectance);
	}

	void MaterialPreview::resetMaterial() {
		mBaseColor = QColor(184, 184, 184);
		mMetallic = 0.0f;
		mRoughness = 0.40f;
		mReflectance = 0.5f;
		applyMaterial();
	}

	void MaterialPreview::setBaseColor(const QColor& color) {
		if (!color.isValid()) {
			return;
		}
		mBaseColor = color;
		applyMaterial();
	}

	void MaterialPreview::setMetallic(float value) {
		mMetallic = std::clamp(value, 0.0f, 1.0f);
		if (mMaterialInstance) {
			mMaterialInstance->setParameter("metallic", mMetallic);
		}
	}

	void MaterialPreview::setRoughness(float value) {
		mRoughness = std::clamp(value, 0.0f, 1.0f);
		if (mMaterialInstance) {
			mMaterialInstance->setParameter("roughness", mRoughness);
		}
	}

	void MaterialPreview::setReflectance(float value) {
		mReflectance = std::clamp(value, 0.0f, 1.0f);
		if (mMaterialInstance) {
			mMaterialInstance->setParameter("reflectance", mReflectance);
		}
	}

	void MaterialPreview::setIblIntensity(float value) {
		mIblIntensity = std::clamp(value, 0.0f, 200000.0f);
		updateIndirectLight();
	}

	void MaterialPreview::setIblRotationDegrees(float degrees) {
		mIblRotationDegrees = std::fmod(degrees, 360.0f);
		if (mIblRotationDegrees < 0.0f) {
			mIblRotationDegrees += 360.0f;
		}
		updateIndirectLight();
	}

	void MaterialPreview::setEnvironmentVisible(bool visible) {
		mEnvironmentVisible = visible && mEnvironmentSkybox != nullptr;
		if (mScene) {
			mScene->setSkybox(mEnvironmentVisible ? mEnvironmentSkybox : mColorSkybox);
		}
	}

	void MaterialPreview::updateIndirectLight() {
		if (!mIndirectLight) {
			return;
		}
		mIndirectLight->setIntensity(mIblIntensity);
		const filament::math::mat3f rotation = filament::math::mat3f::rotation(
			mIblRotationDegrees * kDegToRad,
			filament::math::float3{ 0.0f, 1.0f, 0.0f });
		mIndirectLight->setRotation(rotation);
	}

	QString MaterialPreview::environmentPath() const {
		// 优先使用随仓库带的高质量 studio HDR；路径通过 CMake 的
		// QT_FILAMENT_SOURCE_DIR 注入，避免依赖启动时的工作目录。
		const QString sourceDir = QStringLiteral(QT_FILAMENT_SOURCE_DIR);
		QStringList candidates;
		if (!sourceDir.isEmpty()) {
			candidates
				<< sourceDir + "/filament/third_party/environments/studio_small_02_2k.hdr"
				<< sourceDir + "/filament/third_party/environments/lightroom_14b.hdr"
				<< sourceDir + "/filament/assets/environments/white_furnace/white_furnace.exr";
		}
		candidates
			<< QStringLiteral("filament/third_party/environments/studio_small_02_2k.hdr")
			<< QStringLiteral("filament/third_party/environments/lightroom_14b.hdr");

		// 从 exe 目录逐级往上找，覆盖从 build/test/Debug 直接启动的情况。
		QDir directory(QCoreApplication::applicationDirPath());
		for (int level = 0; level < 6; ++level) {
			candidates << directory.filePath(
				"filament/third_party/environments/studio_small_02_2k.hdr");
			candidates << directory.filePath(
				"filament/third_party/environments/lightroom_14b.hdr");
			if (!directory.cdUp()) {
				break;
			}
		}

		for (const QString& candidate : candidates) {
			if (QFileInfo::exists(candidate)) {
				CORE_INFO("[Env] found {}", candidate.toLocal8Bit().constData());
				return candidate;
			}
		}
		CORE_WARN("[Env] no environment file found");
		return {};
	}

} // namespace MOON

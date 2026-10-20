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
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
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

		using filament::math::float3;

		// --------------------------------------------------------------------
		// 预览网格生成
		// 所有网格都按“最大尺寸 2、中心在原点”归一化，这样相机和灯光
		// 不需要因为切换网格而重新调整。
		// --------------------------------------------------------------------

		const char* PreviewMeshName(PreviewMesh mesh) {
			switch (mesh) {
			case PreviewMesh::Ball: return "Ball";
			case PreviewMesh::Cube: return "Cube";
			case PreviewMesh::Plane: return "Plane";
			case PreviewMesh::Sphere:
			default: return "Sphere";
			}
		}

		// 标准 UV 球：法线就是单位位置，按纬度/经度生成。
		void BuildUVSphere(std::vector<float3>& positions,
			std::vector<float3>& normals, std::vector<uint32_t>& indices) {
			constexpr uint32_t slices = 128;
			constexpr uint32_t stacks = 64;
			constexpr float radius = 1.0f;
			const uint32_t stride = slices + 1;

			positions.reserve((size_t)(stacks + 1) * stride);
			normals.reserve((size_t)(stacks + 1) * stride);
			indices.reserve((size_t)stacks * slices * 6u);

			for (uint32_t stack = 0; stack <= stacks; ++stack) {
				const float v = (float)stack / (float)stacks;
				const float theta = v * kPi;
				const float sinTheta = std::sin(theta);
				const float cosTheta = std::cos(theta);

				for (uint32_t slice = 0; slice <= slices; ++slice) {
					const float u = (float)slice / (float)slices;
					const float phi = u * 2.0f * kPi;
					const float3 normal{
						sinTheta * std::cos(phi),
						cosTheta,
						sinTheta * std::sin(phi)
					};
					positions.push_back(normal * radius);
					normals.push_back(normal);
				}
			}

			for (uint32_t stack = 0; stack < stacks; ++stack) {
				for (uint32_t slice = 0; slice < slices; ++slice) {
					const uint32_t i0 = stack * stride + slice;
					const uint32_t i1 = i0 + 1u;
					const uint32_t i2 = i0 + stride;
					const uint32_t i3 = i2 + 1u;
					// CCW seen from outside（Filament 默认背面剔除）
					indices.insert(indices.end(), { i0, i1, i2, i1, i3, i2 });
				}
			}
		}

		// 立方体：每个面独立法线，UV 0..1 每面；适合检查平铺/朝向。
		void BuildCube(std::vector<float3>& positions,
			std::vector<float3>& normals, std::vector<uint32_t>& indices) {
			struct Face {
				float3 normal;
				float3 tangent;
				float3 bitangent;
			};

			// tangent x bitangent == normal，下面反过来的三角形绕序才是朝外的。
			const Face faces[6] = {
				{ { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } },
				{ { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
				{ { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
				{ { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
				{ { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
				{ { 0.0f, 0.0f, -1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
			};

			for (const Face& face : faces) {
				const uint32_t base = (uint32_t)positions.size();
				for (int corner = 0; corner < 4; ++corner) {
					const float u = (corner == 1 || corner == 2) ? 1.0f : 0.0f;
					const float v = (corner >= 2) ? 1.0f : 0.0f;
					const float3 position = face.normal
						+ face.tangent * (u * 2.0f - 1.0f)
						+ face.bitangent * (v * 2.0f - 1.0f);
					positions.push_back(position);
					normals.push_back(face.normal);
				}
				indices.insert(indices.end(), {
					base, base + 2, base + 1,
					base, base + 3, base + 2 });
			}
		}

		// 平面：XY 上的 1x1 方片，面向 +Z（默认相机一侧）。
		void BuildPlane(std::vector<float3>& positions,
			std::vector<float3>& normals, std::vector<uint32_t>& indices) {
			positions = {
				{ -1.0f, -1.0f, 0.0f },
				{ 1.0f, -1.0f, 0.0f },
				{ 1.0f, 1.0f, 0.0f },
				{ -1.0f, 1.0f, 0.0f },
			};
			normals.assign(4, float3{ 0.0f, 0.0f, 1.0f });
			indices = { 0, 1, 2, 0, 2, 3 };
		}

		// Filament/Moon 的 material ball：带凹面的球 + 小底座。
		// 凹面是球半径的径向调制，法线用解析法线求，边缘才会像一圈凸起。
		void BuildMaterialBall(std::vector<float3>& positions,
			std::vector<float3>& normals, std::vector<uint32_t>& indices) {
			constexpr float pi = 3.14159265358979323846f;
			constexpr uint32_t rings = 32;
			constexpr uint32_t segments = 64;
			constexpr float dentAngle = 0.55f;  // 凹面角半径（约 31.5°）
			constexpr float dentDepth = 0.24f;
			constexpr float centerY = 0.1f;

			// 统一按“几何法线朝外”来修正绕序，避免底座/极点的镜像三角形。
			const auto addTriangle = [&](uint32_t a, uint32_t b, uint32_t c) {
				const float3 positionA = positions[a];
				const float3 positionB = positions[b];
				const float3 positionC = positions[c];
				const float3 geometric = cross(positionB - positionA, positionC - positionA);
				if (dot(geometric, geometric) < 1.0e-12f) {
					return;
				}
				const float3 averageNormal = normalize(
					normals[a] + normals[b] + normals[c]);
				if (dot(geometric, averageNormal) < 0.0f) {
					std::swap(b, c);
				}
				indices.insert(indices.end(), { a, b, c });
			};

			const uint32_t stride = segments + 1;
			for (uint32_t ring = 0; ring <= rings; ++ring) {
				const float v = (float)ring / (float)rings;
				const float phi = v * pi;
				const float sinPhi = std::sin(phi);
				const float cosPhi = std::cos(phi);

				for (uint32_t segment = 0; segment <= segments; ++segment) {
					const float u = (float)segment / (float)segments;
					const float theta = u * 2.0f * pi;
					const float sinTheta = std::sin(theta);
					const float cosTheta = std::cos(theta);

					const float3 radial{ sinPhi * cosTheta, cosPhi, sinPhi * sinTheta };
					const float alpha = std::acos(std::clamp(radial.z, -1.0f, 1.0f));

					float radius = 1.0f;
					float slope = 0.0f; // dR/dalpha
					if (alpha < dentAngle) {
						const float t = alpha / dentAngle;
						radius -= dentDepth * (0.5f + 0.5f * std::cos(pi * t));
						slope = dentDepth * (0.5f * pi / dentAngle) * std::sin(pi * t);
					}

					// 径向调制曲面的解析法线：R * e - R' * de/dalpha。
					const float3 increasingAlpha{
						cosPhi * cosTheta,
						-sinPhi,
						cosPhi * sinTheta
					};
					const float3 normal = alpha < 1.0e-4f
						? radial
						: normalize(radial * radius - increasingAlpha * slope);

					positions.push_back(radial * radius + float3{ 0.0f, centerY, 0.0f });
					normals.push_back(normal);
				}
			}

			for (uint32_t ring = 0; ring < rings; ++ring) {
				for (uint32_t segment = 0; segment < segments; ++segment) {
					const uint32_t a = ring * stride + segment;
					const uint32_t b = a + stride;
					addTriangle(a, a + 1, b);
					addTriangle(a + 1, b + 1, b);
				}
			}

			// 底座：绕 Y 轴旋转的 2D 轮廓，上表面接球体底部。
			struct ProfilePoint {
				float radius;
				float height;
			};
			static constexpr ProfilePoint profile[] = {
				{ 0.00f, -0.90f }, { 0.40f, -0.90f }, { 0.44f, -0.93f }, { 0.44f, -1.00f },
				{ 0.56f, -1.10f }, { 0.60f, -1.14f }, { 0.54f, -1.16f }, { 0.00f, -1.16f }
			};
			constexpr uint32_t baseRows = sizeof(profile) / sizeof(profile[0]);
			constexpr uint32_t baseSegments = 64;
			constexpr uint32_t baseStride = baseSegments + 1;
			const uint32_t baseVertexStart = (uint32_t)positions.size();

			for (uint32_t row = 0; row < baseRows; ++row) {
				const ProfilePoint& current = profile[row];
				const ProfilePoint& previous = profile[row > 0 ? row - 1 : row];
				const ProfilePoint& next = profile[row + 1 < baseRows ? row + 1 : row];

				float nr = -(next.height - previous.height);
				float ny = next.radius - previous.radius;
				const float length = std::sqrt(nr * nr + ny * ny);
				if (length > 1.0e-6f) {
					nr /= length;
					ny /= length;
				}
				else {
					nr = 0.0f;
					ny = 1.0f;
				}

				for (uint32_t segment = 0; segment <= baseSegments; ++segment) {
					const float u = (float)segment / (float)baseSegments;
					const float theta = u * 2.0f * pi;
					const float sinTheta = std::sin(theta);
					const float cosTheta = std::cos(theta);

					positions.push_back(float3{
						current.radius * cosTheta,
						current.height,
						current.radius * sinTheta });
					normals.push_back(normalize(float3{
						nr * cosTheta,
						ny,
						nr * sinTheta }));
				}
			}

			for (uint32_t row = 0; row + 1 < baseRows; ++row) {
				for (uint32_t segment = 0; segment < baseSegments; ++segment) {
					const uint32_t a = baseVertexStart + row * baseStride + segment;
					const uint32_t b = a + baseStride;
					addTriangle(a, b, a + 1);
					addTriangle(a + 1, b, b + 1);
				}
			}
		}

		// 在源码目录 / 当前目录 / exe 上级目录中查找仓库自带资源。
		QString ResolveRepoAsset(const QString& relative) {
			const QString sourceDir = QStringLiteral(QT_FILAMENT_SOURCE_DIR);
			QStringList candidates;
			if (!sourceDir.isEmpty()) {
				candidates << sourceDir + "/" + relative;
			}
			candidates << relative;

			QDir directory(QCoreApplication::applicationDirPath());
			for (int level = 0; level < 6; ++level) {
				candidates << directory.filePath(relative);
				if (!directory.cdUp()) {
					break;
				}
			}

			for (const QString& candidate : candidates) {
				if (QFileInfo::exists(candidate)) {
					return candidate;
				}
			}
			return {};
		}

		// 读取 OBJ 的 position + normal + triangle indices。
		// 预览网格只关心形状和法线，UV/材质组都忽略。
		bool LoadObjMesh(const QString& path,
			std::vector<float3>& positions,
			std::vector<float3>& normals,
			std::vector<uint32_t>& indices) {
			using namespace filament::math;

			const QByteArray nativePath = QDir::toNativeSeparators(path).toLocal8Bit();
			std::ifstream stream(nativePath.constData());
			if (!stream.is_open()) {
				return false;
			}

			std::vector<float3> objPositions;
			std::vector<float3> objNormals;
			std::unordered_map<uint64_t, uint32_t> vertexMap;
			std::string line;

			while (std::getline(stream, line)) {
				if (line.size() < 2) {
					continue;
				}

				if (line[0] == 'v' && line[1] == ' ') {
					std::istringstream values(line.substr(1));
					float x = 0.0f, y = 0.0f, z = 0.0f;
					if (values >> x >> y >> z) {
						objPositions.push_back(float3{ x, y, z });
					}
				}
				else if (line[0] == 'v' && line[1] == 'n' &&
					line.size() > 2 && line[2] == ' ') {
					std::istringstream values(line.substr(2));
					float x = 0.0f, y = 0.0f, z = 0.0f;
					if (values >> x >> y >> z) {
						objNormals.push_back(float3{ x, y, z });
					}
				}
				else if (line[0] == 'f' && line[1] == ' ') {
					std::istringstream values(line.substr(1));
					std::vector<uint32_t> face;
					std::string token;
					bool valid = true;

					while (values >> token) {
						const size_t firstSlash = token.find('/');
						const int positionIndex = std::atoi(token.c_str());

						int normalIndex = 0;
						if (firstSlash != std::string::npos) {
							const size_t secondSlash = token.find('/', firstSlash + 1);
							if (secondSlash != std::string::npos && secondSlash + 1 < token.size()) {
								normalIndex = std::atoi(token.c_str() + secondSlash + 1);
							}
						}

						const int position = positionIndex > 0
							? positionIndex - 1
							: (positionIndex < 0 ? (int)objPositions.size() + positionIndex : -1);
						const int normal = normalIndex > 0
							? normalIndex - 1
							: (normalIndex < 0 ? (int)objNormals.size() + normalIndex : -1);
						if (position < 0 || position >= (int)objPositions.size() ||
							normal < 0 || normal >= (int)objNormals.size()) {
							valid = false;
							break;
						}

						const uint64_t key =
							((uint64_t)(uint32_t)position << 32) | (uint32_t)normal;
						auto found = vertexMap.find(key);
						uint32_t index = 0;
						if (found == vertexMap.end()) {
							index = (uint32_t)positions.size();
							positions.push_back(objPositions[position]);
							const float3 sourceNormal = objNormals[normal];
							const float lengthSquared = dot(sourceNormal, sourceNormal);
							normals.push_back(lengthSquared > 1.0e-12f
								? sourceNormal / std::sqrt(lengthSquared)
								: float3{ 0.0f, 1.0f, 0.0f });
							vertexMap.emplace(key, index);
						}
						else {
							index = found->second;
						}
						face.push_back(index);
					}

					if (!valid || face.size() < 3) {
						positions.clear();
						normals.clear();
						indices.clear();
						return false;
					}

					for (size_t i = 1; i + 1 < face.size(); ++i) {
						indices.push_back(face[0]);
						indices.push_back(face[i]);
						indices.push_back(face[i + 1]);
					}
				}
			}

			return !positions.empty() && positions.size() == normals.size() && !indices.empty();
		}

		// 把网格缩放到最大尺寸 2 并居中，保证切换网格后构图不跳。
		void NormalizeMesh(std::vector<float3>& positions) {
			if (positions.empty()) {
				return;
			}

			constexpr float kMax = std::numeric_limits<float>::max();
			float3 minB{ kMax, kMax, kMax };
			float3 maxB{ -kMax, -kMax, -kMax };
			for (const float3& p : positions) {
				minB = { std::min(minB.x, p.x), std::min(minB.y, p.y), std::min(minB.z, p.z) };
				maxB = { std::max(maxB.x, p.x), std::max(maxB.y, p.y), std::max(maxB.z, p.z) };
			}

			const float3 center = (minB + maxB) * 0.5f;
			const float3 size = maxB - minB;
			const float extent = std::max(size.x, std::max(size.y, size.z));
			const float scale = extent > 1.0e-4f ? 2.0f / extent : 1.0f;
			for (float3& p : positions) {
				p = (p - center) * scale;
			}
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
		if (!rebuildMesh(mMeshType)) {
			CORE_ERROR("[Preview] 预览网格创建失败");
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

	void MaterialPreview::setMeshType(PreviewMesh type) {
		if (type == mMeshType && static_cast<bool>(mEntity)) {
			return;
		}
		mMeshType = type;
		if (mEngine && mScene && mMaterialInstance) {
			rebuildMesh(type);
		}
	}

	bool MaterialPreview::rebuildMesh(PreviewMesh type) {
		if (!mEngine || !mScene || !mMaterialInstance) {
			return false;
		}
		mMeshType = type;

		std::vector<float3> positions;
		std::vector<float3> normals;
		std::vector<uint32_t> indices;
		switch (type) {
		case PreviewMesh::Ball: {
			// 和 Moon / Filament 的材质预览一致：优先加载 Filament 自带的
			// material_sphere.obj；只有资源缺失时才退回程序化球座。
			const QString objPath = ResolveRepoAsset(
				"filament/assets/models/material_sphere/material_sphere.obj");
			if (!objPath.isEmpty() && LoadObjMesh(objPath, positions, normals, indices)) {
				CORE_INFO("[Preview] 已加载 material_sphere.obj: {}",
					objPath.toLocal8Bit().constData());
			}
			else {
				CORE_WARN("[Preview] material_sphere.obj 不可用，回退到程序化 material ball");
				BuildMaterialBall(positions, normals, indices);
			}
			break;
		}
		case PreviewMesh::Cube:
			BuildCube(positions, normals, indices);
			break;
		case PreviewMesh::Plane:
			BuildPlane(positions, normals, indices);
			break;
		case PreviewMesh::Sphere:
		default:
			BuildUVSphere(positions, normals, indices);
			break;
		}

		if (!createRenderable(positions, normals, indices)) {
			return false;
		}
		CORE_INFO("[Preview] 预览网格: {} ({} triangles)",
			PreviewMeshName(type), triangleCount());
		return true;
	}

	bool MaterialPreview::createRenderable(
		const std::vector<filament::math::float3>& positions,
		const std::vector<filament::math::float3>& normals,
		const std::vector<uint32_t>& indices) {
		using namespace filament::math;

		if (positions.empty() || positions.size() != normals.size() || indices.empty()) {
			return false;
		}

		destroyMesh();

		mPositions = positions;
		mTangents.resize(mPositions.size());
		for (size_t i = 0; i < mPositions.size(); ++i) {
			mTangents[i] = MakeTangentFrame(normalize(normals[i]));
		}
		mIndices = indices;
		mIndexCount = (uint32_t)mIndices.size();

		// 统一归一到最大尺寸 2；相机、灯光和 orbit 距离因此对所有网格都成立。
		NormalizeMesh(mPositions);

		constexpr float kMax = std::numeric_limits<float>::max();
		float3 minB{ kMax, kMax, kMax };
		float3 maxB{ -kMax, -kMax, -kMax };
		for (const float3& p : mPositions) {
			minB = { std::min(minB.x, p.x), std::min(minB.y, p.y), std::min(minB.z, p.z) };
			maxB = { std::max(maxB.x, p.x), std::max(maxB.y, p.y), std::max(maxB.z, p.z) };
		}
		const float3 center = (minB + maxB) * 0.5f;
		float3 halfExtent = (maxB - minB) * 0.5f;
		// 平面在 Z 方向没有厚度，给一个最小包围盒，避免被裁剪掉。
		halfExtent.x = std::max(halfExtent.x, 0.01f);
		halfExtent.y = std::max(halfExtent.y, 0.01f);
		halfExtent.z = std::max(halfExtent.z, 0.01f);

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
			.boundingBox({ center, halfExtent })
			.material(0, mMaterialInstance)
			.geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES,
				mVertexBuffer, mIndexBuffer, 0, mIndexCount)
			// 平面从背面看也应该可见；球/立方体/底座保持默认背面剔除。
			.culling(mMeshType != PreviewMesh::Plane)
			.receiveShadows(false)
			.castShadows(false)
			.build(*mEngine, mEntity);

		mScene->addEntity(mEntity);
		return true;
	}

	void MaterialPreview::destroyMesh() {
		if (mScene && mEntity) {
			mScene->remove(mEntity);
		}
		if (mEngine) {
			if (mEntity) {
				mEngine->destroy(mEntity);
			}
			if (mVertexBuffer) {
				mEngine->destroy(mVertexBuffer);
			}
			if (mIndexBuffer) {
				mEngine->destroy(mIndexBuffer);
			}
		}
		if (mEntity) {
			utils::EntityManager::get().destroy(mEntity);
		}

		mEntity = utils::Entity();
		mVertexBuffer = nullptr;
		mIndexBuffer = nullptr;
		mPositions.clear();
		mTangents.clear();
		mIndices.clear();
		mIndexCount = 0;
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

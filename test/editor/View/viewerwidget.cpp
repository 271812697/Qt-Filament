#include "viewerwidget.h"
#include "glloader.h"

#include "core/log.h"

#include <filament/Camera.h>
#include <filament/Engine.h>
//#include <backend/filament.h>
#include <filament/IndexBuffer.h>
#include <filament/IndirectLight.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/TransformManager.h>
#include <filament/VertexBuffer.h>
#include <filament/View.h>
#include <filament/Renderer.h>
#include <filament/Viewport.h>
#include <filament/Box.h>
#include <filament/Texture.h>
#include <utils/EntityManager.h>
#include <filameshio/MeshReader.h>
#include "filament/LightManager.h"
#include "../build/filament/samples/generated/resources/resources.h"

#include <filament-iblprefilter/IBLPrefilterContext.h>
#include <imageio/ImageDecoder.h>
#include <image/LinearImage.h>

#include "PlatformGlfwGL.h"
#include <camutils/Manipulator.h>
#include <QResizeEvent>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
struct App {
	utils::Entity light;
	filament::Material* material;
	filament::MaterialInstance* materialInstance;
	filamesh::MeshReader::Mesh mesh;
	filament::Camera* cam;
	filament::camutils::Manipulator<float>* mMainCameraMan;
	filament::math::mat4f transform;
};

namespace MOON {

	struct OpenGLProcAddressHelper {
		inline static QOpenGLContext* ctx;
		static void* getProcAddress(const char* name) {
			return (void*)ctx->getProcAddress(name);
		}
	};
	filament::Engine* engine = nullptr;
	filament::Renderer* renderer = nullptr;

	filament::Scene* scene = nullptr;// engine->createScene();
	filament::View* view = nullptr;// engine->createView();
	filament::SwapChain* swapchain = nullptr;// engine->createSwapChain(this, 0);
	App app;

	// ----------------------------------------------------------------------------
	// SaveDomains 二进制网格文件加载（与 PathTrace::Mesh.cpp 的 SaveDomains 对应）
	// 文件布局:
	//   uint64 domainCount
	//   每个 domain:
	//     uint64 ptSize, uint64 nmlSize, uint64 triSize
	//     ptSize 个 Vector3d(3×double)  points
	//     nmlSize 个 Vector3d(3×double)  normals
	//     triSize 个 Facet(3×uint32)    indices
	// ----------------------------------------------------------------------------
	struct LoadedDomain {
		std::vector<filament::math::float3> positions;
		std::vector<filament::math::float3> normals;
		std::vector<uint32_t> indices;
	};

	// 按优先级尝试的文件路径（SaveDomains 输出名是 "res"，无扩展名）
	static const char* kDomainMeshPaths[] = {
		"C:/Users/27181/Desktop/ImGui-D3D11Hook-master/2.50Atypec.bin", // 现有测试数据
		"res",                                          // SaveDomains 默认输出
	};

	// 每个 Renderable 合并的 domain 数量（性能测试用）。
	// 默认 1 = 每个 domain 独立一个 Renderable（draw call 最多）；
	// 可用环境变量 DOMAINS_PER_MESH 覆盖，无需重新编译。
	static int g_domainsPerMesh = 1;

	// 把单位法线转成 Filament 的 TANGENTS 四元数（旋转 (0,0,1) 到 n 的最短弧）
	static filament::math::float4 MakeTangentFrame(const filament::math::float3& n)
	{
		const float denom = std::sqrt(2.0f * (1.0f + n.z));
		if (denom > 1e-4f) {
			return { -n.y / denom, n.x / denom, 0.0f, denom * 0.5f };
		}
		// n ≈ (0,0,-1)：绕 X 轴转 180°
		return { 1.0f, 0.0f, 0.0f, 0.0f };
	}

	// 分量级 min/max（math 库的 min/max 只支持标量）
	static filament::math::float3 MinVec(const filament::math::float3& a, const filament::math::float3& b)
	{
		return { std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) };
	}
	static filament::math::float3 MaxVec(const filament::math::float3& a, const filament::math::float3& b)
	{
		return { std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) };
	}

	// HSV → RGB（h,s,v ∈ [0,1]）
	static filament::math::float3 HsvToRgb(float h, float s, float v)
	{
		const int i = int(h * 6.0f) % 6;
		const float f = h * 6.0f - std::floor(h * 6.0f);
		const float p = v * (1.0f - s);
		const float q = v * (1.0f - f * s);
		const float t = v * (1.0f - (1.0f - f) * s);
		switch (i) {
		case 0: return { v, t, p };
		case 1: return { q, v, p };
		case 2: return { p, v, t };
		case 3: return { p, q, v };
		case 4: return { t, p, v };
		default: return { v, p, q };
		}
	}

	// 黄金角分布的颜色（相邻 mesh 颜色差异大）
	static filament::math::float3 MakeMeshColor(size_t index)
	{
		const float hue = std::fmod(index * 0.61803398875f, 1.0f);
		return HsvToRgb(hue, 0.45f, 0.85f);
	}

	struct UploadedMesh {
		std::vector<filament::math::float3> positions;
		std::vector<filament::math::float4> tangents;
		std::vector<uint32_t> indices;
		filament::math::float3 minB;
		filament::math::float3 maxB;
	};
	static std::vector<UploadedMesh> g_domainMeshes;    // CPU 数据，命令执行期间必须存活
	static std::vector<utils::Entity> g_domainEntities; // 每个 Renderable 的实体
	static filament::math::float3 g_unionMin;
	static filament::math::float3 g_unionMax;
	static float g_fitScale = 1.0f;
	static uint64_t g_totalTriangles = 0;

	static bool LoadDomainsFromFile(const std::string& path, std::vector<LoadedDomain>& out)
	{
		std::ifstream in(path, std::ios::binary);
		if (!in.is_open()) return false;

		uint64_t domainCount = 0;
		in.read(reinterpret_cast<char*>(&domainCount), sizeof(domainCount));
		if (!in) return false;
		out.reserve((size_t)domainCount);

		for (uint64_t d = 0; d < domainCount; d++) {
			uint64_t ptSize = 0, nmlSize = 0, triSize = 0;
			in.read(reinterpret_cast<char*>(&ptSize), sizeof(ptSize));
			in.read(reinterpret_cast<char*>(&nmlSize), sizeof(nmlSize));
			in.read(reinterpret_cast<char*>(&triSize), sizeof(triSize));
			if (!in) return false;

			LoadedDomain dom;
			dom.positions.resize((size_t)ptSize);
			dom.normals.resize((size_t)nmlSize);
			dom.indices.resize((size_t)triSize * 3);

			// 整块读入再转 float3，避免逐分量 read
			std::vector<double> raw((size_t)(ptSize + nmlSize) * 3);
			in.read(reinterpret_cast<char*>(raw.data()),
				(std::streamsize)(raw.size() * sizeof(double)));
			if (!in) return false;

			for (size_t i = 0; i < ptSize; i++) {
				dom.positions[i] = {
					(float)raw[i * 3 + 0], (float)raw[i * 3 + 1], (float)raw[i * 3 + 2]
				};
			}
			for (size_t i = 0; i < nmlSize; i++) {
				dom.normals[i] = {
					(float)raw[(ptSize + i) * 3 + 0],
					(float)raw[(ptSize + i) * 3 + 1],
					(float)raw[(ptSize + i) * 3 + 2]
				};
			}

			in.read(reinterpret_cast<char*>(dom.indices.data()),
				(std::streamsize)(dom.indices.size() * sizeof(uint32_t)));
			if (!in) return false;

			// 法线缺失（nmlSize != ptSize）时，用面法线累加补一份
			if (dom.normals.size() != dom.positions.size()) {
				std::vector<filament::math::float3> flat(dom.positions.size(), { 0, 0, 0 });
				for (size_t t = 0; t + 2 < dom.indices.size(); t += 3) {
					uint32_t i0 = dom.indices[t], i1 = dom.indices[t + 1], i2 = dom.indices[t + 2];
					if (i0 >= dom.positions.size() || i1 >= dom.positions.size() ||
						i2 >= dom.positions.size()) continue;
					const auto e1 = dom.positions[i1] - dom.positions[i0];
					const auto e2 = dom.positions[i2] - dom.positions[i0];
					const auto fn = cross(e1, e2);
					flat[i0] += fn; flat[i1] += fn; flat[i2] += fn;
				}
				dom.normals.resize(dom.positions.size());
				for (size_t i = 0; i < dom.normals.size(); i++) {
					dom.normals[i] = normalize(flat[i]);
				}
			}
			out.push_back(std::move(dom));
		}
		return true;
	}

	// 把 [begin, end) 区间内的 domain 合并成一个网格（局部索引 → 全局偏移）
	static bool MergeDomains(const std::vector<LoadedDomain>& domains,
		size_t begin, size_t end, UploadedMesh& out)
	{
		uint64_t totalPoints = 0, totalTris = 0;
		for (size_t i = begin; i < end; i++) {
			const auto& d = domains[i];
			totalPoints += d.positions.size();
			totalTris += d.indices.size() / 3;
		}
		if (totalPoints == 0 || totalTris == 0) return false;

		out.positions.resize((size_t)totalPoints);
		out.tangents.resize((size_t)totalPoints);
		out.indices.resize((size_t)totalTris * 3);

		out.minB = { std::numeric_limits<float>::max() };
		out.maxB = { -std::numeric_limits<float>::max() };
		uint64_t pointBase = 0;
		uint64_t indexOut = 0;

		for (size_t i = begin; i < end; i++) {
			const auto& d = domains[i];
			const size_t n = d.positions.size();
			for (size_t j = 0; j < n; j++) {
				out.positions[pointBase + j] = d.positions[j];
				out.minB = MinVec(out.minB, d.positions[j]);
				out.maxB = MaxVec(out.maxB, d.positions[j]);
			}
			if (d.normals.size() == n) {
				for (size_t j = 0; j < n; j++) {
					out.tangents[pointBase + j] = MakeTangentFrame(normalize(d.normals[j]));
				}
			}
			else {
				std::vector<filament::math::float3> flat(n, { 0, 0, 0 });
				for (size_t t = 0; t + 2 < d.indices.size(); t += 3) {
					uint32_t i0 = d.indices[t], i1 = d.indices[t + 1], i2 = d.indices[t + 2];
					if (i0 >= n || i1 >= n || i2 >= n) continue;
					const auto e1 = out.positions[pointBase + i1] - out.positions[pointBase + i0];
					const auto e2 = out.positions[pointBase + i2] - out.positions[pointBase + i0];
					const auto fn = cross(e1, e2);
					flat[i0] += fn; flat[i1] += fn; flat[i2] += fn;
				}
				for (size_t j = 0; j < n; j++) {
					out.tangents[pointBase + j] = MakeTangentFrame(normalize(flat[j]));
				}
			}
			for (size_t t = 0; t + 2 < d.indices.size(); t += 3) {
				uint32_t i0 = d.indices[t], i1 = d.indices[t + 1], i2 = d.indices[t + 2];
				if (i0 >= n || i1 >= n || i2 >= n) continue; // 跳过坏索引
				out.indices[indexOut++] = (uint32_t)(pointBase + i0);
				out.indices[indexOut++] = (uint32_t)(pointBase + i1);
				out.indices[indexOut++] = (uint32_t)(pointBase + i2);
			}
			pointBase += n;
		}
		out.indices.resize((size_t)indexOut);
		if (indexOut == 0) return false;
		return true;
	}

	// 把 g_domainMeshes 逐个构建成 Filament Renderable 并加入场景
	static void AddDomainMeshesToScene(filament::Engine* engine, filament::Scene* scene,
		filament::Material* material)
	{
		if (g_domainMeshes.empty()) return;

		const filament::math::float3 center = (g_unionMin + g_unionMax) * 0.5f;
		const filament::math::mat4f fit =
			filament::math::mat4f::translation(filament::math::float3{ 0, 0, -2 }) *
			filament::math::mat4f::scaling(g_fitScale) *
			filament::math::mat4f::translation(-center);

		auto& tcm = engine->getTransformManager();
		const size_t meshCount = g_domainMeshes.size();
		constexpr size_t PROGRESS_STEP = 4096;
		for (size_t i = 0; i < g_domainMeshes.size(); i++) {
			auto& m = g_domainMeshes[i];

			// 每个 mesh 一个材质实例 + 独立颜色（黄金角调色板）
			filament::MaterialInstance* mi = material->createInstance();
			mi->setParameter("baseColor", filament::RgbType::LINEAR, MakeMeshColor(i));
			mi->setParameter("metallic", 0.0f);
			mi->setParameter("roughness", 0.6f);
			mi->setParameter("reflectance", 0.5f);

			filament::VertexBuffer* vb = filament::VertexBuffer::Builder()
				.vertexCount((uint32_t)m.positions.size())
				.bufferCount(2)
				.attribute(filament::VertexAttribute::POSITION, 0,
					filament::VertexBuffer::AttributeType::FLOAT3, 0, sizeof(float) * 3)
				.attribute(filament::VertexAttribute::TANGENTS, 1,
					filament::VertexBuffer::AttributeType::FLOAT4, 0, sizeof(float) * 4)
				.build(*engine);
			vb->setBufferAt(*engine, 0, filament::VertexBuffer::BufferDescriptor(
				m.positions.data(),
				m.positions.size() * sizeof(filament::math::float3), nullptr));
			vb->setBufferAt(*engine, 1, filament::VertexBuffer::BufferDescriptor(
				m.tangents.data(),
				m.tangents.size() * sizeof(filament::math::float4), nullptr));

			filament::IndexBuffer* ib = filament::IndexBuffer::Builder()
				.indexCount((uint32_t)m.indices.size())
				.bufferType(filament::IndexBuffer::IndexType::UINT)
				.build(*engine);
			ib->setBuffer(*engine, filament::IndexBuffer::BufferDescriptor(
				m.indices.data(),
				m.indices.size() * sizeof(uint32_t), nullptr));

			utils::Entity entity = utils::EntityManager::get().create();
			const filament::math::float3 meshCenter = (m.minB + m.maxB) * 0.5f;
			const filament::math::float3 meshHalf = (m.maxB - m.minB) * 0.5f;
			filament::RenderableManager::Builder(1)
				.boundingBox({ meshCenter, meshHalf })
				.material(0, mi)
				.geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES,
					vb, ib, 0, (uint32_t)m.indices.size())
				.culling(true)
				.receiveShadows(false)
				.castShadows(false)
				.build(*engine, entity);
			scene->addEntity(entity);

			// 整体模型居中 + 缩放到视野内，所有 Renderable 应用同一变换
			tcm.setTransform(tcm.getInstance(entity), fit);
			g_domainEntities.push_back(entity);

			if ((i + 1) % PROGRESS_STEP == 0 || i + 1 == meshCount) {
				CORE_INFO("[Mesh] 已构建 {}/{} 个 Renderable...", i + 1, meshCount);
			}
		}
	}

	// 入口：找文件 → 解析 → 分组合并 → 建 Renderable
	static void LoadAndAddDomainMeshes(filament::Engine* engine, filament::Scene* scene,
		filament::Material* material)
	{
		if (const char* v = getenv("DOMAINS_PER_MESH")) {
			const int n = atoi(v);
			if (n > 0) g_domainsPerMesh = n;
		}

		std::string path;
		for (const char* p : kDomainMeshPaths) {
			std::ifstream test(p, std::ios::binary);
			if (test.is_open()) {
				test.close();
				path = p;
				break;
			}
		}
		if (path.empty()) {
			CORE_WARN("[Mesh] 未找到 SaveDomains 文件（默认名 res）");
			return;
		}

		std::vector<LoadedDomain> domains;
		if (!LoadDomainsFromFile(path, domains) || domains.empty()) {
			CORE_ERROR("[Mesh] 解析失败: {}", path);
			return;
		}

		// 按 g_domainsPerMesh 分组，每组合并成一个网格（一个 Renderable）
		g_domainMeshes.clear();
		g_domainEntities.clear();
		g_unionMin = { std::numeric_limits<float>::max() };
		g_unionMax = { -std::numeric_limits<float>::max() };
		const size_t domainCount = domains.size();
		g_domainMeshes.reserve((domainCount + g_domainsPerMesh - 1) / g_domainsPerMesh);

		for (size_t b = 0; b < domainCount; b += g_domainsPerMesh) {
			const size_t e = std::min(b + (size_t)g_domainsPerMesh, domainCount);
			UploadedMesh mesh;
			if (!MergeDomains(domains, b, e, mesh)) continue;
			g_unionMin = MinVec(g_unionMin, mesh.minB);
			g_unionMax = MaxVec(g_unionMax, mesh.maxB);
			g_domainMeshes.push_back(std::move(mesh));
		}
		if (g_domainMeshes.empty()) {
			CORE_ERROR("[Mesh] 合并失败（空网格）: {}", path);
			return;
		}

		const filament::math::float3 extent = g_unionMax - g_unionMin;
		const float maxExtent = filament::math::max(
			extent.x, filament::math::max(extent.y, extent.z));
		g_fitScale = maxExtent > 0.0f ? 4.0f / maxExtent : 1.0f;

		AddDomainMeshesToScene(engine, scene, material);
		uint64_t totalTris = 0;
		for (const auto& m : g_domainMeshes) totalTris += m.indices.size() / 3;
		g_totalTriangles = totalTris;
		CORE_INFO("[Mesh] 已加载 {}: domains={} renderables={} (每个 Renderable 合并 {} 个 domain) triangles={}",
			path, domainCount, g_domainMeshes.size(), g_domainsPerMesh, totalTris);
	}

	// ----------------------------------------------------------------------------
	// IBL（基于 Filament 样例 filamentapp::IBL::loadFromEquirect 的最小实现）
	// 从等距柱状 HDR/EXR 生成：天空盒 cubemap + 预滤波 reflections + 环境光
	// ----------------------------------------------------------------------------
	static constexpr float IBL_INTENSITY = 8000.0f;

	static bool SetupIBL(filament::Engine* engine, filament::Scene* scene)
	{
		static const char* kIblPaths[] = {
			"C:/Project/demo/Qt-Filament/filament/assets/environments/white_furnace/white_furnace.exr",
			"assets/environments/white_furnace/white_furnace.exr",
		};
		std::string path;
		for (const char* p : kIblPaths) {
			std::ifstream test(p, std::ios::binary);
			if (test.is_open()) {
				test.close();
				path = p;
				break;
			}
		}
		if (path.empty()) return false;

		std::ifstream in(path, std::ios::binary);
		image::LinearImage* img = new image::LinearImage(
			image::ImageDecoder::decode(in, path));
		if (!img->isValid()) {
			delete img;
			return false;
		}

		const uint32_t w = img->getWidth();
		const uint32_t h = img->getHeight();
		const uint32_t n = img->getChannels();
		if (w != h * 2 || n != 3) { // 必须是 2:1 等距柱状图、RGB
			delete img;
			return false;
		}

		// 上传等距柱状图（引擎执行命令时才会真正拷贝，这里用回调管理生命周期）
		filament::Texture::PixelBufferDescriptor buffer(
			img->getPixelRef(),
			(size_t)w * h * n * sizeof(float),
			filament::Texture::Format::RGB,
			filament::Texture::Type::FLOAT,
			[](void*, size_t, void* user) {
				delete reinterpret_cast<image::LinearImage*>(user);
			},
			img);

		filament::Texture* equirect = filament::Texture::Builder()
			.width(w)
			.height(h)
			.levels(0xff)
			.format(filament::Texture::InternalFormat::R11F_G11F_B10F)
			.sampler(filament::Texture::Sampler::SAMPLER_2D)
			.usage(filament::Texture::Usage::DEFAULT | filament::Texture::Usage::GEN_MIPMAPPABLE)
			.build(*engine);
		equirect->setImage(*engine, 0, std::move(buffer));

		// GPU 预滤波：等距 → cubemap → specular mip 链
		IBLPrefilterContext context(*engine);
		IBLPrefilterContext::EquirectangularToCubemap equirectangularToCubemap(context);
		IBLPrefilterContext::SpecularFilter specularFilter(context);

		filament::Texture* skyboxTexture = equirectangularToCubemap(equirect);
		engine->destroy(equirect);
		filament::Texture* iblTexture = specularFilter(skyboxTexture);

		filament::IndirectLight* ibl = filament::IndirectLight::Builder()
			.reflections(iblTexture)
			.intensity(IBL_INTENSITY)
			.build(*engine);
		filament::Skybox* skybox = filament::Skybox::Builder()
			.environment(skyboxTexture)
			.showSun(true)
			.build(*engine);

		scene->setIndirectLight(ibl);
		scene->setSkybox(skybox);
		CORE_INFO("[IBL] 已加载 {} ({}x{})", path, w, h);
		return true;
	}

	ViewerWidget::ViewerWidget(QWidget* parent) {
		this->setFocusPolicy(Qt::StrongFocus);
		this->setMouseTracking(true);
		QSurfaceFormat format;
		format.setSamples(0); // 离屏渲染 + blit 方案要求默认 FBO 单采样
		this->setFormat(format);

	}
	ViewerWidget::~ViewerWidget() {

	}
	void ViewerWidget::initializeGL() {

		QOpenGLWidget::initializeGL();
		// opengl funcs
		bool flag = initializeOpenGLFunctions();
		OpenGLProcAddressHelper::ctx = context();
		//CUSTOM_GL_API::CustomLoadGL(OpenGLProcAddressHelper::getProcAddress);
		GlLoader::CustomLoadGL(OpenGLProcAddressHelper::getProcAddress);

		//开启计时器
		this->startTimer(0);
		mElapsed.start();
		mLastFrameTime = -1.0;
		glGenFramebuffers(1, &mBlitFbo); // GUI 线程用于把共享纹理 blit 到 Qt FBO

		filament::backend::PlatformGlfwGL* platform = new filament::backend::PlatformGlfwGL();
		platform->glwidget = this;
		platform->setSharedContext(context());
		platform->setTargetSize((uint32_t)width(), (uint32_t)height());
		mPlatform = platform;

		// 延迟到事件循环空闲时创建引擎：此时 Qt widget 的 GL 上下文不在 current 状态，
		// wglShareLists 才能成功建立共享（否则 ERROR_BUSY，纹理无法共享 → blit 黑屏）
		QTimer::singleShot(0, this, [this]() { createEngineAndSetup(); });
	}

	void ViewerWidget::createEngineAndSetup() {
		if (engine) return;

		// Qt 的 QOpenGLWidget 在两次渲染之间也会让上下文保持 current，
		// 而 wglShareLists 要求源上下文不被占用——必须先 doneCurrent 释放，
		// 否则驱动线程无法与 widget 上下文共享纹理（blit 结果全黑）。
		if (context() && QOpenGLContext::currentContext() == context()) {
			context()->doneCurrent();
		}

		auto createEngine = [&]() {
			auto backend = filament::Engine::Backend::OPENGL;
			filament::Engine::Config engineConfig = {};
			engineConfig.stereoscopicEyeCount = 2;
			engineConfig.stereoscopicType = filament::Engine::StereoscopicType::NONE;
			// 网格数量很大（数万个 Renderable/材质实例），默认 4MB 的
			// 后端句柄池会被撑爆并退回系统堆，这里调大到 64MB
			engineConfig.driverHandleArenaSizeMB = 64;
			return filament::Engine::Builder()
				.backend(backend)
				.featureLevel(filament::backend::FeatureLevel::FEATURE_LEVEL_3)
				.config(&engineConfig).platform(mPlatform)
				.build();
			};
		engine = createEngine();
		renderer = engine->createRenderer();
		scene = engine->createScene();
		view = engine->createView();
		swapchain = engine->createSwapChain(this, 0);
		view->setName("Main View");

		auto setup = [&]() {
			view->setPostProcessingEnabled(true);
			auto& em = utils::EntityManager::get();
			//if (!SetupIBL(engine, scene)) 
			{
				// IBL 加载失败时回退到纯色天空盒
				auto skybox = filament::Skybox::Builder()
					.color({ 0.1, 0.125, 0.25, 1.0 }).build(*engine);
				scene->setSkybox(skybox);
				CORE_WARN("[IBL] 未找到环境贴图，使用纯色天空盒");
			}
			app.mMainCameraMan = filament::camutils::Manipulator<float>::Builder()
				.targetPosition(0, 0, 0)
				.zoomSpeed(0.1f)
				.flightMoveDamping(15.0)
				.build(filament::camutils::Mode::ORBIT);
			app.mMainCameraMan->setViewport(width(), height());
			app.material = filament::Material::Builder()
				.package(RESOURCES_AIDEFAULTMAT_DATA, RESOURCES_AIDEFAULTMAT_SIZE).build(*engine);

			// Add light sources into the scene.
			app.light = em.create();
			filament::LightManager::Builder(filament::LightManager::Type::SUN)
				.color(filament::Color::toLinear<filament::ACCURATE>(filament::sRGBColor(0.98f, 0.92f, 0.89f)))
				.intensity(110000)
				.direction({ 0.7, -1, -0.8 })
				.sunAngularRadius(1.9f)
				.castShadows(false)
				.build(*engine, app.light);
			scene->addEntity(app.light);
			//scene->addEntity(renderable);
			auto camera = utils::EntityManager::get().create();
			app.cam = engine->createCamera(camera);
			view->setCamera(app.cam);

			// 加载 SaveDomains 保存的二进制网格并加入场景
			LoadAndAddDomainMeshes(engine, scene, app.material);
			};
		setup();
		view->setScene(scene);
		view->setViewport({ 0, 0, (uint32_t)width(), (uint32_t)height() });
	}
	void ViewerWidget::timerEvent(QTimerEvent* e) {
		this->update();
	}
	void animate(filament::Engine* engine, filament::View* view, double now, double deltaTime) {
		app.mMainCameraMan->update(deltaTime);
		filament::math::float3 eye, center, up;
		app.mMainCameraMan->getLookAt(&eye, &center, &up);
		app.cam->lookAt(eye, center, up);
		const uint32_t w = view->getViewport().width;
		const uint32_t h = view->getViewport().height;
		const float aspect = (float)w / h;
		app.cam->setProjection(45.0, aspect, 0.1, 1000.0);
	};
	void ViewerWidget::paintGL() {
		// 引擎尚未创建（initializeGL 后第一个空闲时刻才创建）
		if (!engine || !view || !renderer) {
			glClearColor(0.1f, 0.125f, 0.25f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			return;
		}

		const double now = mElapsed.elapsed() / 1000.0;
		const double delta = mLastFrameTime < 0.0 ? 0.0 : now - mLastFrameTime;
		mLastFrameTime = now;

		// 每秒打印一次帧率与 draw call 规模（性能测试用）
		if (!g_domainEntities.empty()) {
			static int frameCount = 0;
			static double lastFpsTime = 0.0;
			frameCount++;
			if (now - lastFpsTime >= 1.0) {
				CORE_INFO("[FPS] {} fps, renderables(draw calls)={}, triangles={}",
					frameCount / (now - lastFpsTime),
					g_domainEntities.size(), g_totalTriangles);

				// CPU/GPU 时间拆分（上一帧的 FrameInfo）
				auto history = renderer->getFrameInfoHistory(1);
				if (!history.empty()) {
					const auto& fi = history[0];
					const int64_t frontendUs = (fi.endFrame - fi.beginFrame) / 1000;
					const int64_t backendUs =
						(fi.backendEndFrame - fi.backendBeginFrame) / 1000;
					const int64_t gpuUs =
						(fi.gpuFrameDuration > 0 ? fi.gpuFrameDuration
							: fi.denoisedGpuFrameDuration) / 1000;
					CORE_INFO("  [Frame] frontend(cpu)={}us backend(cpu)={}us gpu={}us",
						frontendUs, backendUs, gpuUs);
				}
				frameCount = 0;
				lastFpsTime = now;
			}
		}

		animate(engine, view, now, delta);
		bool submitted = false;
		if (renderer->beginFrame(swapchain)) {
			renderer->render(view);
			renderer->endFrame();
			submitted = true;
		}

		// 多线程后端：命令由驱动线程异步执行，GUI 线程不再 execute。
		// 渲染结果在驱动线程 commit() 时拷入环形纹理，这里把上一帧 blit 到 Qt FBO。
		auto present = [&](int slot) {
			if (!mPlatform || !mBlitFbo) return;
			const GLuint tex = mPlatform->getTextureId((uint32_t)slot);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, mBlitFbo);
			glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, tex, 0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebufferObject());
			const GLsizei w = (GLsizei)width();
			const GLsizei h = (GLsizei)height();
			glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
			glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
			mHasBlitContent = true;
			mLastBlitSlot = slot;
			};

		if (submitted) {
			mSubmittedFrames++;
			if (mSubmittedFrames >= 2 && mPlatform) {
				// 等待驱动线程完成上一帧的 commit（有 1 帧延迟，等待时间很短）
				mPlatform->waitFrameCompleted(mSubmittedFrames - 1);
				const int slot = (int)((mSubmittedFrames - 2) % 3); // RING_COUNT = 3
				present(slot);
			}
		}
		else if (mHasBlitContent && mPlatform) {
			// beginFrame 返回 false（跳帧）时重复呈现上一帧，避免黑屏/闪烁
			present(mLastBlitSlot);
		}
	}
	bool ViewerWidget::event(QEvent* evt) {
		return QOpenGLWidget::event(evt);
	}
	void ViewerWidget::leaveEvent(QEvent* event) {
		if (mGrabbing) {
			app.mMainCameraMan->grabEnd();
			mGrabbing = false;
		}
	}

	void ViewerWidget::resizeEvent(QResizeEvent* event) {
		unsigned int viewW = event->size().width();
		unsigned int viewH = event->size().height();
		QOpenGLWidget::resizeEvent(event);
		if (view) {
			view->setViewport({ 0,0,viewW,viewH });
		}
		if (app.mMainCameraMan) {
			app.mMainCameraMan->setViewport((int)viewW, (int)viewH);
		}
		if (mPlatform) {
			mPlatform->setTargetSize(viewW, viewH); // 驱动线程下次 makeCurrent 时重建离屏缓冲
		}
	}
	void ViewerWidget::mousePressEvent(QMouseEvent* event) {
		if (event->button() == Qt::LeftButton ||
			event->button() == Qt::MiddleButton ||
			event->button() == Qt::RightButton) {
			const bool strafe = (event->button() != Qt::LeftButton);
			app.mMainCameraMan->grabBegin(event->pos().x(), event->pos().y(), strafe);
			mGrabbing = true;
			event->accept();
		}
	}

	void ViewerWidget::mouseMoveEvent(QMouseEvent* event) {
		if (mGrabbing) {
			app.mMainCameraMan->grabUpdate(event->pos().x(), event->pos().y());
			event->accept();
		}
	}

	void ViewerWidget::mouseReleaseEvent(QMouseEvent* event) {
		if (mGrabbing) {
			app.mMainCameraMan->grabEnd();
			mGrabbing = false;
		}
	}

	void ViewerWidget::wheelEvent(QWheelEvent* event) {
		QPoint delta = event->angleDelta();
		if (delta.isNull()) {
			delta = event->pixelDelta();
		}
		const float scrollDelta = -delta.y() / 120.0f; // 向上滚 = 拉近
		if (scrollDelta != 0.0f) {
			app.mMainCameraMan->scroll(event->pos().x(), event->pos().y(), scrollDelta);
			event->accept();
		}
	}
	void ViewerWidget::keyPressEvent(QKeyEvent* event) {

	}
	void ViewerWidget::keyReleaseEvent(QKeyEvent* event) {

	}

	void ViewerWidget::showImGui() {

	}
}




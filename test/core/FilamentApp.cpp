#include "FilamentApp.h"
#include "log.h"

#include <QOpenGLContext>
#include <QOpenGLWidget>
#include <QMetaObject>

#include <filament/Camera.h>
#include <filament/Box.h>
#include <filament/Color.h>
#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/LightManager.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderableManager.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/TransformManager.h>
#include <filament/VertexBuffer.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <utils/EntityManager.h>
#include <camutils/Manipulator.h>
#include "generated/resources/resources.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <math/mat4.h>
#include <math/vec3.h>
#include <math/vec4.h>
#include <string>
#include <vector>

namespace MOON {
namespace {

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
	static std::vector<filament::VertexBuffer*> g_domainVertexBuffers;      // 卸载时销毁
	static std::vector<filament::IndexBuffer*> g_domainIndexBuffers;        // 卸载时销毁
	static std::vector<filament::MaterialInstance*> g_domainMaterialInstances; // 卸载时销毁
	static std::vector<filament::math::float3> g_domainBaseColors;             // 各实体原 baseColor
	static std::vector<uint8_t> g_domainVisible;                            // 每个实体的可见性
	static constexpr size_t kNoHighlight = std::numeric_limits<size_t>::max();
	static size_t g_highlightedEntity = kNoHighlight;
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

	// 卸载当前已加载的 domain 网格：移出场景并销毁实体 / GPU 资源 / CPU 缓存。
	// 注意：调用前需确保旧网格的 buffer 上传命令已执行完毕（新帧已提交过）。
	static void RemoveAllDomainMeshes(filament::Engine* engine, filament::Scene* scene)
	{
		if (!engine) return;

		if (scene) {
			for (utils::Entity e : g_domainEntities) {
				scene->remove(e);
			}
		}
		// 先销毁实体：RenderableManager 会移除 renderable，解除其对
		// MaterialInstance 的引用（否则 destroy(MaterialInstance) 会断言
		// “material instance still in use by Renderable”）
		for (utils::Entity e : g_domainEntities) {
			engine->destroy(e);
		}
		const size_t count = g_domainEntities.size();
		for (size_t i = 0; i < count; i++) {
			if (i < g_domainMaterialInstances.size())
				engine->destroy(g_domainMaterialInstances[i]);
			if (i < g_domainVertexBuffers.size())
				engine->destroy(g_domainVertexBuffers[i]);
			if (i < g_domainIndexBuffers.size())
				engine->destroy(g_domainIndexBuffers[i]);
			utils::EntityManager::get().destroy(g_domainEntities[i]);
		}

		g_domainEntities.clear();
		g_domainVertexBuffers.clear();
		g_domainIndexBuffers.clear();
		g_domainMaterialInstances.clear();
		g_domainBaseColors.clear();
		g_domainVisible.clear();
		g_domainMeshes.clear();
		g_totalTriangles = 0;
		g_highlightedEntity = kNoHighlight;
	}

	// 把 g_domainMeshes[beginIndex..] 这批新 mesh 构建成 Renderable 加入场景。
	// batchMin/batchMax/fitScale 只对本次加载生效，不动之前已加载的实体。
	static void AddDomainMeshesToScene(filament::Engine* engine, filament::Scene* scene,
		filament::Material* material, size_t beginIndex,
		const filament::math::float3& batchMin, const filament::math::float3& batchMax,
		float fitScale)
	{
		const size_t meshCount = g_domainMeshes.size();
		if (beginIndex >= meshCount) return;

		const filament::math::float3 center = (batchMin + batchMax) * 0.5f;
		const filament::math::mat4f fit =
			filament::math::mat4f::translation(filament::math::float3{ 0, 0, -2 }) *
			filament::math::mat4f::scaling(fitScale) *
			filament::math::mat4f::translation(-center);

		auto& tcm = engine->getTransformManager();
		constexpr size_t PROGRESS_STEP = 4096;
		size_t built = 0;
		for (size_t i = beginIndex; i < meshCount; i++) {
			auto& m = g_domainMeshes[i];
			built++;

			// 每个 mesh 一个材质实例 + 独立颜色（黄金角调色板，全局序号避免撞色）
			filament::MaterialInstance* mi = material->createInstance();
			const filament::math::float3 baseColor = MakeMeshColor(i);
			mi->setParameter("baseColor", filament::RgbType::LINEAR, baseColor);
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

			// 本次文件整体居中 + 缩放到视野内；旧实体保持原变换不动
			tcm.setTransform(tcm.getInstance(entity), fit);
			g_domainEntities.push_back(entity);
			g_domainVertexBuffers.push_back(vb);
			g_domainIndexBuffers.push_back(ib);
			g_domainMaterialInstances.push_back(mi);
			g_domainBaseColors.push_back(baseColor);
			g_domainVisible.push_back(1);   // 新加载的实体默认可见

			if (built % PROGRESS_STEP == 0 || i + 1 == meshCount) {
				CORE_INFO("[Mesh] 已构建 {}/{} 个 Renderable...", built, meshCount - beginIndex);
			}
		}
	}

	// 从显式路径加载：解析 → 分组合并 → 追加进场景（不清空已有 mesh）
	static bool LoadAndAddDomainMeshes(filament::Engine* engine, filament::Scene* scene,
		filament::Material* material, const std::string& path)
	{
		if (path.empty()) {
			CORE_WARN("[Mesh] 未指定 SaveDomains 文件路径");
			return false;
		}
		if (const char* v = getenv("DOMAINS_PER_MESH")) {
			const int n = atoi(v);
			if (n > 0) g_domainsPerMesh = n;
		}

		std::vector<LoadedDomain> domains;
		if (!LoadDomainsFromFile(path, domains) || domains.empty()) {
			CORE_ERROR("[Mesh] 解析失败: {}", path);
			return false;
		}

		// 先合并到临时缓冲，解析/合并失败时不改动当前场景
		filament::math::float3 unionMin{ std::numeric_limits<float>::max() };
		filament::math::float3 unionMax{ -std::numeric_limits<float>::max() };
		const size_t domainCount = domains.size();
		std::vector<UploadedMesh> merged;
		merged.reserve((domainCount + g_domainsPerMesh - 1) / g_domainsPerMesh);
		for (size_t b = 0; b < domainCount; b += g_domainsPerMesh) {
			const size_t e = std::min(b + (size_t)g_domainsPerMesh, domainCount);
			UploadedMesh mesh;
			if (!MergeDomains(domains, b, e, mesh)) continue;
			unionMin = MinVec(unionMin, mesh.minB);
			unionMax = MaxVec(unionMax, mesh.maxB);
			merged.push_back(std::move(mesh));
		}
		if (merged.empty()) {
			CORE_ERROR("[Mesh] 合并失败（空网格）: {}", path);
			return false;
		}

		const filament::math::float3 extent = unionMax - unionMin;
		const float maxExtent = filament::math::max(
			extent.x, filament::math::max(extent.y, extent.z));
		const float fitScale = maxExtent > 0.0f ? 4.0f / maxExtent : 1.0f;

		// 追加：把本文件合并结果接到全局列表尾部，然后只构建新加入的这一批
		const size_t beginIndex = g_domainMeshes.size();
		const size_t addedRenderables = merged.size();
		uint64_t addedTris = 0;
		for (const auto& m : merged) addedTris += m.indices.size() / 3;

		g_domainMeshes.insert(g_domainMeshes.end(),
			std::make_move_iterator(merged.begin()),
			std::make_move_iterator(merged.end()));
		g_totalTriangles += addedTris;

		AddDomainMeshesToScene(engine, scene, material,
			beginIndex, unionMin, unionMax, fitScale);
		CORE_INFO("[Mesh] 已追加加载 {}: domains={} renderables(本次)={} 累计={} "
			"(每个 Renderable 合并 {} 个 domain) triangles(本次)={}",
			path, domainCount, addedRenderables, g_domainMeshes.size(),
			g_domainsPerMesh, addedTris);
		return true;
	}

	// 把所有实体的局部 AABB 用世界变换转到世界空间，算出整体包围球
	static bool ComputeSceneBounds(filament::Engine* engine,
		filament::math::float3& centerOut, float& radiusOut)
	{
		if (!engine) return false;
		filament::math::float3 minB{ std::numeric_limits<float>::max() };
		filament::math::float3 maxB{ -std::numeric_limits<float>::max() };
		bool hasContent = false;
		auto& tcm = engine->getTransformManager();
		auto& rcm = engine->getRenderableManager();
		for (utils::Entity e : g_domainEntities) {
			auto const ri = rcm.getInstance(e);
			if (!ri) continue;
			auto const& box = rcm.getAxisAlignedBoundingBox(ri);
			auto const ti = tcm.getInstance(e);
			const auto m = ti ? tcm.getWorldTransform(ti) : filament::math::mat4f();
			for (int sx : { -1, 1 }) {
				for (int sy : { -1, 1 }) {
					for (int sz : { -1, 1 }) {
						const auto corner = m * filament::math::float4(
							box.center.x + sx * box.halfExtent.x,
							box.center.y + sy * box.halfExtent.y,
							box.center.z + sz * box.halfExtent.z,
							1.0f);
						const filament::math::float3 w{ corner.x, corner.y, corner.z };
						minB = MinVec(minB, w);
						maxB = MaxVec(maxB, w);
					}
				}
			}
			hasContent = true;
		}
		if (!hasContent) return false;
		centerOut = (minB + maxB) * 0.5f;
		radiusOut = std::max(0.05f, length(maxB - minB) * 0.5f);
		return true;
	}

} // namespace

// ----------------------------------------------------------------------------
// 单例访问
// ----------------------------------------------------------------------------
FilamentApp& FilamentApp::instance() {
    static FilamentApp sInstance;
    return sInstance;
}

FilamentApp::FilamentApp(QObject* parent)
    : QObject(parent) {
}

// ----------------------------------------------------------------------------
// 引擎 / 场景初始化
// ----------------------------------------------------------------------------
bool FilamentApp::initialize(QOpenGLWidget* hostWidget, filament::backend::Platform* platform) {
    if (mEngine) return true;
    if (!hostWidget || !platform) return false;

    // Qt 的 QOpenGLWidget 在渲染间隙也会让上下文保持 current，
    // 而 wglShareLists 要求源上下文不被占用——先 doneCurrent 释放，
    // 否则驱动线程无法与 widget 上下文共享纹理（blit 结果全黑）。
    if (auto* ctx = hostWidget->context();
        ctx && QOpenGLContext::currentContext() == ctx) {
        ctx->doneCurrent();
    }

    filament::Engine::Config engineConfig = {};
    engineConfig.stereoscopicEyeCount = 2;
    engineConfig.stereoscopicType = filament::Engine::StereoscopicType::NONE;
    // 网格数量很大（数万个 Renderable/材质实例），默认 4MB 的
    // 后端句柄池会被撑爆并退回系统堆，这里调大到 64MB
    engineConfig.driverHandleArenaSizeMB = 256;

    mEngine = filament::Engine::Builder()
        .backend(filament::Engine::Backend::OPENGL)
        .featureLevel(filament::backend::FeatureLevel::FEATURE_LEVEL_3)
        .config(&engineConfig)
        .platform(platform)
        .build();
    if (!mEngine) return false;

    mRenderer = mEngine->createRenderer();
    mScene = mEngine->createScene();
    mView = mEngine->createView();
    mSwapChain = mEngine->createSwapChain(hostWidget, 0);
    mView->setName("Main View");
    mView->setPostProcessingEnabled(true);

    // 纯色天空盒（IBL 环境贴图暂未接入）
    auto skybox = filament::Skybox::Builder()
        .color({ 0.1f, 0.125f, 0.25f, 1.0f })
        .build(*mEngine);
    mScene->setSkybox(skybox);
    CORE_WARN("[IBL] 未找到环境贴图，使用纯色天空盒");

    mCameraManipulator = filament::camutils::Manipulator<float>::Builder()
        .targetPosition(0, 0, 0)
        .orbitSpeed(0.01f, -0.01f)   // 垂直方向与鼠标一致（默认相反）
        .zoomSpeed(0.1f)
        .flightMoveDamping(15.0)
        .build(filament::camutils::Mode::ORBIT);
    mCameraManipulator->setViewport(hostWidget->width(), hostWidget->height());

    mMaterial = filament::Material::Builder()
        .package(RESOURCES_AIDEFAULTMAT_DATA, RESOURCES_AIDEFAULTMAT_SIZE)
        .build(*mEngine);
    if (!mMaterial) return false;

    // 平行太阳光
    auto& em = utils::EntityManager::get();
    mLight = em.create();
    filament::LightManager::Builder(filament::LightManager::Type::SUN)
        .color(filament::Color::toLinear<filament::ACCURATE>(
            filament::sRGBColor(0.98f, 0.92f, 0.89f)))
        .intensity(110000)
        .direction({ 0.7f, -1.0f, -0.8f })
        .sunAngularRadius(1.9f)
        .castShadows(false)
        .build(*mEngine, mLight);
    mScene->addEntity(mLight);

    // 主相机
    utils::Entity cameraEntity = em.create();
    mCamera = mEngine->createCamera(cameraEntity);
    mView->setCamera(mCamera);

    mView->setScene(mScene);
    mView->setViewport({
        0, 0,
        (uint32_t)hostWidget->width(),
        (uint32_t)hostWidget->height()
    });

    // 若引擎创建前已通过菜单选择了文件，初始化完成后自动加载
    if (!mPendingFilePath.isEmpty()) {
        loadScene(mPendingFilePath);
    }
    return isReady();
}

// ----------------------------------------------------------------------------
// 场景网格加载 / 卸载
// ----------------------------------------------------------------------------
bool FilamentApp::loadScene(const QString& filePath) {
    mPendingFilePath = filePath;
    if (!mEngine || !mScene || !mMaterial) {
        // 引擎未就绪：只记住路径，initialize() 完成后会自动加载
        return false;
    }
    const std::string path = filePath.toLocal8Bit().constData();
    const size_t startIndex = g_domainEntities.size();  // 本次追加前的实体数
    const bool ok = LoadAndAddDomainMeshes(mEngine, mScene, mMaterial, path);
    if (ok) {
        mLastLoadedEntityStart = startIndex;
        emit sceneLoaded(filePath);
    }
    return ok;
}

void FilamentApp::clearScene() {
    if (mEngine && mScene) {
        RemoveAllDomainMeshes(mEngine, mScene);
        mLastLoadedEntityStart = 0;
        emit sceneCleared();
    }
}

bool FilamentApp::hasSceneContent() const {
    return !g_domainEntities.empty();
}

size_t FilamentApp::renderableCount() const {
    return g_domainEntities.size();
}

uint64_t FilamentApp::triangleCount() const {
    return g_totalTriangles;
}

size_t FilamentApp::entityCount() const {
    return g_domainEntities.size();
}

utils::Entity FilamentApp::entityAt(size_t index) const {
    return index < g_domainEntities.size() ? g_domainEntities[index] : utils::Entity();
}

bool FilamentApp::isEntityVisible(size_t index) const {
    return index < g_domainVisible.size() && g_domainVisible[index] != 0;
}

void FilamentApp::setEntityVisible(size_t index, bool visible) {
    if (!mScene || index >= g_domainEntities.size() || index >= g_domainVisible.size()) {
        return;
    }
    if (g_domainVisible[index] == (visible ? 1 : 0)) {
        return; // 状态没变化
    }
    g_domainVisible[index] = visible ? 1 : 0;
    if (visible) {
        mScene->addEntity(g_domainEntities[index]);
    } else {
        mScene->remove(g_domainEntities[index]);
    }
}

void FilamentApp::setAllEntitiesVisible(bool visible) {
    for (size_t i = 0; i < g_domainEntities.size(); i++) {
        setEntityVisible(i, visible);
    }
}

void FilamentApp::requestPick(int x, int y) {
    if (!mView) {
        emit pickedEntityChanged(-1);
        return;
    }

    const filament::Viewport viewport = mView->getViewport();
    const int w = (int)viewport.width;
    const int h = (int)viewport.height;
    if (w <= 0 || h <= 0) {
        emit pickedEntityChanged(-1);
        return;
    }

    // Qt 鼠标坐标左上角为原点，Filament pick 用 GL 约定（左下角为原点）
    const uint32_t px = (uint32_t)std::clamp(x, 0, w - 1);
    const uint32_t py = (uint32_t)std::clamp(h - y, 0, h - 1);

    // 记录查询时刻的相机矩阵与视口（pick 是异步的，回调可能晚几帧）
    mPickViewportWidth = viewport.width;
    mPickViewportHeight = viewport.height;
    if (mCamera) {
        mPickProjection = mCamera->getProjectionMatrix();
        mPickModel = mCamera->getModelMatrix();
    }

    FilamentApp* self = this;
    mView->pick(px, py, [self](filament::View::PickingQueryResult const& result) {
        const utils::Entity picked = result.renderable;
        const filament::math::float3 frag = result.fragCoords;
        // 回调可能来自驱动线程，把结果投递回 GUI 线程再处理
        QMetaObject::invokeMethod(self, [self, picked, frag]() {
            int entityIndex = -1;
            for (size_t i = 0; i < g_domainEntities.size(); i++) {
                if (g_domainEntities[i] == picked) {
                    entityIndex = (int)i;
                    break;
                }
            }

            // 根据查询时刻的相机矩阵 + 屏幕坐标重建世界位置
            float worldX = 0.0f, worldY = 0.0f, worldZ = 0.0f;
            if (entityIndex >= 0 && self->mPickViewportWidth > 0 &&
                self->mPickViewportHeight > 0) {
                using namespace filament::math;
                const double invW = 1.0 / double(self->mPickViewportWidth);
                const double invH = 1.0 / double(self->mPickViewportHeight);
                double4 const clip{
                    double(frag.x) * invW * 2.0 - 1.0,
                    double(frag.y) * invH * 2.0 - 1.0,
                    double(frag.z) * 2.0 - 1.0,
                    1.0
                };
                double4 view = inverse(self->mPickProjection) * clip;
                if (std::abs(view.w) > 1e-12) {
                    view /= view.w;
                }
                double4 const world = self->mPickModel * view;
                worldX = float(world.x);
                worldY = float(world.y);
                worldZ = float(world.z);
            }

            emit self->pickedEntityChanged(entityIndex);
            emit self->pickedWorldPosition(entityIndex, worldX, worldY, worldZ);
        }, Qt::QueuedConnection);
    });
}

void FilamentApp::setOrbitCenter(float worldX, float worldY, float worldZ) {
    if (!mCameraManipulator) return;

    // camutils 的 Bookmark 字段不对外，无法原地改 pivot；
    // 用当前眼睛位置重建一个 target=点击点的 ORBIT 操纵器，视觉上保持不动，
    // 旋转时即以点击点为中心。
    filament::math::float3 eye, oldCenter, up;
    mCameraManipulator->getLookAt(&eye, &oldCenter, &up);
    delete mCameraManipulator;

    auto* manipulator = filament::camutils::Manipulator<float>::Builder()
        .targetPosition(worldX, worldY, worldZ)
        .orbitHomePosition(eye.x, eye.y, eye.z)
        .upVector(up.x, up.y, up.z)
        .orbitSpeed(0.01f, -0.01f)   // 与初始相机一致：垂直方向反转
        .zoomSpeed(0.1f)
        .flightMoveDamping(15.0)
        .build(filament::camutils::Mode::ORBIT);
    if (mView) {
        const filament::Viewport viewport = mView->getViewport();
        manipulator->setViewport((int)viewport.width, (int)viewport.height);
    }
    mCameraManipulator = manipulator;
}

void FilamentApp::fitCameraToScene() {
    if (!mEngine || !mCameraManipulator || !mView) return;

    // 保持当前视角方向，只把相机放到能框住场景的距离
    filament::math::float3 eye, target, up;
    mCameraManipulator->getLookAt(&eye, &target, &up);
    const float dirLen = length(target - eye);
    const filament::math::float3 forward =
        dirLen > 1e-6f ? normalize(target - eye)
                       : filament::math::float3{ 0, 0, -1 };
    // forward 指向目标，相机在目标反方向：fromDir = -forward
    lookFromDirection(
        -forward.x, -forward.y, -forward.z,
        up.x, up.y, up.z);
}

void FilamentApp::lookFromDirection(
    float fromX, float fromY, float fromZ,
    float upX, float upY, float upZ) {
    if (!mEngine || !mCameraManipulator || !mView) return;

    filament::math::float3 center;
    float radius = 0.0f;
    if (!ComputeSceneBounds(mEngine, center, radius)) return;

    const float dist = radius / 0.4142f * 1.15f;   // tan(22.5°) + 边距
    const filament::math::float3 from = normalize(
        filament::math::float3{ fromX, fromY, fromZ });
    const filament::math::float3 newEye = center + from * dist;

    delete mCameraManipulator;
    auto* manipulator = filament::camutils::Manipulator<float>::Builder()
        .targetPosition(center.x, center.y, center.z)
        .orbitHomePosition(newEye.x, newEye.y, newEye.z)
        .upVector(upX, upY, upZ)
        .orbitSpeed(0.01f, -0.01f)
        .zoomSpeed(0.1f)
        .flightMoveDamping(15.0)
        .build(filament::camutils::Mode::ORBIT);
    if (mView) {
        const filament::Viewport viewport = mView->getViewport();
        manipulator->setViewport((int)viewport.width, (int)viewport.height);
    }
    mCameraManipulator = manipulator;

    if (mProjectionMode == ProjectionMode::Orthographic) {
        // 正交：让视口高度直接等于包围球直径 + 边距
        const float desiredHalf = radius * 1.15f;
        mOrthoZoomScale = desiredHalf / std::max(0.01f, dist * 0.4142f);
    }
    updateCameraProjection();
}

void FilamentApp::setProjectionMode(ProjectionMode mode) {
    if (mProjectionMode == mode) return;
    mProjectionMode = mode;
    updateCameraProjection();
}

void FilamentApp::adjustOrthoZoom(float scrollDelta) {
    if (scrollDelta == 0.0f) return;
    // 上滚（scrollDelta<0）拉近：缩小正交高度；下滚拉远：放大
    mOrthoZoomScale *= std::pow(0.8f, -scrollDelta);
    mOrthoZoomScale = std::clamp(mOrthoZoomScale, 0.001f, 10000.0f);
    updateCameraProjection();
}

void FilamentApp::updateCameraProjection() {
    if (!mCamera || !mView) return;

    const filament::Viewport viewport = mView->getViewport();
    const float aspect = viewport.height > 0
        ? float(viewport.width) / float(viewport.height)
        : 1.0f;

    if (mProjectionMode == ProjectionMode::Orthographic) {
        // 正交视口高度取“相机到目标距离 × tan(半 FOV)”，使切换前后目标大小接近；
        // 宽度随视口宽高比自适应。
        float halfHeight = 1.0f;
        if (mCameraManipulator) {
            filament::math::float3 eye, target, up;
            mCameraManipulator->getLookAt(&eye, &target, &up);
            const float dist = length(eye - target);   // ADL: math vec3 的 length
            halfHeight = std::max(0.01f,
                dist * 0.414f * mOrthoZoomScale);      // tan(22.5°) × 缩放
        }
        const float halfWidth = halfHeight * aspect;
        mCamera->setProjection(
            filament::Camera::Projection::ORTHO,
            -halfWidth, halfWidth,
            -halfHeight, halfHeight,
            0.1, 1000.0);
    } else {
        mCamera->setProjection(45.0, aspect, 0.1, 1000.0);
    }
}

void FilamentApp::clearPickedEntity() {
    if (g_highlightedEntity != kNoHighlight) {
        setEntityHighlighted(g_highlightedEntity, false);
    }
    emit pickedEntityChanged(-1);
}

void FilamentApp::setEntityHighlighted(size_t index, bool highlighted) {
    if (index >= g_domainEntities.size() ||
        index >= g_domainMaterialInstances.size() ||
        index >= g_domainBaseColors.size()) {
        return;
    }

    if (highlighted) {
        if (g_highlightedEntity == index) return;
        // 只允许一个悬浮高亮：先恢复之前高亮的实体
        if (g_highlightedEntity != kNoHighlight) {
            setEntityHighlighted(g_highlightedEntity, false);
        }
        const filament::math::float3 gold = filament::Color::toLinear<filament::ACCURATE>(
            filament::sRGBColor(1.0f, 0.843f, 0.0f));   // #FFD700
        g_domainMaterialInstances[index]->setParameter(
            "baseColor", filament::RgbType::LINEAR, gold);
        g_highlightedEntity = index;
    } else {
        if (g_highlightedEntity != index) return;
        g_domainMaterialInstances[index]->setParameter(
            "baseColor", filament::RgbType::LINEAR, g_domainBaseColors[index]);
        g_highlightedEntity = kNoHighlight;
    }
}

} // namespace MOON

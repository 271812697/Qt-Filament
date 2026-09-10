#pragma once

#include <QObject>
#include <QString>
#include <math/mat4.h>
#include <math/vec3.h>
#include <utils/Entity.h>

class QOpenGLWidget;

namespace filament {
	class Camera;
	class ColorGrading;
	class Engine;
	class Material;
	class Renderer;
	class Scene;
	class SwapChain;
	class View;

	namespace backend {
		class Platform;
	}

	namespace camutils {
		template<typename T> class Manipulator;
	}
}

namespace MOON {

	// 全局 Filament 应用：负责引擎创建 / 场景初始化 / 网格加载与卸载。
	// ViewerWidget 只做薄封装：
	//   FilamentApp::instance().initialize(hostWidget, platform);
	//   FilamentApp::instance().loadScene(filePath);
	class FilamentApp : public QObject {
		Q_OBJECT
	public:
		enum class ProjectionMode {
			Perspective,
			Orthographic,
		};

		static FilamentApp& instance();

	signals:
		// 引擎与场景初始化完成（UI 可以开始读/写 View 设置）
		void engineInitialized();
		// 场景网格加载/卸载完成（TreeViewPanel 等界面据此刷新）
		void sceneLoaded(const QString& filePath);
		void sceneCleared();
		// View::pick 结果：命中的实体下标，-1 表示没有命中
		void pickedEntityChanged(int entityIndex);
		// View::pick 结果附带的世界坐标（命中实体时有效；-1 时坐标无意义）
		void pickedWorldPosition(int entityIndex, float worldX, float worldY, float worldZ);
		// 当前在 TreeView 中选中的实体（-1 表示无）
		void selectedEntityChanged(int entityIndex);

	public:
		// 创建 Engine / Renderer / Scene / View / SwapChain / Camera / Light / Material，
		// 并初始化默认相机操纵器。幂等，可重复调用。
		bool initialize(QOpenGLWidget* hostWidget, filament::backend::Platform* platform);

		// 加载 SaveDomains 网格文件并替换当前场景内容。
		// 引擎未就绪时仅记住路径，initialize() 完成后会自动加载。
		bool loadScene(const QString& filePath);

		// 卸载当前场景中的 domain 网格（实体与 GPU 资源）。
		void clearScene();

		// ---- 只读接口（其它模块访问 Filament 对象） ----
		filament::Engine* engine() const { return mEngine; }
		filament::Renderer* renderer() const { return mRenderer; }
		filament::Scene* scene() const { return mScene; }
		filament::View* view() const { return mView; }
		filament::SwapChain* swapChain() const { return mSwapChain; }
		filament::Camera* camera() const { return mCamera; }
		filament::Material* material() const { return mMaterial; }
		utils::Entity light() const { return mLight; }
		filament::camutils::Manipulator<float>* cameraManipulator() const {
			return mCameraManipulator;
		}

		bool isReady() const {
			return mEngine && mRenderer && mScene && mView && mSwapChain && mCamera;
		}

		// 场景是否加载了 domain 网格
		bool hasSceneContent() const;
		size_t renderableCount() const;
		uint64_t triangleCount() const;

		// ---- 实体列表与可见性（TreeViewPanel 使用） ----
		size_t entityCount() const;
		utils::Entity entityAt(size_t index) const;
		bool isEntityVisible(size_t index) const;
		void setEntityVisible(size_t index, bool visible);
		void setAllEntitiesVisible(bool visible);
		// 场景实体高亮（TreeView 悬浮时变金黄，移开后恢复原色）
		void setEntityHighlighted(size_t index, bool highlighted);
		// 3D 视口拾取：x/y 为 Qt 左上角逻辑坐标，结果通过 pickedEntityChanged 异步回调
		void requestPick(int x, int y);
		// 清除当前拾取高亮并广播 -1（视口鼠标移出等场景使用）
		void clearPickedEntity();
		// 以命中点作为相机 orbit 旋转中心（保持当前距离与视角方向）
		void setOrbitCenter(float worldX, float worldY, float worldZ);
		// 相机 fit：计算所有已加载实体包围球，把相机拉/缩到刚好框住内容
		void fitCameraToScene();
		// 从指定方向看场景中心（fromDir 为“场景中心 → 相机”的方向）
		void lookFromDirection(float fromX, float fromY, float fromZ,
			float upX, float upY, float upZ);
		// 切换/查询相机投影模式（透视/正交）
		ProjectionMode projectionMode() const { return mProjectionMode; }
		void setProjectionMode(ProjectionMode mode);
		// 正交模式下滚轮缩放（scrollDelta：-1=上滚/拉近，+1=下滚/拉远）
		void adjustOrthoZoom(float scrollDelta);
		// 默认 ColorGrading 开关（开启时创建一个默认颜色分级对象）
		void setColorGradingEnabled(bool enabled);
		// 按当前视口宽高比应用投影（每帧由渲染循环调用）
		void updateCameraProjection();
		// 最近一次 loadScene 追加的实体在全局实体列表里的起始下标
		// （新加载是追加，不删除旧 mesh；TreeView 用它只新增本批节点）
		size_t lastLoadedEntityStart() const { return mLastLoadedEntityStart; }

		// ---- 实体选择（TreeView / PropertyPanel 联动） ----
		int selectedEntity() const { return mSelectedEntity; }
		void setSelectedEntity(int entityIndex);

		// ---- 实体 Transform（世界变换，旋转为角度制欧拉角） ----
		filament::math::float3 entityTranslation(size_t index) const;
		filament::math::float3 entityRotationEuler(size_t index) const;
		filament::math::float3 entityScale(size_t index) const;
		void setEntityTranslation(size_t index, filament::math::float3 value);
		void setEntityRotationEuler(size_t index, filament::math::float3 value);
		void setEntityScale(size_t index, filament::math::float3 value);

		// ---- 实体材质原色（PropertyPanel 用） ----
		filament::math::float3 entityBaseColor(size_t index) const;
		void setEntityBaseColor(size_t index, filament::math::float3 value);

		// ---- Renderable 组件状态（Filament 部分没有 getter，这里做缓存） ----
		bool entityCastShadows(size_t index) const;
		bool entityReceiveShadows(size_t index) const;
		bool entityCulling(size_t index) const;
		uint8_t entityPriority(size_t index) const;
		void setEntityCastShadows(size_t index, bool value);
		void setEntityReceiveShadows(size_t index, bool value);
		void setEntityCulling(size_t index, bool value);
		void setEntityPriority(size_t index, uint8_t value);

	private:
		explicit FilamentApp(QObject* parent = nullptr);
		~FilamentApp() = default;
		FilamentApp(FilamentApp const&) = delete;
		FilamentApp& operator=(FilamentApp const&) = delete;

		filament::Engine* mEngine = nullptr;
		filament::Renderer* mRenderer = nullptr;
		filament::Scene* mScene = nullptr;
		filament::View* mView = nullptr;
		filament::SwapChain* mSwapChain = nullptr;
		filament::Camera* mCamera = nullptr;
		filament::ColorGrading* mColorGrading = nullptr;
		filament::Material* mMaterial = nullptr;
		utils::Entity mLight;
		filament::camutils::Manipulator<float>* mCameraManipulator = nullptr;
		QString mPendingFilePath; // 引擎就绪前选中的文件
		size_t mLastLoadedEntityStart = 0;
		// 拾取查询发出时的相机状态（pick 异步，回调里需要查询时刻的矩阵）
		filament::math::mat4 mPickProjection;
		filament::math::mat4 mPickModel;
		uint32_t mPickViewportWidth = 0;
		uint32_t mPickViewportHeight = 0;
		ProjectionMode mProjectionMode = ProjectionMode::Perspective;
		float mOrthoZoomScale = 1.0f;   // 正交视口缩放系数
		int mSelectedEntity = -1;
	};

} // namespace MOON

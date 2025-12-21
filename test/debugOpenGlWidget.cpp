#include "debugOpenGlWidget.h"
#include "glloader.h"

#include <iostream>

#include <filament/Camera.h>
#include <filament/Engine.h>
//#include <backend/filament.h>
#include <filament/IndexBuffer.h>
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
#include <utils/EntityManager.h>
#include <filameshio/MeshReader.h>
#include "filament/LightManager.h"
#include "../build/filament/samples/generated/resources/monkey.h"
#include "../build/filament/samples/generated/resources/resources.h"

#include "PlatformGlfwGL.h"
#include <camutils/Manipulator.h>
#include <QWindow>
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
	HWND getOpenGLWidgetHWND(QOpenGLWidget* glWidget) {
		if (!glWidget) {
			return nullptr;
		}

		// 方式1：通过 QWidget::windowHandle() 获取 QWindow，再转 HWND（推荐）
		QWindow* window = glWidget->windowHandle();
		if (window) {
			// winId() 返回的是平台原生窗口句柄（Windows 下即为 HWND）
			return reinterpret_cast<HWND>(window->winId());
		}

		// 方式2：兼容旧版 Qt/嵌入场景（直接获取 QWidget 原生句柄）
		WId wid = glWidget->winId();
		return reinterpret_cast<HWND>(wid);
	}
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
	DebugOpenGLWidget::DebugOpenGLWidget(QWidget* parent) {
		this->setFocusPolicy(Qt::StrongFocus);
		this->setMouseTracking(true);
		QSurfaceFormat format;
		format.setSamples(4);
		this->setFormat(format);
		
	}
	DebugOpenGLWidget::~DebugOpenGLWidget() {

	}
	void DebugOpenGLWidget::initializeGL() {
		
		QOpenGLWidget::initializeGL();
		// opengl funcs
		bool flag = initializeOpenGLFunctions();
		OpenGLProcAddressHelper::ctx = context();
		//CUSTOM_GL_API::CustomLoadGL(OpenGLProcAddressHelper::getProcAddress);
		GlLoader::CustomLoadGL(OpenGLProcAddressHelper::getProcAddress);

		//开启计时器
		this->startTimer(0);
		
		filament::backend::PlatformGlfwGL* platform = new filament::backend::PlatformGlfwGL();
		platform->glwidget = this;
		auto createEngine = [&]() {
			auto backend = filament::Engine::Backend::OPENGL;
			filament::Engine::Config engineConfig = {};
			engineConfig.stereoscopicEyeCount = 2;
			engineConfig.stereoscopicType = filament::Engine::StereoscopicType::NONE;
			return filament::Engine::Builder()
				.backend(backend)
				.featureLevel(filament::backend::FeatureLevel::FEATURE_LEVEL_3)
				.config(&engineConfig).platform(platform)
				.build();
			};
		engine = createEngine();
		renderer = engine->createRenderer();
		scene = engine->createScene();
		view = engine->createView();
		swapchain = engine->createSwapChain(this, 0);
		view->setName("Main View");

		auto setup = [&]() {
			auto skybox = filament::Skybox::Builder().color({ 0.1, 0.125, 0.25, 1.0 }).build(*engine);
			scene->setSkybox(skybox);
			view->setPostProcessingEnabled(true);
			auto& tcm = engine->getTransformManager();
			auto& rcm = engine->getRenderableManager();
			auto& em = utils::EntityManager::get();
			app.mMainCameraMan = filament::camutils::Manipulator<float>::Builder()
				.targetPosition(0, 0, 0)
				.flightMoveDamping(15.0)
				.build(filament::camutils::Mode::ORBIT);
			app.material = filament::Material::Builder()
				.package(RESOURCES_AIDEFAULTMAT_DATA, RESOURCES_AIDEFAULTMAT_SIZE).build(*engine);
			auto mi = app.materialInstance = app.material->createInstance();
			mi->setParameter("baseColor", filament::RgbType::LINEAR, filament::math::float3{ 0.8 });
			mi->setParameter("metallic", 1.0f);
			mi->setParameter("roughness", 0.4f);
			mi->setParameter("reflectance", 0.5f);
			app.mesh = filamesh::MeshReader::loadMeshFromBuffer(engine, MONKEY_SUZANNE_DATA, nullptr, nullptr, mi);
			auto ti = tcm.getInstance(app.mesh.renderable);
			app.transform = filament::math::mat4f{ filament::math::mat3f(1), filament::math::float3(0, 0, -4) } *tcm.getWorldTransform(ti);
			rcm.setCastShadows(rcm.getInstance(app.mesh.renderable), false);
			scene->addEntity(app.mesh.renderable);

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

			};
		setup();
		view->setScene(scene);
		view->setViewport({ 0,0,1280,800 });
	}
	void DebugOpenGLWidget::timerEvent(QTimerEvent* e) {
		this->update();
	}
	void animate (filament::Engine* engine, filament::View* view, double now) {

		auto& tcm = engine->getTransformManager();
		auto ti = tcm.getInstance(app.mesh.renderable);
		tcm.setTransform(ti, app.transform * filament::math::mat4f::rotation(now, filament::math::float3{ 0, 1, 0 }));
		filament::math::float3 eye, center, up;
		app.mMainCameraMan->getLookAt(&eye, &center, &up);
		app.cam->lookAt(eye, center, up);
		constexpr float ZOOM = 1.5f;
		const uint32_t w = view->getViewport().width;
		const uint32_t h = view->getViewport().height;
		const float aspect = (float)w / h;
		app.cam->setProjection(filament::Camera::Projection::ORTHO,
			-aspect * ZOOM, aspect * ZOOM,
			-ZOOM, ZOOM, 0, 1000);
		};
	void DebugOpenGLWidget::paintGL() {
		glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
		//glClearColor(1,0,0,1);
		
		static double t = 0.0f;
		t += 0.0016;
		animate(engine, view, t);
		bool flag = renderer->beginFrame(swapchain);
		renderer->render(view);
		
		renderer->endFrame();
		engine->execute();
		//SwapBuffers();
		//doneCurrent();
	}
	bool DebugOpenGLWidget::event(QEvent* evt) {
		return QOpenGLWidget::event(evt);
	}
	void DebugOpenGLWidget::leaveEvent(QEvent* event) {

	}

	void DebugOpenGLWidget::resizeEvent(QResizeEvent* event) {
		QOpenGLWidget::resizeEvent(event);
	}
	void DebugOpenGLWidget::mousePressEvent(QMouseEvent* event) {

	}

	void DebugOpenGLWidget::mouseMoveEvent(QMouseEvent* event) {

	}

	void DebugOpenGLWidget::mouseReleaseEvent(QMouseEvent* event) {

	}

	void DebugOpenGLWidget::wheelEvent(QWheelEvent* event) {

	}
	void DebugOpenGLWidget::keyPressEvent(QKeyEvent* event) {

	}
	void DebugOpenGLWidget::keyReleaseEvent(QKeyEvent* event) {

	}

	void DebugOpenGLWidget::showImGui() {

	}
}




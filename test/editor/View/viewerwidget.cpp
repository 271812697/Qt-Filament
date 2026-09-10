#include "viewerwidget.h"
#include "glloader.h"

#include "core/log.h"
#include "core/FilamentApp.h"

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

namespace MOON {

	struct OpenGLProcAddressHelper {
		inline static QOpenGLContext* ctx;
		static void* getProcAddress(const char* name) {
			return (void*)ctx->getProcAddress(name);
		}
	};


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

		// 视口悬浮拾取结果（Filament View::pick 异步回调）
		connect(&FilamentApp::instance(), &FilamentApp::pickedEntityChanged,
			this, &ViewerWidget::onPickProcessed);
		connect(&FilamentApp::instance(), &FilamentApp::pickedWorldPosition,
			this, &ViewerWidget::onPickedWorldPosition);
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
		// 引擎创建与场景初始化全部在 FilamentApp 内部完成，
		// ViewerWidget 只提供 GL 上下文宿主与自定义 Platform。
		FilamentApp::instance().initialize(this, mPlatform);
	}
	void ViewerWidget::onReadFile(const QString& filePath) {
		// 文件解析 / 网格合并 / 场景替换全部在 FilamentApp::loadScene 内完成
		FilamentApp::instance().loadScene(filePath);
	}
	void ViewerWidget::timerEvent(QTimerEvent* e) {
		this->update();
	}
	void animate(double now, double deltaTime) {
		FilamentApp& app = FilamentApp::instance();
		filament::camutils::Manipulator<float>* man = app.cameraManipulator();
		filament::Camera* cam = app.camera();
		filament::View* view = app.view();
		if (!man || !cam || !view) return;
		man->update(deltaTime);
		filament::math::float3 eye, center, up;
		man->getLookAt(&eye, &center, &up);
		cam->lookAt(eye, center, up);
		// 投影由 FilamentApp 按当前模式（透视/正交）统一设置
		app.updateCameraProjection();
	};
	void ViewerWidget::paintGL() {
		FilamentApp& app = FilamentApp::instance();
		filament::Engine* engine = app.engine();
		filament::View* view = app.view();
		filament::Renderer* renderer = app.renderer();
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
		if (app.hasSceneContent()) {
			static int frameCount = 0;
			static double lastFpsTime = 0.0;
			frameCount++;
			if (now - lastFpsTime >= 1.0) {
				CORE_INFO("[FPS] {} fps, renderables(draw calls)={}, triangles={}",
					frameCount / (now - lastFpsTime),
					app.renderableCount(), app.triangleCount());

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

		animate(now, delta);
		bool submitted = false;
		if (renderer->beginFrame(app.swapChain())) {
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
			FilamentApp::instance().cameraManipulator()->grabEnd();
			mGrabbing = false;
		}

		// 离开视口：清掉悬浮拾取高亮；有在途查询时等结果回来再清
		mPickInside = false;
		mPickQueued = false;
		const bool hadActive = mPickActiveEntity >= 0;
		mPickActiveEntity = -1;
		if (!mPickInFlight && hadActive) {
			FilamentApp::instance().clearPickedEntity();
		}
	}

	void ViewerWidget::resizeEvent(QResizeEvent* event) {
		unsigned int viewW = event->size().width();
		unsigned int viewH = event->size().height();
		QOpenGLWidget::resizeEvent(event);
		FilamentApp& app = FilamentApp::instance();
		filament::View* view = app.view();
		if (view) {
			view->setViewport({ 0,0,viewW,viewH });
		}
		if (auto* man = app.cameraManipulator()) {
			man->setViewport((int)viewW, (int)viewH);
		}
		if (mPlatform) {
			mPlatform->setTargetSize(viewW, viewH); // 驱动线程下次 makeCurrent 时重建离屏缓冲
		}
	}
	void ViewerWidget::mousePressEvent(QMouseEvent* event) {
		mLastMousePos = event->pos();
		if (event->button() == Qt::LeftButton) {
			// 左键：先拾取点击点世界坐标，作为 orbit 旋转中心再开始拖拽
			mLeftDown = true;
			mOrbitCenterPending = true;
			mPickInside = true;
			mPickPos = event->pos();
			mPickQueued = true;
			issuePickIfIdle();

			// 若点击空白处/拾取迟迟不返回，250ms 后用当前中心兜底开始旋转
			QTimer::singleShot(250, this, [this]() {
				if (mOrbitCenterPending && mLeftDown && !mGrabbing) {
					beginOrbitGrab();
				}
			});
			event->accept();
			return;
		}

		if (event->button() == Qt::MiddleButton ||
			event->button() == Qt::RightButton) {
			const bool strafe = (event->button() != Qt::LeftButton);
			if (auto* man = FilamentApp::instance().cameraManipulator()) {
				man->grabBegin(event->pos().x(), event->pos().y(), strafe);
			}
			mGrabbing = true;
			event->accept();
		}
	}

	void ViewerWidget::mouseMoveEvent(QMouseEvent* event) {
		mLastMousePos = event->pos();
		if (mGrabbing) {
			FilamentApp::instance().cameraManipulator()
				->grabUpdate(event->pos().x(), event->pos().y());
			event->accept();
			return;
		}

		// 悬浮拾取：记录最新位置，空闲时向 Filament 发 pick 查询
		mPickInside = true;
		mPickPos = event->pos();
		mPickQueued = true;
		issuePickIfIdle();
		event->accept();
	}

	void ViewerWidget::mouseReleaseEvent(QMouseEvent* event) {
		mLastMousePos = event->pos();
		if (event->button() == Qt::LeftButton) {
			mLeftDown = false;
			mOrbitCenterPending = false;
		}
		if (mGrabbing) {
			FilamentApp::instance().cameraManipulator()->grabEnd();
			mGrabbing = false;
		}
	}

	void ViewerWidget::onPickedWorldPosition(int entityIndex,
		float worldX, float worldY, float worldZ) {
		if (!mOrbitCenterPending || !mLeftDown) return;
		FilamentApp& app = FilamentApp::instance();
		if (entityIndex >= 0) {
			// 命中网格：把旋转中心设到点击的世界位置
			app.setOrbitCenter(worldX, worldY, worldZ);
		}
		beginOrbitGrab();
	}

	void ViewerWidget::beginOrbitGrab() {
		mOrbitCenterPending = false;
		FilamentApp& app = FilamentApp::instance();
		// 开始旋转前清掉悬浮高亮，避免拖拽时残留金黄色
		if (mPickActiveEntity >= 0) {
			mPickActiveEntity = -1;
			app.clearPickedEntity();
		}
		if (auto* man = app.cameraManipulator()) {
			man->grabBegin(mLastMousePos.x(), mLastMousePos.y(), false);
			mGrabbing = true;
		}
	}

	void ViewerWidget::onPickProcessed(int entityIndex) {
		FilamentApp& app = FilamentApp::instance();
		if (mPickInside) {
			if (entityIndex != mPickActiveEntity) {
				if (mPickActiveEntity >= 0) {
					app.setEntityHighlighted((size_t)mPickActiveEntity, false);
				}
				if (entityIndex >= 0) {
					app.setEntityHighlighted((size_t)entityIndex, true);
				}
				mPickActiveEntity = entityIndex;
			}
		} else if (mPickActiveEntity >= 0) {
			// 查询期间鼠标已离开视口，恢复旧实体颜色
			mPickActiveEntity = -1;
			app.clearPickedEntity();
		}

		mPickInFlight = false;
		issuePickIfIdle();
	}

	void ViewerWidget::issuePickIfIdle() {
		if (!mPickInside || !mPickQueued || mPickInFlight) return;
		mPickQueued = false;
		mPickInFlight = true;
		FilamentApp::instance().requestPick(mPickPos.x(), mPickPos.y());
	}

	void ViewerWidget::wheelEvent(QWheelEvent* event) {
		QPoint delta = event->angleDelta();
		if (delta.isNull()) {
			delta = event->pixelDelta();
		}
		const float scrollDelta = -delta.y() / 120.0f; // 向上滚 = 拉近
		if (scrollDelta != 0.0f) {
			FilamentApp& app = FilamentApp::instance();
			if (app.projectionMode() == FilamentApp::ProjectionMode::Orthographic) {
				// 正交投影：滚轮直接缩放正交视口高度
				app.adjustOrthoZoom(scrollDelta);
			} else {
				app.cameraManipulator()
					->scroll(event->pos().x(), event->pos().y(), scrollDelta);
			}
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




#include "viewerwidget.h"
#include "glloader.h"

#include "core/log.h"
#include "core/FilamentApp.h"

#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/Renderer.h>
#include <filament/View.h>
#include <filament/Viewport.h>

#include "PlatformGlfwGL.h"
#include <camutils/Manipulator.h>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace MOON {

	struct OpenGLProcAddressHelper {
		inline static QOpenGLContext* ctx;
		static void* getProcAddress(const char* name) {
			return (void*)ctx->getProcAddress(name);
		}
	};

	ViewerWidget::ViewerWidget(QWidget* parent) : QOpenGLWidget(parent) {
		this->setFocusPolicy(Qt::StrongFocus);
		this->setMouseTracking(true);
		QSurfaceFormat format;
		format.setSamples(0); // 离屏渲染 + blit 方案要求默认 FBO 单采样
		this->setFormat(format);
	}

	ViewerWidget::~ViewerWidget() = default;

	int ViewerWidget::renderWidth() const {
		return std::max(1, (int)std::lround(width() * devicePixelRatioF()));
	}

	int ViewerWidget::renderHeight() const {
		return std::max(1, (int)std::lround(height() * devicePixelRatioF()));
	}

	void ViewerWidget::initializeGL() {
		QOpenGLWidget::initializeGL();
		bool flag = initializeOpenGLFunctions();
		Q_UNUSED(flag);
		OpenGLProcAddressHelper::ctx = context();
		GlLoader::CustomLoadGL(OpenGLProcAddressHelper::getProcAddress);

		// 开启计时器
		this->startTimer(0);
		mElapsed.start();
		mLastFrameTime = -1.0;
		glGenFramebuffers(1, &mBlitFbo); // GUI 线程用于把共享纹理 blit 到 Qt FBO

		filament::backend::PlatformGlfwGL* platform = new filament::backend::PlatformGlfwGL();
		platform->glwidget = this;
		platform->setSharedContext(context());
		platform->setTargetSize((uint32_t)renderWidth(), (uint32_t)renderHeight());
		mPlatform = platform;

		// 延迟到事件循环空闲时创建引擎：此时 Qt widget 的 GL 上下文不在 current 状态，
		// wglShareLists 才能成功建立共享（否则 ERROR_BUSY，纹理无法共享 → blit 黑屏）
		QTimer::singleShot(0, this, [this]() { createEngineAndSetup(); });
	}

	void ViewerWidget::createEngineAndSetup() {
		// 引擎创建与预览场景初始化全部在 FilamentApp 内部完成，
		// ViewerWidget 只提供 GL 上下文宿主与自定义 Platform。
		if (!FilamentApp::instance().initialize(this, mPlatform)) {
			CORE_ERROR("[Viewer] Filament 引擎 / 材质预览初始化失败");
		}
		this->update();
	}

	void animate(double now, double deltaTime) {
		Q_UNUSED(now);
		FilamentApp& app = FilamentApp::instance();
		if (!app.isReady()) {
			return;
		}
		filament::camutils::Manipulator<float>* man = app.cameraManipulator();
		filament::Camera* cam = app.camera();
		if (!man || !cam) {
			return;
		}
		man->update(deltaTime);
		filament::math::float3 eye, center, up;
		man->getLookAt(&eye, &center, &up);
		cam->lookAt(eye, center, up);
		app.updateCameraProjection();
	}

	void ViewerWidget::paintGL() {
		FilamentApp& app = FilamentApp::instance();
		filament::Engine* engine = app.engine();
		filament::View* view = app.view();
		filament::Renderer* renderer = app.renderer();
		if (!engine || !view || !renderer) {
			glClearColor(0.055f, 0.060f, 0.070f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			return;
		}

		const double now = mElapsed.elapsed() / 1000.0;
		const double delta = mLastFrameTime < 0.0 ? 0.0 : now - mLastFrameTime;
		mLastFrameTime = now;

		// 每秒打印一次帧率与 draw call 规模（性能观察用）
		if (app.hasSceneContent()) {
			static int frameCount = 0;
			static double lastFpsTime = 0.0;
			frameCount++;
			if (now - lastFpsTime >= 1.0) {
				CORE_INFO("[FPS] {} fps, renderables(draw calls)={}, triangles={}",
					frameCount / (now - lastFpsTime),
					app.renderableCount(), app.triangleCount());

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
			const GLsizei w = (GLsizei)renderWidth();
			const GLsizei h = (GLsizei)renderHeight();
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
			if (auto* man = FilamentApp::instance().cameraManipulator()) {
				man->grabEnd();
			}
			mGrabbing = false;
		}
		QOpenGLWidget::leaveEvent(event);
	}

	void ViewerWidget::resizeEvent(QResizeEvent* event) {
		QOpenGLWidget::resizeEvent(event);

		const unsigned int viewW = (unsigned int)renderWidth();
		const unsigned int viewH = (unsigned int)renderHeight();
		FilamentApp& app = FilamentApp::instance();
		if (filament::View* view = app.view()) {
			view->setViewport({ 0, 0, viewW, viewH });
		}
		if (auto* man = app.cameraManipulator()) {
			man->setViewport((int)viewW, (int)viewH);
		}
		if (mPlatform) {
			mPlatform->setTargetSize(viewW, viewH); // 驱动线程下次 makeCurrent 时重建离屏缓冲
		}
		app.updateCameraProjection();
	}

	void ViewerWidget::mousePressEvent(QMouseEvent* event) {
		mLastMousePos = event->pos();
		if (event->button() == Qt::LeftButton ||
			event->button() == Qt::MiddleButton ||
			event->button() == Qt::RightButton) {
			// 左键绕材质球旋转；中/右键平移（strafe）
			const bool strafe = (event->button() != Qt::LeftButton);
			if (auto* man = FilamentApp::instance().cameraManipulator()) {
				man->grabBegin(event->pos().x(), event->pos().y(), strafe);
				mGrabbing = true;
			}
			event->accept();
			return;
		}
		QOpenGLWidget::mousePressEvent(event);
	}

	void ViewerWidget::mouseMoveEvent(QMouseEvent* event) {
		mLastMousePos = event->pos();
		if (mGrabbing) {
			if (auto* man = FilamentApp::instance().cameraManipulator()) {
				man->grabUpdate(event->pos().x(), event->pos().y());
			}
			event->accept();
			return;
		}
		QOpenGLWidget::mouseMoveEvent(event);
	}

	void ViewerWidget::mouseReleaseEvent(QMouseEvent* event) {
		mLastMousePos = event->pos();
		if (mGrabbing) {
			if (auto* man = FilamentApp::instance().cameraManipulator()) {
				man->grabEnd();
			}
			mGrabbing = false;
			event->accept();
			return;
		}
		QOpenGLWidget::mouseReleaseEvent(event);
	}

	void ViewerWidget::mouseDoubleClickEvent(QMouseEvent* event) {
		if (event->button() == Qt::LeftButton) {
			FilamentApp::instance().resetPreviewCamera();
			event->accept();
			return;
		}
		QOpenGLWidget::mouseDoubleClickEvent(event);
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
			}
			else if (auto* man = app.cameraManipulator()) {
				man->scroll(event->pos().x(), event->pos().y(), scrollDelta);
			}
			event->accept();
			return;
		}
		QOpenGLWidget::wheelEvent(event);
	}

	void ViewerWidget::keyPressEvent(QKeyEvent* event) {
		FilamentApp& app = FilamentApp::instance();
		if (event->key() == Qt::Key_F) {
			app.fitCameraToScene();
			event->accept();
			return;
		}
		if (event->key() == Qt::Key_R) {
			app.resetPreviewCamera();
			event->accept();
			return;
		}
		QOpenGLWidget::keyPressEvent(event);
	}

	void ViewerWidget::keyReleaseEvent(QKeyEvent* event) {
		QOpenGLWidget::keyReleaseEvent(event);
	}

	void ViewerWidget::timerEvent(QTimerEvent* e) {
		Q_UNUSED(e);
		this->update();
	}

	void ViewerWidget::showImGui() {
	}
}

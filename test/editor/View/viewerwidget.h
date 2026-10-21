#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_5_Core>
#include <QElapsedTimer>
#include <QPoint>
#include <QString>

namespace filament::backend {
	class PlatformGlfwGL;
}

namespace MOON {

	// 材质预览视口：宿主 Qt 的 QOpenGLWidget，把 Filament 的离屏渲染结果 blit 到 Qt FBO。
	// 场景内容（材质球 / 默认 PBR 材质 / IBL）由 FilamentApp + MaterialPreview 提供；
	// 这里只处理相机交互：左键 orbit、中/右键 pan、滚轮 zoom、双击复位。
	class ViewerWidget : public QOpenGLWidget, QOpenGLFunctions_4_5_Core
	{
		Q_OBJECT
	public:
		explicit ViewerWidget(QWidget* parent);
		~ViewerWidget();
		void initializeGL() override;
		void timerEvent(QTimerEvent* e) override;
		void paintGL() override;
		bool event(QEvent* evt) override;
		void leaveEvent(QEvent* event) override;
		void resizeEvent(QResizeEvent* event) override;
		void mousePressEvent(QMouseEvent* event) override;
		void mouseMoveEvent(QMouseEvent* event) override;
		void mouseReleaseEvent(QMouseEvent* event) override;
		void mouseDoubleClickEvent(QMouseEvent* event) override;
		void wheelEvent(QWheelEvent* event) override;
		void keyPressEvent(QKeyEvent* event) override;
		void keyReleaseEvent(QKeyEvent* event) override;
		void showImGui();

	private slots:
		void createEngineAndSetup();

	private:
		// Filament 的渲染目标 / Qt 的 FBO 都按设备像素工作；
		// 逻辑尺寸只用于 Qt 事件与布局。
		int renderWidth() const;
		int renderHeight() const;

		bool mGrabbing = false;
		QElapsedTimer mElapsed;
		double mLastFrameTime = -1.0;
		uint64_t mSubmittedFrames = 0;      // 已提交给 Filament 的帧数
		GLuint mBlitFbo = 0;                // GUI 线程用于 blit 的 FBO
		filament::backend::PlatformGlfwGL* mPlatform = nullptr;
		bool mHasBlitContent = false;       // 是否已成功呈现过至少一帧
		int mLastBlitSlot = 0;              // 最近一次呈现用的环形纹理槽
		QPoint mLastMousePos;               // 最近一次鼠标位置（用于开始 grab）
	};
}

#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_5_Core>
#include <QElapsedTimer>

namespace filament::backend {
class PlatformGlfwGL;
}

namespace MOON {

	class DebugOpenGLWidget : public QOpenGLWidget, QOpenGLFunctions_4_5_Core
	{
		Q_OBJECT
	public:
		explicit DebugOpenGLWidget(QWidget* parent);
		~DebugOpenGLWidget();
		void initializeGL() override;
		void timerEvent(QTimerEvent* e) override;
		void paintGL() override;
		bool event(QEvent* evt) override;
		void leaveEvent(QEvent* event) override;
		void resizeEvent(QResizeEvent* event) override;
		void mousePressEvent(QMouseEvent* event) override;
		void mouseMoveEvent(QMouseEvent* event) override;
		void mouseReleaseEvent(QMouseEvent* event) override;
		void wheelEvent(QWheelEvent* event) override;
		void keyPressEvent(QKeyEvent* event) override;
		void keyReleaseEvent(QKeyEvent* event) override;
		void showImGui();

	private slots:
		void createEngineAndSetup();

	private:
		bool mGrabbing = false;
		QElapsedTimer mElapsed;
		double mLastFrameTime = -1.0;
		uint64_t mSubmittedFrames = 0;      // 已提交给 Filament 的帧数
		GLuint mBlitFbo = 0;                // GUI 线程用于 blit 的 FBO
		filament::backend::PlatformGlfwGL* mPlatform = nullptr;
		bool mHasBlitContent = false;       // 是否已成功呈现过至少一帧
		int mLastBlitSlot = 0;              // 最近一次呈现用的环形纹理槽
	};
}




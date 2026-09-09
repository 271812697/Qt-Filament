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
		void wheelEvent(QWheelEvent* event) override;
		void keyPressEvent(QKeyEvent* event) override;
		void keyReleaseEvent(QKeyEvent* event) override;
		void showImGui();

	public slots:
		// 从文件加载 SaveDomains 网格并替换当前场景（Open 菜单 / 外部调用）
		void onReadFile(const QString& filePath);

	private slots:
		void createEngineAndSetup();
		// Filament 异步拾取结果（GUI 线程收到后处理高亮/联动树）
		void onPickProcessed(int entityIndex);
		// 拾取附带世界坐标：用于点击后把 orbit 旋转中心设到命中点
		void onPickedWorldPosition(int entityIndex,
			float worldX, float worldY, float worldZ);

	private:
		void issuePickIfIdle();
		void beginOrbitGrab();

		bool mGrabbing = false;
		QElapsedTimer mElapsed;
		double mLastFrameTime = -1.0;
		uint64_t mSubmittedFrames = 0;      // 已提交给 Filament 的帧数
		GLuint mBlitFbo = 0;                // GUI 线程用于 blit 的 FBO
		filament::backend::PlatformGlfwGL* mPlatform = nullptr;
		bool mHasBlitContent = false;       // 是否已成功呈现过至少一帧
		int mLastBlitSlot = 0;              // 最近一次呈现用的环形纹理槽
		// 视口悬浮拾取状态
		bool mPickInFlight = false;         // 有未返回的 pick 查询
		bool mPickQueued = false;           // 鼠标又移动了，等当前查询返回后补发
		bool mPickInside = false;           // 鼠标是否还在视口内
		int mPickActiveEntity = -1;         // 当前由视口拾取高亮的实体
		QPoint mPickPos;                    // 最近一次希望拾取的位置
		// 点击设旋转中心
		bool mLeftDown = false;             // 左键是否按住
		bool mOrbitCenterPending = false;   // 等待拾取结果来设置旋转中心
		QPoint mLastMousePos;               // 最近一次鼠标位置（用于开始 grab）
	};
}




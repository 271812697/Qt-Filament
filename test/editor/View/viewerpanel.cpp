#include "viewerpanel.h"
#include "viewerwidget.h"

#include "core/FilamentApp.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace MOON {
	ViewerPanel::ViewerPanel(QWidget* parent, Qt::WindowFlags f)
		: QWidget(parent, f) {
		// 顶部一条窄栏：说明当前预览的材质，以及视口操作提示。
		auto* header = new QWidget(this);
		header->setObjectName("PreviewHeader");
		header->setFixedHeight(28);
		header->setStyleSheet(R"(
			QWidget#PreviewHeader { background: #2b2b2e; border-bottom: 1px solid #1b1b1d; }
			QLabel#PreviewTitle { color: #e0e0e0; font-weight: 600; padding-left: 8px; }
			QLabel#PreviewHint { color: #7d8288; padding-left: 10px; }
			QPushButton#PreviewReset {
				color: #c8ccd0; background: transparent; border: 1px solid transparent;
				border-radius: 3px; padding: 2px 8px; margin-right: 4px;
			}
			QPushButton#PreviewReset:hover { background: #3a3a3e; border-color: #268bd2; }
		)");
		auto* headerLayout = new QHBoxLayout(header);
		headerLayout->setContentsMargins(0, 0, 0, 0);
		headerLayout->setSpacing(0);

		auto* title = new QLabel("MATERIAL PREVIEW", header);
		title->setObjectName("PreviewTitle");
		auto* hint = new QLabel(
			"Left drag: orbit  ·  Middle/Right drag: pan  ·  Wheel: zoom  ·  Double-click: reset",
			header);
		hint->setObjectName("PreviewHint");
		auto* reset = new QPushButton("Reset View", header);
		reset->setObjectName("PreviewReset");
		reset->setCursor(Qt::PointingHandCursor);
		reset->setToolTip("Reset the preview camera (R)");

		headerLayout->addWidget(title);
		headerLayout->addWidget(hint, 1);
		headerLayout->addWidget(reset);

		mSceneWindow = new ViewerWidget(this);
		QVBoxLayout* layout = new QVBoxLayout(this);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		layout->addWidget(header);
		layout->addWidget(mSceneWindow, 1);

		connect(reset, &QPushButton::clicked, this, []() {
			FilamentApp::instance().resetPreviewCamera();
		});

		// default to strong focus
		this->setFocusPolicy(Qt::StrongFocus);
		this->setMouseTracking(true);
	}

	ViewerWidget* ViewerPanel::viewerWidget() const {
		return mSceneWindow;
	}

	void ViewerPanel::keyPressEvent(QKeyEvent* event) {
		QWidget::keyPressEvent(event);
	}
}

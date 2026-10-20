#include <qpushbutton.h>
#include <qboxlayout.h>
#include <QVBoxLayout>
#include <QtWidgets/QApplication>
#include <QAction>
#include <QMenuBar>
#include <QKeySequence>
#include <QSplitter>
#include <QtWidgets/QDockWidget>
#include "editor.h"
#include "View/viewerwidget.h"
#include "UI/SettingPanel/SettingPanel.h"
#include "UI/MaterialPanel/MaterialPanel.h"
#include "MulViewPanel.h"
#include "UI/LogPanel/LogPanel.h"
#include "UI/ShaderPanel/ShaderPanel.h"
#include "core/FilamentApp.h"

#include "Command/menubar/cameraMode.h"
#include "Command/viewer/CameraFitCommand.h"
#include "Command/menubar/fpsStat.h"
#include "Command/menubar/visibleview.h"

namespace MOON {
	class Editor::EditorInternal {
	public:
		EditorInternal(Editor* editor) :self(editor)
		{
		}
		void initPanels() {
			QIcon icon;
			icon.addFile(QString::fromUtf8(":/widgets/icons/awesomeface.png"), QSize(), QIcon::Normal, QIcon::Off);
			self->setWindowIcon(icon);
			self->statusBar();   // 创建状态栏，菜单项悬浮提示（statusTip）显示在这里

			// 材质预览和 Shader 源码作为同一个 QDockWidget 的内容：
			// 上半部分是 3D 预览，下半部分是只读着色器/材质源码。
			auto* previewWorkspace = new QWidget(self);
			auto* previewLayout = new QVBoxLayout(previewWorkspace);
			previewLayout->setContentsMargins(0, 0, 0, 0);
			previewLayout->setSpacing(0);

			auto* previewSplitter = new QSplitter(Qt::Vertical, previewWorkspace);
			previewSplitter->setChildrenCollapsible(false);

			middlePanel = new MulViewPanel(previewSplitter);
			QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
			sizePolicy.setHeightForWidth(middlePanel->sizePolicy().hasHeightForWidth());
			middlePanel->setSizePolicy(sizePolicy);

			mShaderPanel = new ShaderPanel(previewSplitter);
			previewSplitter->addWidget(middlePanel);
			previewSplitter->addWidget(mShaderPanel);
			previewSplitter->setStretchFactor(0, 3);
			previewSplitter->setStretchFactor(1, 2);
			previewSplitter->setSizes({ 560, 300 });
			previewLayout->addWidget(previewSplitter);

			auto* previewDock = new QDockWidget(self);
			previewDock->setObjectName(QStringLiteral("PreviewDock"));
			previewDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
			previewDock->setTitleBarWidget(new QWidget(previewDock));
			previewDock->setWidget(previewWorkspace);
			previewDock->setStyleSheet(QStringLiteral("QDockWidget { border: none; }"));
			self->setCentralWidget(previewDock);

			// 材质预览应用不再需要场景树；左侧保留渲染设置，右侧是材质参数。
			auto settingPanel = new SettingPanel(self);
			settingPanel->setAllowedAreas(Qt::AllDockWidgetAreas);
			settingPanel->setWindowTitle(QApplication::translate("Settingpanel", "Setting", nullptr));

			auto materialPanel = new MaterialPanel(self);
			materialPanel->setAllowedAreas(Qt::AllDockWidgetAreas);
			materialPanel->setWindowTitle(QApplication::translate("Materialpanel", "Material", nullptr));

			self->addDockWidget(Qt::LeftDockWidgetArea, settingPanel);
			self->addDockWidget(Qt::RightDockWidgetArea, materialPanel);

			auto logPanelDock = new LogPanel(self);
			self->addDockWidget(Qt::BottomDockWidgetArea, logPanelDock);

			connectSignals();
		}
		void connectSignals() {
		}
		void buildMaterialMenu() {
			auto* resetMaterial = new QAction(QStringLiteral("&Reset Material"), self);
			resetMaterial->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
			resetMaterial->setStatusTip("Restore the default PBR parameter values");
			connect(resetMaterial, &QAction::triggered, self, []() {
				FilamentApp::instance().resetPreviewMaterial();
			});

			auto* resetCamera = new QAction(QStringLiteral("Reset &Camera"), self);
			resetCamera->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
			resetCamera->setStatusTip("Return the preview camera to the default 3/4 view");
			connect(resetCamera, &QAction::triggered, self, []() {
				FilamentApp::instance().resetPreviewCamera();
			});

			auto* fitPreview = new QAction(QStringLiteral("&Fit Preview"), self);
			fitPreview->setShortcut(QKeySequence(Qt::Key_F));
			fitPreview->setStatusTip("Fit the sphere in the viewport");
			connect(fitPreview, &QAction::triggered, self, []() {
				FilamentApp::instance().fitCameraToScene();
			});

			menu_Material->addAction(resetMaterial);
			menu_Material->addAction(resetCamera);
			menu_Material->addSeparator();
			menu_Material->addAction(fitPreview);
		}
		void buildDisplayMenu() {
			auto cameraModeCommand = new CameraModeComand(self);
			menu_Display->addAction(cameraModeCommand->action());
			// Camera Fit：悬浮子项提示具体视角方向
			auto cameraFitCommand = new CameraFitCommand(self);
			menu_Display->addAction(cameraFitCommand->action());
			auto fpsStatCommand = new FpsStatCommand(self);
			menu_Display->addAction(fpsStatCommand->action());
		}
		void buildViewMenu() {
			auto visible = new VisibleViewCommand(menu_View);
			visible->setUp(menu_View);

			auto* shaderAction = new QAction(QStringLiteral("&Shader Panel"), self);
			shaderAction->setCheckable(true);
			shaderAction->setChecked(true);
			connect(shaderAction, &QAction::toggled, self, [this](bool visible) {
				if (mShaderPanel) {
					mShaderPanel->setVisible(visible);
				}
			});
			menu_View->addAction(shaderAction);
		}

		void buildMenu() {
			mMenubar = new QMenuBar(self);
			mMenubar->setObjectName(QString::fromUtf8("menubar"));

			self->setMenuBar(mMenubar);
			menu_Material = new QMenu(mMenubar);
			menu_Display = new QMenu(mMenubar);
			menu_View = new QMenu(mMenubar);
			mMenubar->addAction(menu_Material->menuAction());
			mMenubar->addAction(menu_Display->menuAction());
			mMenubar->addAction(menu_View->menuAction());
			buildMaterialMenu();
			buildDisplayMenu();
			buildViewMenu();
		}
		void buildToolBar() {

		}
		void retranslateUi() {
			self->setWindowTitle(QCoreApplication::translate("Editor", "Material Preview", nullptr));
			menu_Material->setTitle("&Material");
			menu_Display->setTitle("&Display");
			menu_View->setTitle("&View");
		}
	private:
		Editor* self = nullptr;
		QSplitter* vert_splitter_ = nullptr;
		MulViewPanel* middlePanel = nullptr;
		ShaderPanel* mShaderPanel = nullptr;
		QMenuBar* mMenubar;
		QMenu* menu_Material;
		QMenu* menu_Display;
		QMenu* menu_View;
	};
	Editor::Editor() :mInternal(new EditorInternal(this))
	{
		mInternal->initPanels();
		mInternal->buildMenu();
		mInternal->buildToolBar();
		mInternal->retranslateUi();
	}
	Editor::~Editor()
	{
		delete mInternal;
	}
}

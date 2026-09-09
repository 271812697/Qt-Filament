#include <qpushbutton.h>
#include <qboxlayout.h>
#include <QtWidgets/QApplication>
#include <QMenuBar>
#include <QSplitter>
#include <QtWidgets/QDockWidget>
#include "editor.h"
#include "View/viewerwidget.h"
#include "UI/TreeViewPanel/hierarchypanel.h"
#include "UI/SettingPanel/SettingPanel.h"
#include "UI/PropertyPanel/PropertyPanel.h"
#include "MulViewPanel.h"
//#include "UI/ReousrcePanel/resourcePanel.h"
#include "UI/LogPanel/LogPanel.h"

#include "Command/menubar/openFile.h"
#include "Command/menubar/exportFile.h"
#include "Command/menubar/cameraMode.h"
#include "Command/viewer/CameraFitCommand.h"
#include "Command/menubar/fpsStat.h"
#include "Command/menubar/visibleview.h"
#include "editor/UI/TreeViewPanel/treeViewpanel.h"

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
			auto centralwidget = new QWidget(self);
			self->setCentralWidget(centralwidget);
			auto centralwidget_layout = new QHBoxLayout(centralwidget);
			middlePanel = new MulViewPanel(centralwidget);
			QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
			sizePolicy.setHeightForWidth(middlePanel->sizePolicy().hasHeightForWidth());
			middlePanel->setSizePolicy(sizePolicy);
			centralwidget_layout->addWidget(middlePanel);
			centralwidget_layout->setContentsMargins(0, 0, 0, 0);	

			auto hierarchypanel = new Hierarchypanel(self);
			hierarchypanel->setAllowedAreas(Qt::AllDockWidgetAreas);
			hierarchypanel->setWindowTitle(QApplication::translate("Hierarchypanel", "Hierarchy", nullptr));
			
			auto settingPanel = new SettingPanel(self);
			settingPanel->setAllowedAreas(Qt::AllDockWidgetAreas);
			settingPanel->setWindowTitle(QApplication::translate("Settingpanel", "Setting", nullptr));
			
			auto propertyPanel=new PropertyPanel(self);
			propertyPanel->setAllowedAreas(Qt::AllDockWidgetAreas);
			propertyPanel->setWindowTitle(QApplication::translate("Propertypanel", "Property", nullptr));

			

			self->addDockWidget(Qt::LeftDockWidgetArea, hierarchypanel);
			self->addDockWidget(Qt::LeftDockWidgetArea, settingPanel);
			self->addDockWidget(Qt::RightDockWidgetArea,propertyPanel);

			/*auto resourcePanelDock = new ResPanel(self);
			self->addDockWidget(Qt::RightDockWidgetArea, resourcePanelDock);*/
			auto logPanelDock = new LogPanel(self);
			self->addDockWidget(Qt::BottomDockWidgetArea, logPanelDock);
			//auto debugWidget = new DebugWidget(self);
			//self->addDockWidget(Qt::LeftDockWidgetArea, debugWidget);
			//self->tabifyDockWidget(HierarchypanelDock, debugWidget);

			connectSignals();
		}
		void connectSignals() {
		
		
		}
		void buildFileMenu() {
			auto openFileCommand=new OpenFileCommand(self);
			auto exportFileCommand = new ExportFileCommand(self);

			// Open → 文件对话框 → 加载到当前 filament viewer
			if (middlePanel) {
				if (ViewerWidget* viewer = middlePanel->currentViewerWidget()) {
					connect(openFileCommand, &OpenFileCommand::readFilePath,
						viewer, &ViewerWidget::onReadFile);
				}
			}
			
			menu_File->addAction(openFileCommand->action());
			menu_File->addAction(exportFileCommand->action());
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
			auto visible=new VisibleViewCommand(menu_View);
			visible->setUp(menu_View);
		}

		void buildMenu() {
			mMenubar = new QMenuBar(self);
			mMenubar->setObjectName(QString::fromUtf8("menubar"));
			//mMenubar->setGeometry(QRect(0, 0, 1152, 20));

			self->setMenuBar(mMenubar);
			menu_File = new QMenu(mMenubar);
			menu_Display = new QMenu(mMenubar);
			menu_View = new QMenu(mMenubar);
			menu_sketch = new QMenu(mMenubar);
			mMenubar->addAction(menu_File->menuAction());
			mMenubar->addAction(menu_Display->menuAction());
			mMenubar->addAction(menu_View->menuAction());
			mMenubar->addAction(menu_sketch->menuAction());
			buildFileMenu();
			buildDisplayMenu();
			buildViewMenu();
	
		}
		void buildToolBar() {

		}
		void retranslateUi() {
			
			self->setWindowTitle(QCoreApplication::translate("Editor", "MainWindow", nullptr));
			menu_File->setTitle("&File");
			menu_Display->setTitle("&Display");
			menu_View->setTitle("&View");
			menu_sketch->setTitle("&Sketch");
		
		}
	private:
		Editor* self = nullptr;
		QSplitter* vert_splitter_ = nullptr;
		MulViewPanel* middlePanel = nullptr;
		QMenuBar* mMenubar;
		QMenu* menu_File;
		QMenu* menu_Display;
		QMenu* menu_View;
		QMenu* menu_sketch;
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

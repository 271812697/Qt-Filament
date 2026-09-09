#include "cameraMode.h"
#include <QMenu>
#include <QtWidgets/QFileDialog>
#include <QCoreApplication>
#include "core/FilamentApp.h"


namespace MOON {
	//-----------------------------------------------------------------------------
	CameraModeComand::CameraModeComand(QObject* parentObject)
		: Command(parentObject)
	{
		mActionOrtho = new QAction(Command::tr("Orthographic"), this);
		mActionPersp = new QAction(Command::tr("Perspective"), this);
		mActionOrtho->setCheckable(true);
		mActionPersp->setCheckable(true);
		auto menu = new QMenu(static_cast<QWidget*>(parentObject));
		menu->addAction(mActionOrtho);
		menu->addAction(mActionPersp);
		auto group = new QActionGroup(menu);
		group->setExclusive(true);
		group->addAction(mActionOrtho);
		group->addAction(mActionPersp);
		mActionPersp->setChecked(true);   // 默认透视

		// 菜单项 → FilamentApp 投影模式
		connect(mActionPersp, &QAction::triggered, this, [this](bool checked) {
			if (checked) {
				FilamentApp::instance().setProjectionMode(
					FilamentApp::ProjectionMode::Perspective);
			}
		});
		connect(mActionOrtho, &QAction::triggered, this, [this](bool checked) {
			if (checked) {
				FilamentApp::instance().setProjectionMode(
					FilamentApp::ProjectionMode::Orthographic);
			}
		});

		auto action = new QAction(this);
		action->setText(Command::tr("Projection"));
		action->setMenu(menu);
		setAction(action);
		setIcon(":/widgets/icons/pqCamera.svg");
		
		action->setObjectName(QString::fromUtf8("actionCameraMode"));
		action->setText("&Camera Mode");
		action->setStatusTip("Switch to Camera Mode");
		action->setShortcut(QCoreApplication::translate("pqFileMenuBuilder", "Ctrl+M", nullptr));



	}

	void CameraModeComand::execute()
	{
	}
}





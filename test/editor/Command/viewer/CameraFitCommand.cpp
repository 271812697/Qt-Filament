#include "CameraFitCommand.h"

#include "core/FilamentApp.h"
#include <QIcon>
#include <QMenu>

namespace MOON {
	CameraFitCommand::CameraFitCommand(QObject* parent, Mode mode) :Command(parent)
	{
		this->ReactionMode = mode;
		auto action = new QAction(this);
		setAction(action);
		action->setObjectName(QString::fromUtf8("actionCameraFit"));
		action->setText("Camera &Fit");
		action->setStatusTip("Fit camera to scene");
		action->setIcon(QIcon(QString::fromUtf8(":/widgets/icons/pqZoomToSelection.svg")));
	}

	void CameraFitCommand::execute()
	{
		runMode(ReactionMode);
	}

	void CameraFitCommand::runMode(Mode mode)
	{
		FilamentApp& app = FilamentApp::instance();
		switch (mode) {
		case ZOOM_TO_DATA:
		case RESET_CAMERA:
			app.fitCameraToScene();
			break;
		case RESET_POSITIVE_X:
			app.lookFromDirection(1, 0, 0, 0, 1, 0);
			break;
		case RESET_NEGATIVE_X:
			app.lookFromDirection(-1, 0, 0, 0, 1, 0);
			break;
		case RESET_POSITIVE_Y:
			app.lookFromDirection(0, 1, 0, 0, 0, 1);
			break;
		case RESET_NEGATIVE_Y:
			app.lookFromDirection(0, -1, 0, 0, 0, 1);
			break;
		case RESET_POSITIVE_Z:
			app.lookFromDirection(0, 0, 1, 0, 1, 0);
			break;
		case RESET_NEGATIVE_Z:
			app.lookFromDirection(0, 0, -1, 0, 1, 0);
			break;
		case APPLY_ISOMETRIC_VIEW:
			app.lookFromDirection(1, 1, 1, 0, 1, 0);
			break;
		default:
			// ROTATE_CAMERA_CW / ROTATE_CAMERA_CCW 暂未接入
			break;
		}
	}

	CameraFitCommand::CameraFitCommand(QObject* parentObject)
		: Command(parentObject)
	{
		auto* menu = new QMenu(qobject_cast<QWidget*>(parentObject));
		const QString iconPath = ":/widgets/icons/";

		auto addMode = [this, menu, &iconPath](
			Mode mode, const QString& text,
			const QString& tip, const QString& icon) {
			auto* modeAction = new QAction(text, this);
			modeAction->setIcon(QIcon(iconPath + icon));
			modeAction->setStatusTip(tip);
			modeAction->setToolTip(tip);   // 悬浮时提示具体方向
			connect(modeAction, &QAction::triggered, this, [this, mode]() {
				runMode(mode);
			});
			menu->addAction(modeAction);
		};

		addMode(ZOOM_TO_DATA, "&Fit All",
			"将相机缩放/移动到完整场景范围", "pqZoomToSelection.svg");
		menu->addSeparator();
		addMode(RESET_POSITIVE_X, "Positive &X",
			"相机从 +X 方向观察场景（看向 -X）", "pqXPlus.svg");
		addMode(RESET_NEGATIVE_X, "Negative &X",
			"相机从 -X 方向观察场景（看向 +X）", "pqXMinus.svg");
		addMode(RESET_POSITIVE_Y, "Positive &Y",
			"相机从 +Y 方向观察场景（看向 -Y）", "pqYPlus.svg");
		addMode(RESET_NEGATIVE_Y, "Negative &Y",
			"相机从 -Y 方向观察场景（看向 +Y）", "pqYMinus.svg");
		addMode(RESET_POSITIVE_Z, "Positive &Z",
			"相机从 +Z 方向观察场景（看向 -Z）", "pqZPlus.svg");
		addMode(RESET_NEGATIVE_Z, "Negative &Z",
			"相机从 -Z 方向观察场景（看向 +Z）", "pqZMinus.svg");
		addMode(APPLY_ISOMETRIC_VIEW, "&Isometric",
			"等轴测视角：相机从 (1,1,1) 方向观察", "pqIsometricView.svg");

		auto action = new QAction(this);
		action->setObjectName(QString::fromUtf8("actionCameraFit"));
		action->setText("Camera &Fit");
		action->setStatusTip("相机 fit / 标准视角");
		action->setIcon(QIcon(iconPath + "pqZoomToSelection.svg"));
		action->setMenu(menu);
		setAction(action);
	}
}





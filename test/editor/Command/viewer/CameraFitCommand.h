#pragma once
#include "editor/Command/command.h"
namespace MOON {
	class  CameraFitCommand : public Command
	{
		Q_OBJECT

	public:
		enum Mode
		{
			RESET_CAMERA,
			RESET_POSITIVE_X,
			RESET_POSITIVE_Y,
			RESET_POSITIVE_Z,
			RESET_NEGATIVE_X,
			RESET_NEGATIVE_Y,
			RESET_NEGATIVE_Z,
			APPLY_ISOMETRIC_VIEW,
			ZOOM_TO_DATA,
			ROTATE_CAMERA_CW,
			ROTATE_CAMERA_CCW,
		};
		CameraFitCommand(QObject* parent, Mode mode);
		// 创建带标准视角子菜单的 Camera Fit 菜单项
		CameraFitCommand(QObject* parent);


	protected:
		virtual void execute()override;
	private:
		void runMode(Mode mode);
		Mode ReactionMode = ZOOM_TO_DATA;
	};

}


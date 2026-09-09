#include "CameraFitCommand.h"

namespace MOON {
	CameraFitCommand::CameraFitCommand(QObject* parent, Mode mode) :Command(parent)
	{
		this->ReactionMode = mode;
		auto action = new QAction(this);
		setAction(action);
			
	}

	void CameraFitCommand::execute()
	{
	}
}





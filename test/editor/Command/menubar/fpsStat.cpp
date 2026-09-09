#include "fpsStat.h"
namespace MOON {
	//-----------------------------------------------------------------------------
	FpsStatCommand::FpsStatCommand(QObject* parentObject)
		: Command(parentObject)
	{
		auto* fpsAction = new QAction(Command::tr("Show FPS"), this);
		fpsAction->setCheckable(true);
		
		fpsAction->setObjectName(QString::fromUtf8("actionFpsStat"));
		fpsAction->setStatusTip("Toggle the FPS statistics overlay");
		setAction(fpsAction);
	}

	void FpsStatCommand::execute()
	{
		const bool show = action()->isChecked();
	}
}

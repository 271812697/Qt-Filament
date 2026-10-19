#include "visibleview.h"
#include "editor/UI/TreeViewPanel/hierarchypanel.h"
#include "editor/UI/SettingPanel/SettingPanel.h"
#include "editor/UI/PropertyPanel/PropertyPanel.h"
#include "editor/UI/LogPanel/LogPanel.h"

#include <QMenu>
#include <QtWidgets/QFileDialog>
#include <QCoreApplication>


namespace MOON {
	VisibleViewCommand::VisibleViewCommand(QObject* parent):QObject(parent)
	{
		
	}
	void VisibleViewCommand::setUp(QMenu* menu)
	{
		auto materialPanel = new QAction(VisibleViewCommand::tr("Material"), this);
		auto settingPanel = new QAction(VisibleViewCommand::tr("Setting"), this);
		auto logPanel = new QAction(VisibleViewCommand::tr("Log"), this);
		materialPanel->setCheckable(true);
		materialPanel->setChecked(true);
		settingPanel->setCheckable(true);
		settingPanel->setChecked(true);
		logPanel->setCheckable(true);
		logPanel->setChecked(true);
		menu->addAction(materialPanel);
		menu->addAction(settingPanel);
		menu->addAction(logPanel);

	}
}





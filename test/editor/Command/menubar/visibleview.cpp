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
		auto hierarchypanel = new QAction(VisibleViewCommand::tr("Hierarchy"), this);
		auto settingPanel = new QAction(VisibleViewCommand::tr("Setting"), this);
		auto propertyPanel = new QAction(VisibleViewCommand::tr("Property"), this);
		auto taskViewPanel = new QAction(VisibleViewCommand::tr("Task View"), this);
		auto logPanel = new QAction(VisibleViewCommand::tr("Log"), this);
		hierarchypanel->setCheckable(true);
		hierarchypanel->setChecked(true);
		settingPanel->setCheckable(true);
		settingPanel->setChecked(true);
		propertyPanel->setCheckable(true);
		propertyPanel->setChecked(true);
		taskViewPanel->setCheckable(true);
		taskViewPanel->setChecked(true);
		logPanel->setCheckable(true);
		logPanel->setChecked(true);
		menu->addAction(hierarchypanel);
		menu->addAction(settingPanel);
		menu->addAction(propertyPanel);
		menu->addAction(taskViewPanel);
		menu->addAction(logPanel);

	}
}





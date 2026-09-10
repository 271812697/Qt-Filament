#pragma once
#include "SettingPanel.h"
#include "SettingWidget.h"
#include "editor/UI/DockWidgetTitleBar.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QListWidget>
namespace MOON {
	SettingPanel::SettingPanel(QWidget* parent):QDockWidget(parent)
	{
		
		setTitleBarWidget(new DockWidgetTitleBar(this));
		SettingWidget* ui =new SettingWidget(this);
		setWidget(ui);
	}
}

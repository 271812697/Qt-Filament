#pragma once

#include "editor/UI/SettingPanel/RenderSettingWidget.h"
#include "editor/UI/PropertyPanel/Collapsiblegroupboxwidget.h"

#include <QTreeWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QColorDialog>
#include <QFontDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QStyleFactory>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <fstream>

namespace MOON {


	
	class RenderSettingWidget::RenderSettingWidgetInternal {
	public:
		RenderSettingWidgetInternal(RenderSettingWidget* tree) :mSelf(tree) {

		}		
		void setUp() {
			// 布局
			layout_ = new QVBoxLayout(mSelf);
			layout_->setContentsMargins(0, 0, 0, 0);
			mSelf->setLayout(layout_);
		}
		void Refresh() {
		
			layout_->addStretch();

		}
		~RenderSettingWidgetInternal() {
		}
	private:
		friend class RenderSettingWidget;
		RenderSettingWidget* mSelf = nullptr;
		QVBoxLayout* layout_ = nullptr;
		QTreeView* m_treeView;
		
		
	};
	RenderSettingWidget::RenderSettingWidget(QWidget* parent):QWidget(parent),mInternal(new RenderSettingWidgetInternal(this))
	{
		
		mInternal->setUp();
	}
	void RenderSettingWidget::Refresh() {
	
		mInternal->Refresh();
	}
	RenderSettingWidget::~RenderSettingWidget()
	{
		delete mInternal;
	}
}
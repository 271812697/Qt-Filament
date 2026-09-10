#pragma once
#include "SettingWidget.h"
#include "editor/UI/SettingPanel/RenderSettingWidget.h"
#include "editor/UI/PropertyPanel/Collapsiblegroupboxwidget.h"
#include "Widgets/utils.h"
#include <QVBoxLayout>
#include <QScrollArea>

namespace MOON {

	class SettingWidget::SettingWidgetInternal {
	public:
		SettingWidgetInternal(SettingWidget* tree) :mSelf(tree) {
		}

		void setUp() {
			// Same editor styling as the property panel, so the collapsible
			// groups look consistent in both panels.
			mSelf->setStyleSheet(R"(
				QLabel#PropertyLabel {
					color: #c8ccd0;
				}
				QLabel#GroupTitle {
					color: #e0e0e0;
					font-weight: 600;
				}
				QLineEdit, QDoubleSpinBox, QComboBox {
					background: #2a2a2d;
					border: 1px solid #3a3a3e;
					border-radius: 3px;
					padding: 2px 4px;
					color: #c8ccd0;
					selection-background-color: #268bd2;
				}
				QLineEdit:hover, QDoubleSpinBox:hover, QComboBox:hover,
				QLineEdit:focus, QDoubleSpinBox:focus, QComboBox:focus {
					background: #333338;
					border: 1px solid #268bd2;
				}
				QLineEdit#NumberWidget {
					background: transparent;
					border: 1px solid transparent;
				}
				QLineEdit#NumberWidget:hover,
				QLineEdit#NumberWidget:focus {
					background: #47474b;
					border: 1px solid #268bd2;
				}
				QLineEdit#NumberWidget[invalid=true] {
					border: 1px solid #801717;
				}
				QComboBox::drop-down {
					border: none;
					width: 18px;
				}
				QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
					border: none;
					background: #333338;
					width: 16px;
				}
				QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover {
					background: #47474b;
				}
			)");

			auto* scrollArea = new QScrollArea(mSelf);
			scrollArea->setWidgetResizable(true);
			scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			scrollArea->setFrameShape(QFrame::NoFrame);
			scrollArea->setContentsMargins(0, 0, 0, 0);

			auto* content = new QWidget();
			auto* layout = new QVBoxLayout(content);
			layout->setContentsMargins(0, 0, 0, 0);
			layout->setSpacing(refSpacePx(content));

			// Three top-level collapsible groups, each containing the item
			// groups built by the corresponding setting widget.
			auto* outerRenderSetting = new CollapsibleGroupBoxWidget("Render Setting", content);
			layout->addWidget(outerRenderSetting);
			// RenderPass / View 相关设置
			auto* renderSettingWidget = new RenderSettingWidget(content);
			outerRenderSetting->addSubWidget(renderSettingWidget);
			//layout->addStretch();

			

			outerRenderSetting->setCollapsed(false);

			scrollArea->setWidget(content);

			QVBoxLayout* mainLayout = new QVBoxLayout(mSelf);
			mainLayout->setContentsMargins(0, 0, 0, 0);
			mainLayout->setSpacing(0);
			mainLayout->addWidget(scrollArea);
		}

		~SettingWidgetInternal() {
		}
	private:
		friend class SettingWidget;
		SettingWidget* mSelf = nullptr;
	};

	SettingWidget::SettingWidget(QWidget* parent) :QWidget(parent), mInternal(new SettingWidgetInternal(this))
	{
		mInternal->setUp();
	}
	SettingWidget::~SettingWidget()
	{
		delete mInternal;
	}
}

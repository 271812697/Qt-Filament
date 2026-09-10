#pragma once
#include <QWidget>

namespace MOON {

	// 显示当前选中实体（Filament scene entity）的组件属性
	class PropertyWidget : public QWidget
	{
		Q_OBJECT
	public:
		PropertyWidget(QWidget* parent = nullptr);
		~PropertyWidget();

	public slots:
		// 选中实体 / 场景内容变化时重建属性面板
		void Refresh();

	private:
		class PropertyWidgetInternal;
		PropertyWidgetInternal* mInternal = nullptr;
	};

}

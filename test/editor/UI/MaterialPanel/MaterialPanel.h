#pragma once
#include <QtWidgets/QDockWidget>

namespace MOON {

	// 右侧停靠面板：材质参数（MaterialWidget）。
	class MaterialPanel : public QDockWidget
	{
		Q_OBJECT
	public:
		explicit MaterialPanel(QWidget* parent);
		~MaterialPanel();

	private:
		class MaterialPanelInternal;
		MaterialPanelInternal* mInternal = nullptr;
	};

}

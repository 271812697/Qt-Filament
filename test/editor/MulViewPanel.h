#pragma once
#include <QWidget>
namespace MOON {

	class ViewerPanel;
	class ViewerWidget;

	class MulViewPanel : public QWidget
	{
	public:
		explicit MulViewPanel(QWidget* parent = nullptr);
		~MulViewPanel();
		// 当前激活（或唯一）的 viewer；多标签时可扩展为取当前页
		ViewerWidget* currentViewerWidget() const;
	private:
		class MulViewPanelImpl;

		MulViewPanelImpl* impl;
	};
}

#pragma once
#include <QWidget>
namespace MOON {
	class RenderSettingWidget : public QWidget
	{
		Q_OBJECT
	public:
		RenderSettingWidget(QWidget* parent);
		~RenderSettingWidget();
	public slots:
		void Refresh();
	private:
		class RenderSettingWidgetInternal;
		RenderSettingWidgetInternal* mInternal=nullptr;
	};
}

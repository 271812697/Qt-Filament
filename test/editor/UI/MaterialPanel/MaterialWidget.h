#pragma once
#include <QWidget>

namespace MOON {

	// 材质参数面板：默认 PBR 参数 + IBL 环境参数。
	// 数值直接写入 FilamentApp 的 MaterialPreview，预览球实时更新。
	class MaterialWidget : public QWidget
	{
		Q_OBJECT
	public:
		explicit MaterialWidget(QWidget* parent = nullptr);
		~MaterialWidget();

	public slots:
		// 引擎初始化完成 / 参数被外部复位后，把 Filament 侧的当前值同步回控件
		void RefreshValues();

	private:
		class MaterialWidgetInternal;
		MaterialWidgetInternal* mInternal = nullptr;
	};

}

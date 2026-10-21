#pragma once
#include <QWidget>

namespace MOON {

	// 材质参数面板：参数控件由当前材质包声明的参数元数据动态生成。
	// 数值直接写入 FilamentApp 的 MaterialPreview，预览球实时更新。
	class MaterialWidget : public QWidget
	{
		Q_OBJECT
	public:
		explicit MaterialWidget(QWidget* parent = nullptr);
		~MaterialWidget();

		// 当前 Variant Features 勾选出的 Filament variant key
		int variantKey() const;
		// 当前预览材质在 MaterialAssets() 中的索引
		int materialIndex() const;

	signals:
		void variantKeyChanged(int key);
		void materialChanged(int index);

	public slots:
		// 引擎初始化完成 / 参数被外部复位后，把 Filament 侧的当前值同步回控件
		void RefreshValues();

	private:
		class MaterialWidgetInternal;
		MaterialWidgetInternal* mInternal = nullptr;
	};

}

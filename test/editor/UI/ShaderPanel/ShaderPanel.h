#pragma once
#include <QWidget>

namespace MOON {

	// 只读 GPU 着色器查看面板：左侧 .mat 源码，右侧 Vertex / Fragment / Compute GLSL。
	// 当前预览材质包中的源码由应用内嵌 .filamat 包运行时提取。
	class ShaderPanel : public QWidget
	{
		Q_OBJECT
	public:
		explicit ShaderPanel(QWidget* parent);
		~ShaderPanel();

	public slots:
		// 由 Material 面板的 Variant Features 组驱动
		void SetVariantKey(int key);
		// 切换当前预览材质，重新加载 GLSL 变体包和 .mat 源码
		void SetMaterialIndex(int index);

	private:
		class ShaderPanelInternal;
		ShaderPanelInternal* mInternal = nullptr;
	};

}

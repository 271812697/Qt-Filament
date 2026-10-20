#pragma once
#include <QtWidgets/QDockWidget>

namespace MOON {

	// 只读 GPU 着色器查看面板：Vertex / Fragment / Compute 三个阶段。
	// 当前预览材质 aiDefaultMat 的源码由 matc/matinfo 从其 .filamat 包中提取。
	class ShaderPanel : public QDockWidget
	{
		Q_OBJECT
	public:
		explicit ShaderPanel(QWidget* parent);
		~ShaderPanel();

	private:
		class ShaderPanelInternal;
		ShaderPanelInternal* mInternal = nullptr;
	};

}

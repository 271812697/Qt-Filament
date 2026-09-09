#include "MulViewPanel.h"
#include "editor/View/viewerpanel.h"
#include <QTabWidget>
#include <QGridLayout>
namespace MOON {

	class MulViewPanel::MulViewPanelImpl {
	public:
		QTabWidget* tabWidget = nullptr;
		ViewerPanel* viewerPanel = nullptr;

	};


	MulViewPanel::MulViewPanel(QWidget* parent) :QWidget(parent)
	{
		impl = new MulViewPanelImpl();
		impl->tabWidget = new QTabWidget(this);
		QGridLayout* glayout = new QGridLayout(this);
		glayout->setContentsMargins(0, 0, 0, 0);
		glayout->setSpacing(0);
		glayout->addWidget(impl->tabWidget, 0, 0);
		
		impl->viewerPanel = new ViewerPanel(this);
		impl->tabWidget->addTab(impl->viewerPanel, "viewerpanel");

	}

	MulViewPanel::~MulViewPanel()
	{
		delete impl;
	}

	ViewerWidget* MulViewPanel::currentViewerWidget() const {
		return impl->viewerPanel ? impl->viewerPanel->viewerWidget() : nullptr;
	}

}

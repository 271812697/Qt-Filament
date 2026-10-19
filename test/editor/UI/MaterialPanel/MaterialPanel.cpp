#include "MaterialPanel.h"
#include "MaterialWidget.h"

#include "editor/UI/DockWidgetTitleBar.h"

#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>

namespace MOON {

	class MaterialPanel::MaterialPanelInternal {
	public:
		explicit MaterialPanelInternal(MaterialPanel* self) : mSelf(self) {}

		void setUp() {
			mSelf->setObjectName("MaterialPanel");
			mSelf->setTitleBarWidget(new DockWidgetTitleBar(mSelf));
			mSelf->setMinimumWidth(330);

			auto* scrollArea = new QScrollArea(mSelf);
			scrollArea->setObjectName("MaterialScroll");
			scrollArea->setWidgetResizable(true);
			scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			scrollArea->setFrameShape(QFrame::NoFrame);
			scrollArea->setContentsMargins(0, 0, 0, 0);
			scrollArea->setStyleSheet(
				"QScrollArea#MaterialScroll { background: #2e2e32; border: none; }");

			auto* content = new MaterialWidget(scrollArea);
			scrollArea->setWidget(content);
			mSelf->setWidget(scrollArea);
		}

	private:
		MaterialPanel* mSelf = nullptr;
	};

	MaterialPanel::MaterialPanel(QWidget* parent)
		: QDockWidget(parent), mInternal(new MaterialPanelInternal(this)) {
		mInternal->setUp();
	}

	MaterialPanel::~MaterialPanel() {
		delete mInternal;
	}

} // namespace MOON

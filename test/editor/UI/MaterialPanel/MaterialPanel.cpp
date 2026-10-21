#include "MaterialPanel.h"
#include "MaterialWidget.h"

#include "editor/UI/DockWidgetTitleBar.h"
#include "editor/UI/ShaderPanel/ShaderPanel.h"

#include <QFrame>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

namespace MOON {

	class MaterialPanel::MaterialPanelInternal {
	public:
		explicit MaterialPanelInternal(MaterialPanel* self) : mSelf(self) {}

		void setUp() {
			mSelf->setObjectName("MaterialPanel");
			mSelf->setTitleBarWidget(new DockWidgetTitleBar(mSelf));
			mSelf->setMinimumWidth(600);

			auto* splitter = new QSplitter(Qt::Vertical, mSelf);
			splitter->setChildrenCollapsible(false);

			auto* scrollArea = new QScrollArea(splitter);
			scrollArea->setObjectName("MaterialScroll");
			scrollArea->setWidgetResizable(true);
			scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			scrollArea->setFrameShape(QFrame::NoFrame);
			scrollArea->setContentsMargins(0, 0, 0, 0);
			scrollArea->setStyleSheet(
				"QScrollArea#MaterialScroll { background: #2e2e32; border: none; }");

			mWidget = new MaterialWidget(scrollArea);
			scrollArea->setWidget(mWidget);

			mShaderPanel = new ShaderPanel(splitter);

			splitter->addWidget(scrollArea);
			splitter->addWidget(mShaderPanel);
			splitter->setStretchFactor(0, 2);
			splitter->setStretchFactor(1, 3);
			splitter->setSizes({ 450, 330 });
			mSelf->setWidget(splitter);

			QObject::connect(mWidget, &MaterialWidget::variantKeyChanged,
				mShaderPanel, &ShaderPanel::SetVariantKey);
			QObject::connect(mWidget, &MaterialWidget::materialChanged,
				mShaderPanel, &ShaderPanel::SetMaterialIndex);
			mShaderPanel->SetMaterialIndex(mWidget->materialIndex());
			mShaderPanel->SetVariantKey(mWidget->variantKey());
		}

	private:
		MaterialPanel* mSelf = nullptr;
		MaterialWidget* mWidget = nullptr;
		ShaderPanel* mShaderPanel = nullptr;
	};

	MaterialPanel::MaterialPanel(QWidget* parent)
		: QDockWidget(parent), mInternal(new MaterialPanelInternal(this)) {
		mInternal->setUp();
	}

	MaterialPanel::~MaterialPanel() {
		delete mInternal;
	}

} // namespace MOON

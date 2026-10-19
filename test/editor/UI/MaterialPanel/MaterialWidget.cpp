#include "MaterialWidget.h"

#include "core/FilamentApp.h"
#include "editor/UI/PropertyPanel/Collapsiblegroupboxwidget.h"
#include "Widgets/BoolProperty.h"
#include "Widgets/ColorPickerProperty.h"
#include "Widgets/Property.h"
#include "Widgets/PropertyComponent.h"
#include "Widgets/SliderFloatProperty.h"

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>
#include <vector>

namespace MOON {
	namespace {

		// --------------------------------------------------------------------
		// 默认 PBR 材质参数（aiDefaultMat 声明的 4 个 uniform）
		// --------------------------------------------------------------------
		class PreviewMaterialComponent : public PropertyComponent {
		public:
			PreviewMaterialComponent() {
				addProperty(new ColorPickerProperty("Base Color", this));

				auto* metallic = new SliderFloatProperty("Metallic", this, 0.0f, 1.0f);
				metallic->setStep(0.01f);
				addProperty(metallic);

				auto* roughness = new SliderFloatProperty("Roughness", this, 0.0f, 1.0f);
				roughness->setStep(0.01f);
				addProperty(roughness);

				auto* reflectance = new SliderFloatProperty("Reflectance", this, 0.0f, 1.0f);
				reflectance->setStep(0.01f);
				addProperty(reflectance);
			}

			QVariant getPropertyValue(const QString& name) override {
				auto& app = FilamentApp::instance();
				if (name == "Base Color") return app.previewBaseColor();
				if (name == "Metallic") return app.previewMetallic();
				if (name == "Roughness") return app.previewRoughness();
				if (name == "Reflectance") return app.previewReflectance();
				return {};
			}

			void setPropertyValue(const QString& name, const QVariant& value) override {
				auto& app = FilamentApp::instance();
				if (name == "Base Color") {
					app.setPreviewBaseColor(value.value<QColor>());
				}
				else if (name == "Metallic") {
					app.setPreviewMetallic(value.toFloat());
				}
				else if (name == "Roughness") {
					app.setPreviewRoughness(value.toFloat());
				}
				else if (name == "Reflectance") {
					app.setPreviewReflectance(value.toFloat());
				}
			}

			QString getComponentName() override {
				return "PBR Material";
			}
		};

		// --------------------------------------------------------------------
		// IBL 环境参数：强度和绕 Y 轴旋转
		// --------------------------------------------------------------------
		class PreviewEnvironmentComponent : public PropertyComponent {
		public:
			PreviewEnvironmentComponent() {
				auto* intensity = new SliderFloatProperty(
					"IBL Intensity", this, 0.0f, 80000.0f);
				intensity->setStep(100.0f);
				addProperty(intensity);

				auto* rotation = new SliderFloatProperty(
					"IBL Rotation", this, 0.0f, 360.0f);
				rotation->setStep(1.0f);
				addProperty(rotation);

				addProperty(new BoolProperty(
					"Show Environment", this, BoolProperty::Style::InviwoRect));
			}

			QVariant getPropertyValue(const QString& name) override {
				auto& app = FilamentApp::instance();
				if (name == "IBL Intensity") return app.previewIblIntensity();
				if (name == "IBL Rotation") return app.previewIblRotation();
				if (name == "Show Environment") return app.previewEnvironmentVisible();
				return {};
			}

			void setPropertyValue(const QString& name, const QVariant& value) override {
				auto& app = FilamentApp::instance();
				if (name == "IBL Intensity") {
					app.setPreviewIblIntensity(value.toFloat());
				}
				else if (name == "IBL Rotation") {
					app.setPreviewIblRotation(value.toFloat());
				}
				else if (name == "Show Environment") {
					app.setPreviewEnvironmentVisible(value.toBool());
				}
			}

			QString getComponentName() override {
				return "Environment (IBL)";
			}
		};

	} // namespace

	class MaterialWidget::MaterialWidgetInternal {
	public:
		explicit MaterialWidgetInternal(MaterialWidget* self) : mSelf(self) {}
		~MaterialWidgetInternal() {
			for (auto& group : mGroups) {
				delete group.first;      // CollapsibleGroupBoxWidget（会删除子控件）
				delete group.second;     // PropertyComponent（会删除 WidgetProperty）
			}
			mGroups.clear();
		}

		void setUp() {
			mSelf->setStyleSheet(R"(
				QLabel#MaterialInfo { color: #e0e0e0; font-weight: 600; padding: 8px 8px 2px 8px; }
				QLabel#MaterialHint { color: #7d8288; padding: 0 8px 6px 8px; }
				QPushButton#MaterialButton {
					color: #d7dae0;
					background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
						stop:0 #5a5a60, stop:0.15 #323235, stop:0.9 #323235, stop:1 #222224);
					border: 1px solid #1b1b1d;
					border-radius: 3px;
					padding: 4px 10px;
				}
				QPushButton#MaterialButton:hover { border-color: #268bd2; }
				QPushButton#MaterialButton:pressed { padding-top: 5px; padding-left: 11px; }
			)");

			auto* layout = new QVBoxLayout(mSelf);
			layout->setContentsMargins(0, 0, 0, 0);
			layout->setSpacing(2);

			mInfo = new QLabel(mSelf);
			mInfo->setObjectName("MaterialInfo");
			mInfo->setWordWrap(true);
			auto* hint = new QLabel(
				"Live PBR preview on a sphere, lit by the default PBR material and a prefiltered studio IBL.",
				mSelf);
			hint->setObjectName("MaterialHint");
			hint->setWordWrap(true);
			layout->addWidget(mInfo);
			layout->addWidget(hint);

			auto* materialGroup = new CollapsibleGroupBoxWidget("PBR Material", mSelf);
			auto* materialComponent = new PreviewMaterialComponent();
			layout->addWidget(materialGroup);
			for (WidgetProperty* property : materialComponent->getProperties()) {
				materialGroup->addProperty(property);
			}
			materialGroup->setCollapsed(false);
			mGroups.emplace_back(materialGroup, materialComponent);

			auto* environmentGroup = new CollapsibleGroupBoxWidget("Environment (IBL)", mSelf);
			auto* environmentComponent = new PreviewEnvironmentComponent();
			layout->addWidget(environmentGroup);
			for (WidgetProperty* property : environmentComponent->getProperties()) {
				environmentGroup->addProperty(property);
			}
			environmentGroup->setCollapsed(false);
			mGroups.emplace_back(environmentGroup, environmentComponent);

			auto* buttons = new QWidget(mSelf);
			auto* buttonLayout = new QHBoxLayout(buttons);
			buttonLayout->setContentsMargins(8, 8, 8, 8);
			buttonLayout->setSpacing(6);

			auto* resetMaterial = new QPushButton("Reset Material", buttons);
			resetMaterial->setObjectName("MaterialButton");
			resetMaterial->setCursor(Qt::PointingHandCursor);
			resetMaterial->setToolTip("Restore the default PBR parameter values");

			auto* resetView = new QPushButton("Reset View", buttons);
			resetView->setObjectName("MaterialButton");
			resetView->setCursor(Qt::PointingHandCursor);
			resetView->setToolTip("Reset the preview camera");

			buttonLayout->addWidget(resetMaterial);
			buttonLayout->addWidget(resetView);
			layout->addWidget(buttons);
			layout->addStretch(1);

			QObject::connect(resetMaterial, &QPushButton::clicked, mSelf, []() {
				FilamentApp::instance().resetPreviewMaterial();
			});
			QObject::connect(resetView, &QPushButton::clicked, mSelf, []() {
				FilamentApp::instance().resetPreviewCamera();
			});

			QObject::connect(&FilamentApp::instance(), &FilamentApp::engineInitialized,
				mSelf, &MaterialWidget::RefreshValues);
			QObject::connect(&FilamentApp::instance(), &FilamentApp::previewMaterialReset,
				mSelf, &MaterialWidget::RefreshValues);

			refreshValues();
		}

		void refreshValues() {
			auto& app = FilamentApp::instance();
			if (mInfo) {
				mInfo->setText(QStringLiteral("Previewing: %1").arg(app.previewMaterialName()));
			}
			for (auto& group : mGroups) {
				group.second->updateWidgetValue();
			}
		}

	private:
		friend class MaterialWidget;
		MaterialWidget* mSelf = nullptr;
		QLabel* mInfo = nullptr;
		std::vector<std::pair<CollapsibleGroupBoxWidget*, PropertyComponent*>> mGroups;
	};

	MaterialWidget::MaterialWidget(QWidget* parent)
		: QWidget(parent), mInternal(new MaterialWidgetInternal(this)) {
		mInternal->setUp();
	}

	MaterialWidget::~MaterialWidget() {
		delete mInternal;
	}

	void MaterialWidget::RefreshValues() {
		mInternal->refreshValues();
	}

} // namespace MOON

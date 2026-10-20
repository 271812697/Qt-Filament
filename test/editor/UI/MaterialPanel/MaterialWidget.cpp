#include "MaterialWidget.h"

#include "core/FilamentApp.h"
#include "core/MaterialShaderLibrary.h"
#include "editor/UI/PropertyPanel/Collapsiblegroupboxwidget.h"
#include "Widgets/BoolProperty.h"
#include "Widgets/ColorPickerProperty.h"
#include "Widgets/Property.h"
#include "Widgets/PropertyComponent.h"
#include "Widgets/SliderFloatProperty.h"
#include "Widgets/TextureProperty.h"

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <functional>
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

				auto* clearCoat = new SliderFloatProperty("Clear Coat", this, 0.0f, 1.0f);
				clearCoat->setStep(0.01f);
				addProperty(clearCoat);

				auto* clearCoatRoughness =
					new SliderFloatProperty("Clear Coat Roughness", this, 0.0f, 1.0f);
				clearCoatRoughness->setStep(0.01f);
				addProperty(clearCoatRoughness);

				auto* transmission = new SliderFloatProperty("Transmission", this, 0.0f, 1.0f);
				transmission->setStep(0.01f);
				addProperty(transmission);

				auto* ior = new SliderFloatProperty("Refraction Index", this, 1.0f, 3.0f);
				ior->setStep(0.01f);
				addProperty(ior);

				auto* thickness = new SliderFloatProperty("Thickness", this, 0.0f, 10.0f);
				thickness->setStep(0.01f);
				addProperty(thickness);

				addProperty(new TextureProperty("Albedo Map", this));
				addProperty(new TextureProperty("Metallic Map", this));
				addProperty(new TextureProperty("Roughness Map", this));
				addProperty(new TextureProperty("AO Map", this));
				addProperty(new TextureProperty("Normal Map", this));
				addProperty(new TextureProperty("Emissive Map", this));
			}

			QVariant getPropertyValue(const QString& name) override {
				auto& app = FilamentApp::instance();
				if (name == "Base Color") return app.previewBaseColor();
				if (name == "Metallic") return app.previewMetallic();
				if (name == "Roughness") return app.previewRoughness();
				if (name == "Reflectance") return app.previewReflectance();
				if (name == "Clear Coat") return app.previewClearCoat();
				if (name == "Clear Coat Roughness") return app.previewClearCoatRoughness();
				if (name == "Transmission") return app.previewTransmission();
				if (name == "Refraction Index") return app.previewIor();
				if (name == "Thickness") return app.previewThickness();
				if (name == "Albedo Map") return app.previewAlbedoMap();
				if (name == "Metallic Map") return app.previewMetallicMap();
				if (name == "Roughness Map") return app.previewRoughnessMap();
				if (name == "AO Map") return app.previewAoMap();
				if (name == "Normal Map") return app.previewNormalMap();
				if (name == "Emissive Map") return app.previewEmissiveMap();
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
				else if (name == "Clear Coat") {
					app.setPreviewClearCoat(value.toFloat());
				}
				else if (name == "Clear Coat Roughness") {
					app.setPreviewClearCoatRoughness(value.toFloat());
				}
				else if (name == "Transmission") {
					app.setPreviewTransmission(value.toFloat());
				}
				else if (name == "Refraction Index") {
					app.setPreviewIor(value.toFloat());
				}
				else if (name == "Thickness") {
					app.setPreviewThickness(value.toFloat());
				}
				else if (name == "Albedo Map") {
					app.setPreviewAlbedoMap(value.toString());
				}
				else if (name == "Metallic Map") {
					app.setPreviewMetallicMap(value.toString());
				}
				else if (name == "Roughness Map") {
					app.setPreviewRoughnessMap(value.toString());
				}
				else if (name == "AO Map") {
					app.setPreviewAoMap(value.toString());
				}
				else if (name == "Normal Map") {
					app.setPreviewNormalMap(value.toString());
				}
				else if (name == "Emissive Map") {
					app.setPreviewEmissiveMap(value.toString());
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

		// --------------------------------------------------------------------
		// Filament variant features：只影响 Shader 面板显示哪一份编译好的源码，
		// 不改变预览场景实际使用的 variant。
		// --------------------------------------------------------------------
		class PreviewVariantComponent : public PropertyComponent {
		public:
			PreviewVariantComponent(int* key, std::function<void(int)> onChanged)
				: mKey(key), mOnChanged(std::move(onChanged)) {
				for (const ShaderFeature& feature : MaterialShaderLibrary::Features()) {
					const QString label = QStringLiteral("%1  %2")
						.arg(QString::fromLatin1(feature.name),
							QString::fromLatin1(feature.shortName));
					mItems.emplace_back(label, feature.bit);
					addProperty(new BoolProperty(
						label, this, BoolProperty::Style::InviwoRect));
				}
			}

			QVariant getPropertyValue(const QString& name) override {
				const uint8_t bit = bitFor(name);
				return (*mKey & bit) != 0;
			}

			void setPropertyValue(const QString& name, const QVariant& value) override {
				const uint8_t bit = bitFor(name);
				if (value.toBool()) {
					*mKey |= bit;
				}
				else {
					*mKey &= ~static_cast<int>(bit);
				}
				if (mOnChanged) {
					mOnChanged(*mKey);
				}
			}

			QString getComponentName() override {
				return "Variant Features";
			}

		private:
			uint8_t bitFor(const QString& name) const {
				for (const auto& item : mItems) {
					if (item.first == name) {
						return item.second;
					}
				}
				return 0;
			}

			int* mKey = nullptr;
			std::function<void(int)> mOnChanged;
			std::vector<std::pair<QString, uint8_t>> mItems;
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
				"Live PBR preview on the selected mesh, lit by the default PBR material "
				"and a prefiltered studio IBL.",
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

			auto* variantGroup = new CollapsibleGroupBoxWidget("Variant Features", mSelf);
			auto* variantComponent = new PreviewVariantComponent(
				&mVariants, [this](int key) {
					emit mSelf->variantKeyChanged(key);
				});
			layout->addWidget(variantGroup);
			for (WidgetProperty* property : variantComponent->getProperties()) {
				variantGroup->addProperty(property);
			}
			variantGroup->setCollapsed(false);
			mGroups.emplace_back(variantGroup, variantComponent);

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

		int variantKey() const {
			return mVariants;
		}

	private:
		friend class MaterialWidget;
		MaterialWidget* mSelf = nullptr;
		QLabel* mInfo = nullptr;
		int mVariants = MaterialShaderLibrary::DefaultVariant();
		std::vector<std::pair<CollapsibleGroupBoxWidget*, PropertyComponent*>> mGroups;
	};

	MaterialWidget::MaterialWidget(QWidget* parent)
		: QWidget(parent), mInternal(new MaterialWidgetInternal(this)) {
		mInternal->setUp();
	}

	MaterialWidget::~MaterialWidget() {
		delete mInternal;
	}

	int MaterialWidget::variantKey() const {
		return mInternal ? mInternal->variantKey()
			: MaterialShaderLibrary::DefaultVariant();
	}

	void MaterialWidget::RefreshValues() {
		mInternal->refreshValues();
	}

} // namespace MOON

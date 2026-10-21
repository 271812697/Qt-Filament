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
#include <QComboBox>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <functional>
#include <utility>
#include <vector>

namespace MOON {
	namespace {

		// --------------------------------------------------------------------
		// 根据当前材质包声明的参数动态生成属性控件。
		// --------------------------------------------------------------------
		QString ParameterDisplayName(const QString& name) {
			QString result;
			result.reserve(name.size() + 8);
			for (int i = 0; i < name.size(); ++i) {
				const QChar c = name.at(i);
				if (i > 0 && c.isUpper() && !name.at(i - 1).isUpper()) {
					result += QLatin1Char(' ');
				}
				result += c;
			}
			if (!result.isEmpty()) {
				result[0] = result[0].toUpper();
			}
			result.replace(QStringLiteral("Ior"), QStringLiteral("IOR"));
			result.replace(QStringLiteral("Ao "), QStringLiteral("AO "));
			result.replace(QStringLiteral("Uv"), QStringLiteral("UV"));
			return result;
		}

		class DynamicMaterialComponent : public PropertyComponent {
		public:
			explicit DynamicMaterialComponent(std::vector<MaterialParameter> parameters) {
				for (const MaterialParameter& parameter : parameters) {
					if (parameter.count != 1) {
						continue;
					}
					const QString label = ParameterDisplayName(parameter.name);
					if (label.isEmpty() || mParameterNames.contains(label)) {
						continue;
					}

					switch (parameter.kind) {
					case MaterialParameterKind::Float: {
						float minimum = 0.0f;
						float maximum = 1.0f;
						if (parameter.name == "ior") {
							minimum = 1.0f;
							maximum = 3.0f;
						}
						else if (parameter.name == "thickness") {
							maximum = 10.0f;
						}
						else if (parameter.name == "subsurfacePower") {
							maximum = 20.0f;
						}
						else if (parameter.name == "anisotropy") {
							minimum = -1.0f;
						}
						auto* property = new SliderFloatProperty(
							label, this, minimum, maximum);
						property->setStep(0.01f);
						addProperty(property);
						break;
					}
					case MaterialParameterKind::Float3:
					case MaterialParameterKind::Float4:
						addProperty(new ColorPickerProperty(label, this));
						break;
					case MaterialParameterKind::Bool:
						addProperty(new BoolProperty(
							label, this, BoolProperty::Style::InviwoRect));
						break;
					case MaterialParameterKind::Sampler2D:
						addProperty(new TextureProperty(label, this));
						break;
					default:
						continue;
					}
					mParameterNames.insert(label, parameter.name);
				}
			}

			QVariant getPropertyValue(const QString& label) override {
				const QString name = mParameterNames.value(label);
				return FilamentApp::instance().previewMaterialParameterValue(name);
			}

			void setPropertyValue(const QString& label, const QVariant& value) override {
				const QString name = mParameterNames.value(label);
				if (!name.isEmpty()) {
					FilamentApp::instance().setPreviewMaterialParameterValue(name, value);
				}
			}

			QString getComponentName() override {
				return "Material Parameters";
			}

		private:
			QHash<QString, QString> mParameterNames;
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
			delete mMaterialGroup;
			delete mMaterialComponent;
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
				QComboBox#MaterialSelector {
					color: #d7dae0;
					background: #1e1f23;
					border: 1px solid #3a3a40;
					border-radius: 3px;
					padding: 3px 8px;
				}
				QComboBox#MaterialSelector:hover { border-color: #268bd2; }
				QComboBox#MaterialSelector QAbstractItemView {
					color: #d7dae0;
					background: #242428;
					selection-background-color: #264f78;
					border: 1px solid #3a3a40;
				}
			)");

			auto* layout = new QVBoxLayout(mSelf);
			layout->setContentsMargins(0, 0, 0, 0);
			layout->setSpacing(2);

			auto* selectorRow = new QWidget(mSelf);
			auto* selectorLayout = new QHBoxLayout(selectorRow);
			selectorLayout->setContentsMargins(8, 8, 8, 2);
			selectorLayout->setSpacing(8);
			auto* selectorLabel = new QLabel(QStringLiteral("Material"), selectorRow);
			selectorLabel->setObjectName(QStringLiteral("MaterialInfo"));
			selectorLabel->setStyleSheet(QStringLiteral("padding: 0;"));
			mMaterialCombo = new QComboBox(selectorRow);
			mMaterialCombo->setObjectName(QStringLiteral("MaterialSelector"));
			mMaterialCombo->addItems(FilamentApp::instance().previewMaterialNames());
			mMaterialCombo->setCurrentIndex(FilamentApp::instance().previewMaterialIndex());
			selectorLayout->addWidget(selectorLabel);
			selectorLayout->addWidget(mMaterialCombo, 1);
			layout->addWidget(selectorRow);

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

			mLayout = layout;
			mMaterialGroupIndex = layout->count();
			rebuildMaterialGroup();

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

			QObject::connect(mMaterialCombo,
				QOverload<int>::of(&QComboBox::currentIndexChanged),
				mSelf, [this](int index) {
					if (index < 0 || index == mMaterialIndex) {
						return;
					}
					mMaterialIndex = index;
					FilamentApp::instance().setPreviewMaterialIndex(index);
					emit mSelf->materialChanged(index);
					refreshValues();
				});

			refreshValues();
		}

		QString parameterSignature() const {
			QString signature;
			for (const MaterialParameter& parameter :
				FilamentApp::instance().previewMaterialParameters()) {
				signature += parameter.name;
				signature += QLatin1Char(':');
				signature += QString::number((int)parameter.kind);
				signature += QLatin1Char(';');
			}
			return signature;
		}

		void rebuildMaterialGroup() {
			if (mMaterialGroup) {
				mLayout->removeWidget(mMaterialGroup);
				delete mMaterialGroup;
				mMaterialGroup = nullptr;
			}
			if (mMaterialComponent) {
				delete mMaterialComponent;
				mMaterialComponent = nullptr;
			}

			auto parameters = FilamentApp::instance().previewMaterialParameters();
			mMaterialSignature = parameterSignature();
			mMaterialComponent = new DynamicMaterialComponent(std::move(parameters));
			mMaterialGroup = new CollapsibleGroupBoxWidget(
				QStringLiteral("Material Parameters"), mSelf);
			for (WidgetProperty* property : mMaterialComponent->getProperties()) {
				mMaterialGroup->addProperty(property);
			}
			mMaterialGroup->setCollapsed(false);
			mLayout->insertWidget(mMaterialGroupIndex, mMaterialGroup);
		}

		void ensureMaterialGroup() {
			if (parameterSignature() != mMaterialSignature) {
				rebuildMaterialGroup();
			}
		}

		void refreshValues() {
			auto& app = FilamentApp::instance();
			if (mInfo) {
				mInfo->setText(QStringLiteral("Previewing: %1").arg(app.previewMaterialName()));
			}
			if (mMaterialCombo) {
				const int index = app.previewMaterialIndex();
				if (index >= 0) {
					QSignalBlocker blocker(mMaterialCombo);
					mMaterialCombo->setCurrentIndex(index);
				}
				mMaterialIndex = index;
			}
			ensureMaterialGroup();
			if (mMaterialComponent) {
				mMaterialComponent->updateWidgetValue();
			}
			for (auto& group : mGroups) {
				group.second->updateWidgetValue();
			}
		}

		int variantKey() const {
			return mVariants;
		}

		int materialIndex() const {
			return mMaterialIndex;
		}

	private:
		friend class MaterialWidget;
		MaterialWidget* mSelf = nullptr;
		QLabel* mInfo = nullptr;
		QComboBox* mMaterialCombo = nullptr;
		QVBoxLayout* mLayout = nullptr;
		CollapsibleGroupBoxWidget* mMaterialGroup = nullptr;
		PropertyComponent* mMaterialComponent = nullptr;
		int mMaterialGroupIndex = 0;
		QString mMaterialSignature;
		int mMaterialIndex = 0;
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

	int MaterialWidget::materialIndex() const {
		return mInternal ? mInternal->materialIndex() : 0;
	}

	void MaterialWidget::RefreshValues() {
		mInternal->refreshValues();
	}

} // namespace MOON

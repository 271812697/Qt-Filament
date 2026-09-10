#include "RenderSettingWidget.h"

#include "core/FilamentApp.h"
#include "editor/UI/PropertyPanel/Collapsiblegroupboxwidget.h"
#include "Widgets/BoolProperty.h"
#include "Widgets/EnumProperty.h"
#include "Widgets/Property.h"
#include "Widgets/PropertyComponent.h"
#include "Widgets/SliderFloatProperty.h"

#include <filament/Options.h>
#include <filament/View.h>

#include <QVBoxLayout>
#include <vector>

namespace MOON {

namespace {

filament::View* CurrentView() {
    return FilamentApp::instance().view();
}

// ----------------------------------------------------------------------------
// 包装层：把 Filament View 的 RenderPass 参数暴露成 PropertyComponent
// ----------------------------------------------------------------------------

class PostProcessComponent : public PropertyComponent {
public:
    PostProcessComponent() {
        addProperty(new BoolProperty(
            "PostProcessing", this, BoolProperty::Style::InviwoRect));
        addProperty(new BoolProperty(
            "ColorGrading", this, BoolProperty::Style::InviwoRect));
        addProperty(new EnumProperty("Dithering", this));
        addProperty(new EnumProperty("AntiAliasing", this));
    }

    QVariant getPropertyValue(const QString& name) override {
        auto* view = CurrentView();
        if (name == "PostProcessing") {
            return view ? QVariant(view->isPostProcessingEnabled()) : QVariant();
        }
        if (name == "ColorGrading") {
            return view ? QVariant(view->getColorGrading() != nullptr) : QVariant();
        }
        if (name == "Dithering") {
            return QVariant::fromValue(QList<QString>{ "None", "Temporal" });
        }
        if (name == "AntiAliasing") {
            return QVariant::fromValue(QList<QString>{ "None", "FXAA" });
        }
        return QVariant();
    }

    void setPropertyValue(const QString& name, const QVariant& value) override {
        auto* view = CurrentView();
        if (!view) return;
        if (name == "PostProcessing") {
            view->setPostProcessingEnabled(value.toBool());
        } else if (name == "ColorGrading") {
            FilamentApp::instance().setColorGradingEnabled(value.toBool());
        } else if (name == "Dithering") {
            view->setDithering(static_cast<filament::Dithering>(value.toInt()));
        } else if (name == "AntiAliasing") {
            view->setAntiAliasing(static_cast<filament::AntiAliasing>(value.toInt()));
        }
    }

    QString getComponentName() override {
        return "Post Processing";
    }
};

class AntiAliasingComponent : public PropertyComponent {
public:
    AntiAliasingComponent() {
        addProperty(new BoolProperty(
            "MSAA", this, BoolProperty::Style::InviwoRect));
        addProperty(new EnumProperty("MSSASamples", this));
        addProperty(new BoolProperty(
            "TAA", this, BoolProperty::Style::InviwoRect));
    }

    QVariant getPropertyValue(const QString& name) override {
        auto* view = CurrentView();
        if (!view) return QVariant();
        if (name == "MSAA") {
            return QVariant(view->getMultiSampleAntiAliasingOptions().enabled);
        }
        if (name == "MSSASamples") {
            return QVariant::fromValue(QList<QString>{ "2", "4", "8" });
        }
        if (name == "TAA") {
            return QVariant(view->getTemporalAntiAliasingOptions().enabled);
        }
        return QVariant();
    }

    void setPropertyValue(const QString& name, const QVariant& value) override {
        auto* view = CurrentView();
        if (!view) return;
        if (name == "MSAA") {
            auto o = view->getMultiSampleAntiAliasingOptions();
            o.enabled = value.toBool();
            view->setMultiSampleAntiAliasingOptions(o);
        } else if (name == "MSSASamples") {
            static const uint8_t samples[] = { 2, 4, 8 };
            auto o = view->getMultiSampleAntiAliasingOptions();
            o.sampleCount = samples[value.toInt()];
            view->setMultiSampleAntiAliasingOptions(o);
        } else if (name == "TAA") {
            auto o = view->getTemporalAntiAliasingOptions();
            o.enabled = value.toBool();
            view->setTemporalAntiAliasingOptions(o);
        }
    }

    QString getComponentName() override {
        return "Anti Aliasing";
    }
};

class PostEffectsComponent : public PropertyComponent {
public:
    PostEffectsComponent() {
        addProperty(new BoolProperty(
            "Bloom", this, BoolProperty::Style::InviwoRect));
        addProperty(new SliderFloatProperty("BloomStrength", this, 0.0f, 2.0f));
        addProperty(new BoolProperty(
            "DOF", this, BoolProperty::Style::InviwoRect));
        addProperty(new SliderFloatProperty("CocScale", this, 0.0f, 10.0f));
        addProperty(new BoolProperty(
            "AO", this, BoolProperty::Style::InviwoRect));
        addProperty(new SliderFloatProperty("AoRadius", this, 0.0f, 10.0f));
        addProperty(new SliderFloatProperty("AoIntensity", this, 0.0f, 5.0f));
        addProperty(new BoolProperty(
            "Vignette", this, BoolProperty::Style::InviwoRect));
        addProperty(new SliderFloatProperty("VignetteMid", this, 0.0f, 1.0f));
        addProperty(new BoolProperty(
            "SSR", this, BoolProperty::Style::InviwoRect));
        addProperty(new SliderFloatProperty("SsrDistance", this, 0.1f, 100.0f));
    }

    QVariant getPropertyValue(const QString& name) override {
        auto* view = CurrentView();
        if (!view) return QVariant();
        if (name == "Bloom") return QVariant(view->getBloomOptions().enabled);
        if (name == "BloomStrength") return QVariant(view->getBloomOptions().strength);
        if (name == "DOF") return QVariant(view->getDepthOfFieldOptions().enabled);
        if (name == "CocScale") return QVariant(view->getDepthOfFieldOptions().cocScale);
        if (name == "AO") return QVariant(view->getAmbientOcclusionOptions().enabled);
        if (name == "AoRadius") return QVariant(view->getAmbientOcclusionOptions().radius);
        if (name == "AoIntensity") return QVariant(view->getAmbientOcclusionOptions().intensity);
        if (name == "Vignette") return QVariant(view->getVignetteOptions().enabled);
        if (name == "VignetteMid") return QVariant(view->getVignetteOptions().midPoint);
        if (name == "SSR") return QVariant(view->getScreenSpaceReflectionsOptions().enabled);
        if (name == "SsrDistance") return QVariant(view->getScreenSpaceReflectionsOptions().maxDistance);
        return QVariant();
    }

    void setPropertyValue(const QString& name, const QVariant& value) override {
        auto* view = CurrentView();
        if (!view) return;
        if (name == "Bloom") {
            auto o = view->getBloomOptions();
            o.enabled = value.toBool();
            view->setBloomOptions(o);
        } else if (name == "BloomStrength") {
            auto o = view->getBloomOptions();
            o.strength = value.toFloat();
            view->setBloomOptions(o);
        } else if (name == "DOF") {
            auto o = view->getDepthOfFieldOptions();
            o.enabled = value.toBool();
            view->setDepthOfFieldOptions(o);
        } else if (name == "CocScale") {
            auto o = view->getDepthOfFieldOptions();
            o.cocScale = value.toFloat();
            view->setDepthOfFieldOptions(o);
        } else if (name == "AO") {
            auto o = view->getAmbientOcclusionOptions();
            o.enabled = value.toBool();
            view->setAmbientOcclusionOptions(o);
        } else if (name == "AoRadius") {
            auto o = view->getAmbientOcclusionOptions();
            o.radius = value.toFloat();
            view->setAmbientOcclusionOptions(o);
        } else if (name == "AoIntensity") {
            auto o = view->getAmbientOcclusionOptions();
            o.intensity = value.toFloat();
            view->setAmbientOcclusionOptions(o);
        } else if (name == "Vignette") {
            auto o = view->getVignetteOptions();
            o.enabled = value.toBool();
            view->setVignetteOptions(o);
        } else if (name == "VignetteMid") {
            auto o = view->getVignetteOptions();
            o.midPoint = value.toFloat();
            view->setVignetteOptions(o);
        } else if (name == "SSR") {
            auto o = view->getScreenSpaceReflectionsOptions();
            o.enabled = value.toBool();
            view->setScreenSpaceReflectionsOptions(o);
        } else if (name == "SsrDistance") {
            auto o = view->getScreenSpaceReflectionsOptions();
            o.maxDistance = value.toFloat();
            view->setScreenSpaceReflectionsOptions(o);
        }
    }

    QString getComponentName() override {
        return "Post Effects";
    }
};

class ResolutionComponent : public PropertyComponent {
public:
    ResolutionComponent() {
        addProperty(new BoolProperty(
            "DynamicResolution", this, BoolProperty::Style::InviwoRect));
        addProperty(new SliderFloatProperty("MinScale", this, 0.25f, 1.0f));
        addProperty(new SliderFloatProperty("MaxScale", this, 0.5f, 2.0f));
    }

    QVariant getPropertyValue(const QString& name) override {
        auto* view = CurrentView();
        if (!view) return QVariant();
        if (name == "DynamicResolution") {
            return QVariant(view->getDynamicResolutionOptions().enabled);
        }
        if (name == "MinScale") {
            return QVariant(view->getDynamicResolutionOptions().minScale.x);
        }
        if (name == "MaxScale") {
            return QVariant(view->getDynamicResolutionOptions().maxScale.x);
        }
        return QVariant();
    }

    void setPropertyValue(const QString& name, const QVariant& value) override {
        auto* view = CurrentView();
        if (!view) return;
        auto o = view->getDynamicResolutionOptions();
        if (name == "DynamicResolution") {
            o.enabled = value.toBool();
        } else if (name == "MinScale") {
            o.minScale = { value.toFloat(), value.toFloat() };
        } else if (name == "MaxScale") {
            o.maxScale = { value.toFloat(), value.toFloat() };
        }
        view->setDynamicResolutionOptions(o);
    }

    QString getComponentName() override {
        return "Resolution";
    }
};

} // namespace

// ----------------------------------------------------------------------------
// RenderSettingWidget：用 CollapsibleGroupBox + PropertyComponent 呈现
// ----------------------------------------------------------------------------

class RenderSettingWidget::RenderSettingWidgetInternal {
public:
    RenderSettingWidgetInternal(RenderSettingWidget* self) : mSelf(self) {}

    void setUp() {
        layout_ = new QVBoxLayout(mSelf);
        layout_->setContentsMargins(0, 0, 0, 0);
        layout_->setSpacing(2);

        connect(&FilamentApp::instance(), &FilamentApp::engineInitialized,
            mSelf, &RenderSettingWidget::Refresh);
        connect(&FilamentApp::instance(), &FilamentApp::sceneLoaded,
            mSelf, &RenderSettingWidget::Refresh);
    }

    void Refresh() {
        // 清掉旧组件（参考用法：重建时先释放旧的 PropertyComponent/Group）
        while (auto* item = layout_->takeAt(0)) {
            delete item;
        }
        for (auto& [group, component] : mComponents) {
            delete group;
            delete component;
        }
        mComponents.clear();

        PropertyComponent* comps[] = {
            new PostProcessComponent(),
            new AntiAliasingComponent(),
            new PostEffectsComponent(),
            new ResolutionComponent(),
        };
        for (PropertyComponent* component : comps) {
            auto* group = new CollapsibleGroupBoxWidget(
                component->getComponentName(), mSelf);
            layout_->addWidget(group);
            for (WidgetProperty* property : component->getProperties()) {
                group->addProperty(property);
            }
            mComponents.emplace_back(group, component);
        }
        layout_->addStretch();
    }

    ~RenderSettingWidgetInternal() {
        for (auto& [group, component] : mComponents) {
            delete group;
            delete component;
        }
    }

private:
    friend class RenderSettingWidget;
    RenderSettingWidget* mSelf = nullptr;
    QVBoxLayout* layout_ = nullptr;
    std::vector<std::pair<CollapsibleGroupBoxWidget*, PropertyComponent*>> mComponents;
};

RenderSettingWidget::RenderSettingWidget(QWidget* parent)
    : QWidget(parent), mInternal(new RenderSettingWidgetInternal(this)) {
    mInternal->setUp();
}

void RenderSettingWidget::Refresh() {
    mInternal->Refresh();
}

RenderSettingWidget::~RenderSettingWidget() {
    delete mInternal;
}

} // namespace MOON

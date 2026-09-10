#include "PropertyWidget.h"

#include "core/FilamentApp.h"
#include "editor/UI/PropertyPanel/Collapsiblegroupboxwidget.h"
#include "Widgets/BoolProperty.h"
#include "Widgets/ColorPickerProperty.h"
#include "Widgets/FVec3Property.h"
#include "Widgets/Property.h"
#include "Widgets/PropertyComponent.h"

#include <QColor>
#include <QVBoxLayout>
#include <QVector3D>
#include <algorithm>
#include <vector>

namespace MOON {

namespace {

filament::math::float3 ToFloat3(const QVector3D& v) {
    return { v.x(), v.y(), v.z() };
}

QVector3D ToQVector3D(const filament::math::float3& v) {
    return QVector3D(v.x, v.y, v.z);
}

// ----------------------------------------------------------------------------
// 包装层：Filament scene entity 的组件 → PropertyComponent
// ----------------------------------------------------------------------------

// Transform 组件（对应 TransformManager）
class EntityTransformComponent : public PropertyComponent {
public:
    explicit EntityTransformComponent(size_t index) : mIndex(index) {
        addProperty(new FVec3Property("Position", this));
        addProperty(new FVec3Property("Rotation", this));
        addProperty(new FVec3Property("Scale", this));
    }

    QVariant getPropertyValue(const QString& name) override {
        auto& app = FilamentApp::instance();
        if (name == "Position") return ToQVector3D(app.entityTranslation(mIndex));
        if (name == "Rotation") return ToQVector3D(app.entityRotationEuler(mIndex));
        if (name == "Scale") return ToQVector3D(app.entityScale(mIndex));
        return QVariant();
    }

    void setPropertyValue(const QString& name, const QVariant& value) override {
        auto& app = FilamentApp::instance();
        const QVector3D v = value.value<QVector3D>();
        if (name == "Position") app.setEntityTranslation(mIndex, ToFloat3(v));
        else if (name == "Rotation") app.setEntityRotationEuler(mIndex, ToFloat3(v));
        else if (name == "Scale") app.setEntityScale(mIndex, ToFloat3(v));
    }

    QString getComponentName() override {
        return "Transform";
    }

private:
    size_t mIndex = 0;
};

// Renderable 组件（对应 RenderableManager + MaterialInstance）
class EntityRenderableComponent : public PropertyComponent {
public:
    explicit EntityRenderableComponent(size_t index) : mIndex(index) {
        addProperty(new BoolProperty("Visible", this, BoolProperty::Style::InviwoRect));
        addProperty(new BoolProperty("CastShadows", this, BoolProperty::Style::InviwoRect));
        addProperty(new BoolProperty("ReceiveShadows", this, BoolProperty::Style::InviwoRect));
        addProperty(new BoolProperty("Culling", this, BoolProperty::Style::InviwoRect));
        addProperty(new ColorPickerProperty("BaseColor", this));
    }

    QVariant getPropertyValue(const QString& name) override {
        auto& app = FilamentApp::instance();
        if (name == "Visible") return app.isEntityVisible(mIndex);
        if (name == "CastShadows") return app.entityCastShadows(mIndex);
        if (name == "ReceiveShadows") return app.entityReceiveShadows(mIndex);
        if (name == "Culling") return app.entityCulling(mIndex);
        if (name == "BaseColor") {
            const auto c = app.entityBaseColor(mIndex);
            return QVariant::fromValue(QColor(
                (int)std::clamp(c.x * 255.0f, 0.0f, 255.0f),
                (int)std::clamp(c.y * 255.0f, 0.0f, 255.0f),
                (int)std::clamp(c.z * 255.0f, 0.0f, 255.0f)));
        }
        return QVariant();
    }

    void setPropertyValue(const QString& name, const QVariant& value) override {
        auto& app = FilamentApp::instance();
        if (name == "Visible") {
            app.setEntityVisible(mIndex, value.toBool());
        } else if (name == "CastShadows") {
            app.setEntityCastShadows(mIndex, value.toBool());
        } else if (name == "ReceiveShadows") {
            app.setEntityReceiveShadows(mIndex, value.toBool());
        } else if (name == "Culling") {
            app.setEntityCulling(mIndex, value.toBool());
        } else if (name == "BaseColor") {
            const QColor color = value.value<QColor>();
            app.setEntityBaseColor(mIndex, {
                color.red() / 255.0f,
                color.green() / 255.0f,
                color.blue() / 255.0f
            });
        }
    }

    QString getComponentName() override {
        return "Renderable";
    }

private:
    size_t mIndex = 0;
};

} // namespace

class PropertyWidget::PropertyWidgetInternal {
public:
    PropertyWidgetInternal(PropertyWidget* self) : mSelf(self) {}

    void setUp() {
        layout_ = new QVBoxLayout(mSelf);
        layout_->setContentsMargins(0, 0, 0, 0);
        layout_->setSpacing(2);
        mSelf->setStyleSheet(R"(
			QLabel#PropertyLabel { color: #c8ccd0; }
			QLabel#GroupTitle { color: #e0e0e0; font-weight: 600; }
		)");

        connect(&FilamentApp::instance(), &FilamentApp::selectedEntityChanged,
            mSelf, &PropertyWidget::Refresh);
        connect(&FilamentApp::instance(), &FilamentApp::sceneLoaded,
            mSelf, &PropertyWidget::Refresh);
        connect(&FilamentApp::instance(), &FilamentApp::sceneCleared,
            mSelf, &PropertyWidget::Refresh);
    }

    void Refresh() {
        while (auto* item = layout_->takeAt(0)) {
            delete item;
        }
        for (auto& [group, component] : mComponents) {
            delete group;
            delete component;
        }
        mComponents.clear();

        auto& app = FilamentApp::instance();
        const int index = app.selectedEntity();
        if (index < 0 || (size_t)index >= app.entityCount()) {
            layout_->addStretch();
            return;
        }

        auto* transform = new EntityTransformComponent((size_t)index);
        auto* renderable = new EntityRenderableComponent((size_t)index);
        PropertyComponent* comps[] = { transform, renderable };
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

    ~PropertyWidgetInternal() {
        for (auto& [group, component] : mComponents) {
            delete group;
            delete component;
        }
    }

private:
    friend class PropertyWidget;
    PropertyWidget* mSelf = nullptr;
    QVBoxLayout* layout_ = nullptr;
    std::vector<std::pair<CollapsibleGroupBoxWidget*, PropertyComponent*>> mComponents;
};

PropertyWidget::PropertyWidget(QWidget* parent)
    : QWidget(parent), mInternal(new PropertyWidgetInternal(this)) {
    mInternal->setUp();
}

void PropertyWidget::Refresh() {
    mInternal->Refresh();
}

PropertyWidget::~PropertyWidget() {
    delete mInternal;
}

} // namespace MOON

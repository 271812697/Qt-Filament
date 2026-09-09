#pragma once
#include "Widgets/PropertyQtWidgets.h"
#include "Widgets/numberwidget.h"
#include <QWidget>
#include <QHBoxLayout>
#include <QVector3D>
namespace MOON {

class Fvec3 : public PropertyQtWidget
{
    Q_OBJECT
public:
    explicit Fvec3(QWidget* parent , WidgetProperty* prop);
    explicit Fvec3(QWidget* parent);
    void setVec3Value(float x, float y, float z);
   
    virtual QVariant widgetValue()override;
    virtual void setWidgetValue(const QVariant& value) override;
    float x() const;
    float y() const;
    float z() const;

public Q_SLOTS:
    void onValueChange();
private:
    NumberWidget* m_spinX;
    NumberWidget* m_spinY;
    NumberWidget* m_spinZ;
};

}

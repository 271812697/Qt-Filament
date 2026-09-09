#pragma once
#include "Widgets/PropertyQtWidgets.h"
#include <QWidget>
#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QVector3D>
namespace MOON {
class Fvec4 : public PropertyQtWidget
{
    Q_OBJECT
public:
    explicit Fvec4(QWidget* parent , WidgetProperty* prop);
    explicit Fvec4(QWidget* parent);
    void setVec4Value(float x, float y, float z,float w);
   
    virtual QVariant widgetValue()override;
    virtual void setWidgetValue(const QVariant& value) override;
    float x() const;
    float y() const;
    float z() const;
    float w() const;
   
public Q_SLOTS:
    void onValueChange(double val);
private:
    QDoubleSpinBox* m_spinX;
    QDoubleSpinBox* m_spinY;
    QDoubleSpinBox* m_spinZ;
    QDoubleSpinBox* m_spinW;
};

}

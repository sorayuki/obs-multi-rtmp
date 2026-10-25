#pragma once

#include <functional>
#include <memory>

#include "pch.h"
#include "obs.h"

using OBSPropertiesPtr = std::shared_ptr<obs_properties_t>;

class QPropertiesWidget: virtual public QWidget {
public:
    virtual ~QPropertiesWidget() {}
    virtual void Save() = 0;
    virtual void SetGeometryChangeCallback(std::function<void()> callback) = 0;
};

QPropertiesWidget* createPropertyWidget(
    OBSPropertiesPtr props,
    obs_data* settings,
    QWidget* parent);

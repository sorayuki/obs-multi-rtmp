#pragma once

#include <functional>

#include "pch.h"
#include "obs.h"

class QPropertiesWidget: virtual public QWidget {
public:
    virtual ~QPropertiesWidget() {}
    virtual void Save() = 0;
    virtual void SetGeometryChangeCallback(std::function<void()> callback) = 0;
};

QPropertiesWidget* createPropertyWidget(obs_properties* props, obs_data* settings, QWidget* parent);

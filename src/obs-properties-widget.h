#pragma once

#include "pch.h"
#include "obs.h"

class QPropertiesWidget: virtual public QWidget {
public:
    virtual ~QPropertiesWidget() {}
    virtual void Apply() = 0;
};

QPropertiesWidget* createPropertyWidget(obs_properties* props, obs_data* settings, QWidget* parent);

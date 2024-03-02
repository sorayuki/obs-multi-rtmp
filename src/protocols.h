#pragma once

#include <QStringList>
#include <string>

extern const QStringList protocol_labels;
extern const QStringList protocol_values;

const char *GetOutputID(std::string protocol);
const char *GetServiceID(std::string protocol);
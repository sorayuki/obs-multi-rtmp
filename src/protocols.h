#pragma once

#include <QStringList>
#include <string>

QStringList protocol_labels;
QStringList protocol_values;

static const char *GetOutputID(const char *protocol);
static const char *GetServiceID(const char *protocol);
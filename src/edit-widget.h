#include "pch.h"

class EditOutputWidget : virtual public QDialog {
public:
    virtual ~EditOutputWidget() {}
    virtual QJsonObject Config() = 0;
};

EditOutputWidget* createEditOutputWidget(QJsonObject conf, QWidget* parent = 0);

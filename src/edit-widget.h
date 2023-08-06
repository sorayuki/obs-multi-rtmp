#include "pch.h"

class EditOutputWidget : virtual public QDialog {
public:
    virtual ~EditOutputWidget() {}
};

EditOutputWidget* createEditOutputWidget(const std::string& targetid, QWidget* parent = 0);

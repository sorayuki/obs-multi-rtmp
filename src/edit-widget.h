#include "pch.h"
#include "output-config.h"

#include <functional>

// Editor for a single target. Extracted from the per-target settings dialog
// so the same controls can be embedded in other dialogs (e.g. the multi-target
// editor). The panel only edits the passed working config; committing it is up
// to the owner.
class EditTargetPanel : virtual public QWidget {
public:
    virtual ~EditTargetPanel() {}
    // Writes the current widget values into the working target config.
    virtual void SaveConfig() = 0;
};

EditTargetPanel* createEditTargetPanel(
    MultiOutputConfig& config,
    OutputTargetConfigPtr target,
    std::function<void()> onContentSizeChanged,
    QWidget* parent = 0);

class EditOutputWidget : virtual public QDialog {
public:
    virtual ~EditOutputWidget() {}
};

EditOutputWidget* createEditOutputWidget(const std::string& targetid, QWidget* parent = 0);

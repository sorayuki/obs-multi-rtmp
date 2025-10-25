#include "obs-properties-widget.h"
#include "obs.hpp"

#include "json.hpp"

namespace {
    QString LoadCString(const char* s) {
        if (!s)
            return {};
        return QString::fromUtf8(s, strlen(s));
    }

    template<class T>
    QString to_qstring(T x) {
        auto s = std::to_wstring(x);
        return QString::fromStdWString(s);
    }


    struct UpdateHandler {
        virtual ~UpdateHandler() {}
        virtual void UpdateUI() = 0;
    };


    class QLineEditWithFocus : public QLineEdit {
        UpdateHandler* updater;
    public:
        template<class ...Args>
        QLineEditWithFocus(UpdateHandler* updater, QWidget* parent)
            : updater(updater)
            , QLineEdit(parent)
        {
        }        

        void focusOutEvent(QFocusEvent* e) override {
            QLineEdit::focusOutEvent(e);
            updater->UpdateUI();
        }
    };

    class QLineEditWithEye: public QWidget {
        QLineEditWithFocus* edit_;
        QCheckBox* eye_;
    public:
        QLineEditWithEye(UpdateHandler* updater, QWidget* parent, bool hasEye)
            : QWidget(parent)
        {
            auto layout = new QHBoxLayout();
            layout->addWidget(edit_ = new QLineEditWithFocus(updater, this), 1);

            if (hasEye) {
                edit_->setEchoMode(QLineEdit::EchoMode::Password);
                layout->addWidget(eye_ = new QCheckBox(u8"👀", this));

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
                QObject::connect(eye_, &QCheckBox::checkStateChanged, [=](Qt::CheckState newstate) {
#else
                QObject::connect(eye_, &QCheckBox::stateChanged, [=](int newstate) {
#endif
                    if (newstate == Qt::CheckState::Checked) {
                        edit_->setEchoMode(QLineEdit::EchoMode::Normal);
                    } else {
                        edit_->setEchoMode(QLineEdit::EchoMode::Password);
                    }
                });
            }

            layout->setContentsMargins(0, 0, 0, 0);
            setLayout(layout);

            setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);
        }

        QLineEdit* edit() const { return edit_; }
    };

    struct PropertyWidget {
        UpdateHandler* updater;

        std::string name;
        obs_property_type propType;
        obs_combo_format cbType;
        QLabel* label = nullptr;
        QWidget* ctrl = nullptr;

        PropertyWidget(QWidget* parent, UpdateHandler* updater, obs_property* p) {
            this->name = obs_property_name(p);
            this->updater = updater;

            auto str = obs_property_description(p);
            if (!str)
                str = obs_property_name(p);
            label = new QLabel(QString::fromUtf8(str, strlen(str)));

            switch(propType = obs_property_get_type(p)) {
                case OBS_PROPERTY_BOOL: {
                    auto cb = new QCheckBox(parent);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
                    QObject::connect(cb, &QCheckBox::checkStateChanged, [=](Qt::CheckState) {
#else
                    QObject::connect(cb, &QCheckBox::stateChanged, [=](int) {
#endif
                        updater->UpdateUI();
                    });
                    ctrl = cb;
                    break;
                }
                case OBS_PROPERTY_INT:
                {
                    auto le = new QLineEditWithEye(updater, parent, false);
                    le->edit()->setValidator(new QIntValidator(le));
                    ctrl = le;
                    break;
                }
                case OBS_PROPERTY_FLOAT:
                {
                    auto le = new QLineEditWithEye(updater, parent, false);
                    le->edit()->setValidator(new QDoubleValidator(le));
                    ctrl = le;
                    break;
                }
                case OBS_PROPERTY_TEXT:
                {
                    auto le = new QLineEditWithEye(updater, parent, obs_property_text_type(p) == OBS_TEXT_PASSWORD);
                    ctrl = le;
                    break;
                }
                case OBS_PROPERTY_LIST: {
                    auto cb = new QComboBox(parent);
                    QObject::connect(cb, &QComboBox::currentIndexChanged, [=]() {
                        updater->UpdateUI();
                    });
                    ctrl = cb;
                    break;
                }
                
                default:
                    ctrl = new QLabel("Unsupported", parent);
                    break;
            }

            QObject::connect(label, &QObject::destroyed, [this]() {
                label = nullptr;
            });
            QObject::connect(ctrl, &QObject::destroyed, [this]() {
                ctrl = nullptr;
            });

            ReloadProperty(p);
        }

        ~PropertyWidget() {
            if (label) 
                delete label;
            if (ctrl) 
                delete ctrl;
        }

        void ReloadProperty(obs_property* p) {
            if (obs_property_get_type(p) == propType) {
                switch(propType) {
                    case OBS_PROPERTY_LIST: {
                        auto cb = static_cast<QComboBox*>(ctrl);
                        for(int i = cb->count() - 1; i >= 0; --i)
                            cb->removeItem(i);
                        cbType = obs_property_list_format(p);
                        auto cnt = obs_property_list_item_count(p);
                        for(size_t i = 0; i < cnt; ++i) {
                            auto itemname = obs_property_list_item_name(p, i);
                            QVariant data;
                            if (cbType == obs_combo_format::OBS_COMBO_FORMAT_INT)
                                data = obs_property_list_item_int(p, i);
                            else if (cbType == obs_combo_format::OBS_COMBO_FORMAT_FLOAT)
                                data = obs_property_list_item_float(p, i);
                            else if (cbType == obs_combo_format::OBS_COMBO_FORMAT_STRING)
                                data = LoadCString(obs_property_list_item_string(p, i));
                            cb->addItem(LoadCString(itemname), data);
                        }
                    }
                    default:
                        blog(LOG_WARNING, "ReloadProperty did not handle property of type %d", propType);
                        break;
                }
            }
        }

        void LoadData(obs_data* data) {
            switch(propType) {
            case OBS_PROPERTY_BOOL: 
            {
                bool isChecked = obs_data_get_bool(data, name.c_str());
                static_cast<QCheckBox*>(ctrl)->setChecked(isChecked);
                break;
            }
            case OBS_PROPERTY_INT:
            {
                auto val = obs_data_get_int(data, name.c_str());
                static_cast<QLineEditWithEye*>(ctrl)->edit()->setText(to_qstring(val));
                break;
            }
            case OBS_PROPERTY_FLOAT:
            {
                auto val = obs_data_get_double(data, name.c_str());
                static_cast<QLineEditWithEye*>(ctrl)->edit()->setText(to_qstring(val));
                break;
            }
            case OBS_PROPERTY_TEXT:
            {
                auto val = obs_data_get_string(data, name.c_str());
                static_cast<QLineEditWithEye*>(ctrl)->edit()->setText(LoadCString(val));
                break;
            }
            case OBS_PROPERTY_LIST: {
                auto cb = static_cast<QComboBox*>(ctrl);
                QVariant v;
                if (cbType == obs_combo_format::OBS_COMBO_FORMAT_INT)
                    v = obs_data_get_int(data, name.c_str());
                else if (cbType == obs_combo_format::OBS_COMBO_FORMAT_FLOAT)
                    v = obs_data_get_double(data, name.c_str());
                else if (cbType == obs_combo_format::OBS_COMBO_FORMAT_STRING)
                    v = LoadCString(obs_data_get_string(data, name.c_str()));
                
                auto findRes = cb->findData(v);
                if (findRes >= 0) {
                    cb->setCurrentIndex(findRes);
                }
                break;
            }
            default:
                blog(LOG_ERROR, "Unsupported property type %d", propType);
                break;
            }
        }

        void SaveData(obs_data* data) {
            switch(propType) {
            case OBS_PROPERTY_BOOL:
            {
                auto val = static_cast<QCheckBox*>(ctrl)->isChecked();
                obs_data_set_bool(data, name.c_str(), val);
                break;
            }
            case OBS_PROPERTY_INT:
            {
                try {
                    auto val = tostdu8(static_cast<QLineEditWithEye*>(ctrl)->edit()->text());
                    obs_data_set_int(data, name.c_str(), std::stoi(val));
                } catch(...) {
                }
                break;
            }
            case OBS_PROPERTY_FLOAT:
            {
                try {
                    auto val = tostdu8(static_cast<QLineEditWithEye*>(ctrl)->edit()->text());
                    obs_data_set_int(data, name.c_str(), std::stod(val));
                } catch(...) {
                }
                break;
            }
            case OBS_PROPERTY_TEXT:
            {
                auto val = static_cast<QLineEditWithEye*>(ctrl)->edit()->text();
                obs_data_set_string(data, name.c_str(), tostdu8(val).c_str());
                break;
            }
            case OBS_PROPERTY_LIST: {
                auto cb = static_cast<QComboBox*>(ctrl);
                if (cb->currentIndex() >= 0) {
                    auto currentData = cb->currentData();
                    if (cbType == obs_combo_format::OBS_COMBO_FORMAT_INT)
                        obs_data_set_int(data, name.c_str(), currentData.toLongLong());
                    else if (cbType == obs_combo_format::OBS_COMBO_FORMAT_FLOAT)
                        obs_data_set_double(data, name.c_str(), currentData.toDouble());
                    else if (cbType == obs_combo_format::OBS_COMBO_FORMAT_STRING)
                        obs_data_set_string(data, name.c_str(), tostdu8(currentData.toString()).c_str());
                }
                break;
            }
            default:
                blog(LOG_ERROR, "Unsupported property type %d", propType);
                break;
            }
        }
    };


    class QPropertiesWidgetImpl: virtual public QWidget, public QPropertiesWidget, public UpdateHandler {
        std::unordered_map<std::string, std::shared_ptr<PropertyWidget>> propwids;
        obs_properties* props;
        OBSData settings;
        OBSData orig_settings;

    public:
        QPropertiesWidgetImpl(obs_properties* props, obs_data* p_settings, QWidget* parent)
            : QWidget(parent)
            , props(props)
            , orig_settings(p_settings)
        {
            obs_data_release(p_settings);

            settings = obs_data_create();
            obs_data_release(settings);

            // load default settings
            auto defs = obs_data_get_defaults(orig_settings);
            obs_data_apply(settings, defs);
            obs_data_release(defs);

            obs_data_apply(settings, orig_settings);

            obs_properties_apply_settings(props, settings);
            
            UpdateUI();
        }

        ~QPropertiesWidgetImpl()
        {
            if (props)
                obs_properties_destroy(props);
        }

        void LoadProperties() {
            std::unordered_map<std::string, std::shared_ptr<PropertyWidget>> oldpropwids;
            oldpropwids.swap(propwids);

            auto oldLayout = layout();
            if (oldLayout) {
                for (auto& x : oldpropwids) {
                    if (x.second->label)
                        oldLayout->removeWidget(x.second->label);
                    if (x.second->ctrl)
                        oldLayout->removeWidget(x.second->ctrl);
                }
            }

            auto layout = new QGridLayout();
            layout->setColumnStretch(0, 0);
            layout->setColumnStretch(1, 1);
            layout->setContentsMargins(0, 0, 0, 0);
            auto x = obs_properties_first(props);
            int currow = 0;
            do {
                if (obs_property_visible(x) == false)
                    continue;

                auto name = obs_property_name(x);
                auto it = oldpropwids.find(name);
                if (it == oldpropwids.end()) {
                    auto newwid = std::make_shared<PropertyWidget>(this, this, x);
                    newwid->LoadData(settings);
                    propwids.insert(std::make_pair(newwid->name, newwid));
                    layout->addWidget(newwid->label, currow, 0);
                    layout->addWidget(newwid->ctrl, currow, 1);
                } else {
                    it->second->ReloadProperty(x);
                    it->second->LoadData(settings);
                    propwids.insert(std::make_pair(it->first, it->second));
                    layout->addWidget(it->second->label, currow, 0);
                    layout->addWidget(it->second->ctrl, currow, 1);
                }
                ++currow;
            } while(obs_property_next(&x));

            if (oldLayout)
                delete oldLayout;
            setLayout(layout);
        }

        bool isUpdating = false;
        void UpdateUI() {
            if (isUpdating)
                return;
            isUpdating = true;

            for(auto& x: propwids)
            {
                x.second->SaveData(settings);
            }

            obs_properties_apply_settings(props, settings);
            LoadProperties();

            for(auto& x: propwids)
            {
                x.second->LoadData(settings);
            }

            isUpdating = false;
        }

        void Save() {
            obs_data_apply(orig_settings, settings);
        }
    };
};


QPropertiesWidget* createPropertyWidget(obs_properties* props, obs_data* settings, QWidget* parent) {
    return new QPropertiesWidgetImpl(props, settings, parent);
}

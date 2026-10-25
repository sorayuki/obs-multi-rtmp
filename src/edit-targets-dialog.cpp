#include "pch.h"
#include "edit-targets-dialog.h"
#include "edit-widget.h"
#include "output-config.h"

#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIODevice>
#include <QListWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

class EditTargetsDialogImpl: public QDialog
{
    MultiOutputConfig config_;
    GlobalEditSnapshot snapshot_;
    std::vector<OutputTargetConfigPtr> targets_;

    // Parallel to targets_. A page is created lazily on first selection so
    // switching targets only swaps stacked widgets and never tears down OBS
    // property widgets while a button/list signal is being handled.
    std::vector<QScrollArea*> pages_;
    std::vector<EditTargetPanel*> panels_;

    QListWidget* list_ = nullptr;
    QSplitter* splitter_ = nullptr;
    QWidget* right_ = nullptr;
    QStackedWidget* rightStack_ = nullptr;
    QLabel* placeholder_ = nullptr;
    QPushButton* deleteButton_ = nullptr;

    // Set while the list is changed programmatically, so the selection
    // handler doesn't act on intermediate states.
    bool updating_ = false;
    int currentIndex_ = -1;

    void UpdateDeleteButton()
    {
        const bool targetIsActive = currentIndex_ >= 0
            && currentIndex_ < (int)targets_.size()
            && snapshot_.activeTargetIds.find(targets_[currentIndex_]->id) != snapshot_.activeTargetIds.end();
        deleteButton_->setEnabled(currentIndex_ >= 0 && !targetIsActive);
    }

    static QString TargetName(const OutputTargetConfigPtr& target)
    {
        auto name = QString::fromUtf8(target->name.c_str());
        if (name.isEmpty())
            name = obs_module_text("NewStreaming");
        return name;
    }

    QSize InitialDialogSize(const QWidget* parent) const
    {
        QSize desired(960, 720);
        QScreen* currentScreen = parent && parent->screen() ? parent->screen() : QGuiApplication::primaryScreen();
        if (currentScreen) {
            auto available = currentScreen->availableGeometry().size();
            desired.setWidth((std::min)(desired.width(), available.width() - 80));
            desired.setHeight((std::min)(desired.height(), available.height() - 120));
        }
        desired.setWidth((std::max)(desired.width(), 560));
        desired.setHeight((std::max)(desired.height(), 420));
        return desired;
    }

    void RefreshCurrentItem()
    {
        if (currentIndex_ < 0 || currentIndex_ >= (int)targets_.size())
            return;

        auto item = list_->item(currentIndex_);
        if (item)
            item->setText(TargetName(targets_[currentIndex_]));
    }

    void CommitCurrent()
    {
        if (currentIndex_ < 0 || currentIndex_ >= (int)panels_.size())
            return;
        auto panel = panels_[currentIndex_];
        if (!panel)
            return;

        panel->SaveConfig();
        RefreshCurrentItem();
    }

    QScrollArea* EnsurePage(int row)
    {
        if (pages_[row])
            return pages_[row];

        auto page = new QScrollArea(rightStack_);
        page->setWidgetResizable(true);
        page->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        page->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        auto panel = createEditTargetPanel(config_, targets_[row], []() {}, page, &snapshot_);
        page->setWidget(panel);

        rightStack_->addWidget(page);
        pages_[row] = page;
        panels_[row] = panel;
        return page;
    }

    void SelectRow(int row)
    {
        if (row < 0 || row >= (int)targets_.size()) {
            currentIndex_ = -1;
            UpdateDeleteButton();
            rightStack_->setCurrentWidget(placeholder_);
            return;
        }

        if (row == currentIndex_) {
            if (pages_[row])
                rightStack_->setCurrentWidget(pages_[row]);
            return;
        }

        CommitCurrent();
        currentIndex_ = row;
        UpdateDeleteButton();

        auto page = EnsurePage(row);
        rightStack_->setCurrentWidget(page);
        page->verticalScrollBar()->setValue(0);
    }

    void AddTarget()
    {
        updating_ = true;
        CommitCurrent();

        auto target = std::make_shared<OutputTargetConfig>();
        target->id = GenerateId(config_);
        config_.targets.push_back(target);
        targets_.push_back(target);
        pages_.push_back(nullptr);
        panels_.push_back(nullptr);
        list_->addItem(TargetName(target));

        updating_ = false;
        list_->setCurrentRow((int)targets_.size() - 1);
    }

    void DeleteTarget()
    {
        if (currentIndex_ < 0 || currentIndex_ >= (int)targets_.size())
            return;
        if (snapshot_.activeTargetIds.find(targets_[currentIndex_]->id) != snapshot_.activeTargetIds.end())
            return;

        updating_ = true;

        // Do not commit the current panel: the target is being discarded.
        const int row = currentIndex_;
        auto target = targets_[row];
        config_.targets.remove(target);
        targets_.erase(targets_.begin() + row);
        delete list_->takeItem(row);

        if (pages_[row]) {
            rightStack_->removeWidget(pages_[row]);
            pages_[row]->hide();
            pages_[row]->deleteLater();
        }
        pages_.erase(pages_.begin() + row);
        panels_.erase(panels_.begin() + row);
        currentIndex_ = -1;
        UpdateDeleteButton();

        if (targets_.empty()) {
            rightStack_->setCurrentWidget(placeholder_);
        } else {
            const int newRow = (std::min)(row, (int)targets_.size() - 1);
            list_->setCurrentRow(newRow);
            currentIndex_ = newRow;
            UpdateDeleteButton();
            auto page = EnsurePage(newRow);
            rightStack_->setCurrentWidget(page);
            page->verticalScrollBar()->setValue(0);
        }

        updating_ = false;
    }

    void MoveTarget(int delta)
    {
        if (currentIndex_ < 0 || currentIndex_ >= (int)targets_.size())
            return;

        const int newRow = currentIndex_ + delta;
        if (newRow < 0 || newRow >= (int)targets_.size())
            return;

        updating_ = true;
        CommitCurrent();

        const int row = currentIndex_;
        auto target = targets_[row];
        auto page = pages_[row];
        auto panel = panels_[row];

        targets_.erase(targets_.begin() + row);
        targets_.insert(targets_.begin() + newRow, target);
        pages_.erase(pages_.begin() + row);
        pages_.insert(pages_.begin() + newRow, page);
        panels_.erase(panels_.begin() + row);
        panels_.insert(panels_.begin() + newRow, panel);

        std::list<OutputTargetConfigPtr> reordered;
        for (auto& item : targets_)
            reordered.push_back(item);
        config_.targets.swap(reordered);

        auto item = list_->takeItem(row);
        list_->insertItem(newRow, item);
        currentIndex_ = newRow;
        list_->setCurrentRow(newRow);
        UpdateDeleteButton();

        updating_ = false;
    }

    void ClearPages()
    {
        for (auto page : pages_) {
            if (!page)
                continue;
            rightStack_->removeWidget(page);
            page->hide();
            page->deleteLater();
        }
        pages_.clear();
        panels_.clear();
    }

    // Rebuilds the list and editor pages from config_. Used on construction and
    // after importing a config file.
    void RebuildTargets()
    {
        updating_ = true;

        ClearPages();
        targets_.assign(config_.targets.begin(), config_.targets.end());
        pages_.assign(targets_.size(), nullptr);
        panels_.assign(targets_.size(), nullptr);
        currentIndex_ = -1;
        UpdateDeleteButton();

        list_->clear();
        for (auto& target : targets_)
            list_->addItem(TargetName(target));

        if (targets_.empty()) {
            rightStack_->setCurrentWidget(placeholder_);
        } else {
            currentIndex_ = 0;
            list_->setCurrentRow(0);
            UpdateDeleteButton();
            auto page = EnsurePage(0);
            rightStack_->setCurrentWidget(page);
            page->verticalScrollBar()->setValue(0);
        }

        updating_ = false;
    }

    void ExportConfig()
    {
        CommitCurrent();

        auto fileName = QFileDialog::getSaveFileName(
            this,
            obs_module_text("Btn.ExportConfig"),
            "obs-multi-rtmp-config.json",
            "JSON (*.json)"
        );
        if (fileName.isEmpty())
            return;
        if (!fileName.endsWith(".json", Qt::CaseInsensitive))
            fileName += ".json";

        auto content = SerializeMultiOutputConfig(config_);

        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox(
                QMessageBox::Icon::Critical,
                obs_module_text("Notice.Title"),
                obs_module_text("Error.ExportConfig"),
                QMessageBox::StandardButton::Ok,
                this
            ).exec();
            return;
        }
        file.write(content.c_str(), static_cast<qint64>(content.size()));
    }

    void ImportConfig()
    {
        auto fileName = QFileDialog::getOpenFileName(
            this,
            obs_module_text("Btn.ImportConfig"),
            QString(),
            "JSON (*.json)"
        );
        if (fileName.isEmpty())
            return;

        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox(
                QMessageBox::Icon::Critical,
                obs_module_text("Notice.Title"),
                obs_module_text("Error.ImportConfig"),
                QMessageBox::StandardButton::Ok,
                this
            ).exec();
            return;
        }
        auto bytes = file.readAll();

        auto imported = DeserializeMultiOutputConfig(std::string(bytes.constData(), static_cast<size_t>(bytes.size())));
        if (!imported.has_value()) {
            QMessageBox(
                QMessageBox::Icon::Critical,
                obs_module_text("Notice.Title"),
                obs_module_text("Error.ImportConfig"),
                QMessageBox::StandardButton::Ok,
                this
            ).exec();
            return;
        }

        auto confirm = QMessageBox(
            QMessageBox::Icon::Question,
            obs_module_text("Question.Title"),
            obs_module_text("Question.Import"),
            QMessageBox::Yes | QMessageBox::No,
            this
        ).exec();
        if (confirm != QMessageBox::Yes)
            return;

        config_ = std::move(*imported);
        RebuildTargets();
    }

public:
    EditTargetsDialogImpl(MultiOutputConfig config, GlobalEditSnapshot snapshot, QWidget* parent = 0)
        : QDialog(parent)
        , config_(std::move(config))
        , snapshot_(std::move(snapshot))
    {
        setWindowTitle(obs_module_text("EditTargets.Title"));

        // left: target list with add/remove/move buttons above it and the
        // config import/export buttons below it
        auto left = new QWidget(this);
        auto leftLayout = new QVBoxLayout(left);
        leftLayout->setContentsMargins(0, 0, 0, 0);

        auto buttonLayout = new QHBoxLayout();
        auto addButton = new QPushButton("+", left);
        deleteButton_ = new QPushButton("-", left);
        auto upButton = new QPushButton(QString(QChar(0x2191)), left);
        auto downButton = new QPushButton(QString(QChar(0x2193)), left);
        addButton->setToolTip(obs_module_text("Btn.AddTarget"));
        deleteButton_->setToolTip(obs_module_text("Btn.RemoveTarget"));
        upButton->setToolTip(obs_module_text("Btn.MoveUp"));
        downButton->setToolTip(obs_module_text("Btn.MoveDown"));
        // Let the theme compute the real button size: OBS themes add
        // horizontal padding, so a fixed narrow width clips the label away.
        for (auto button : { addButton, deleteButton_, upButton, downButton })
            button->setMinimumWidth(40);
        buttonLayout->addWidget(addButton);
        buttonLayout->addWidget(deleteButton_);
        buttonLayout->addStretch(1);
        buttonLayout->addWidget(upButton);
        buttonLayout->addWidget(downButton);
        leftLayout->addLayout(buttonLayout);

        list_ = new QListWidget(left);
        list_->setSelectionMode(QAbstractItemView::SingleSelection);
        leftLayout->addWidget(list_, 1);

        // bottom-left: import/export the whole target list
        auto exportButton = new QPushButton(obs_module_text("Btn.ExportConfig"), this);
        auto importButton = new QPushButton(obs_module_text("Btn.ImportConfig"), this);

        // right: placeholder or one lazily created editor page per target
        right_ = new QWidget(this);
        auto rightLayout = new QVBoxLayout(right_);
        rightLayout->setContentsMargins(0, 0, 0, 0);

        rightStack_ = new QStackedWidget(right_);

        placeholder_ = new QLabel(obs_module_text("EditTargets.Empty"), rightStack_);
        placeholder_->setAlignment(Qt::AlignCenter);
        placeholder_->setEnabled(false);
        rightStack_->addWidget(placeholder_);

        rightLayout->addWidget(rightStack_);

        RebuildTargets();

        auto okButton = new QPushButton(obs_module_text("OK"), this);
        auto cancelButton = new QPushButton(obs_module_text("Cancel"), this);

        QObject::connect(okButton, &QPushButton::clicked, [this]() {
            CommitCurrent();
            accept();
        });
        QObject::connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
        QObject::connect(addButton, &QPushButton::clicked, this, &EditTargetsDialogImpl::AddTarget);
        QObject::connect(deleteButton_, &QPushButton::clicked, this, &EditTargetsDialogImpl::DeleteTarget);
        QObject::connect(upButton, &QPushButton::clicked, [this]() { MoveTarget(-1); });
        QObject::connect(downButton, &QPushButton::clicked, [this]() { MoveTarget(1); });
        QObject::connect(exportButton, &QPushButton::clicked, this, &EditTargetsDialogImpl::ExportConfig);
        QObject::connect(importButton, &QPushButton::clicked, this, &EditTargetsDialogImpl::ImportConfig);

        auto footer = new QHBoxLayout();
        footer->addWidget(exportButton);
        footer->addWidget(importButton);
        footer->addStretch(1);
        footer->addWidget(okButton);
        footer->addWidget(cancelButton);

        // top row: target list | editor, resizable through the splitter;
        // footer: import/export on the left, OK/Cancel on the right
        splitter_ = new QSplitter(Qt::Horizontal, this);
        splitter_->setChildrenCollapsible(false);
        splitter_->addWidget(left);
        splitter_->addWidget(right_);
        splitter_->setStretchFactor(0, 0);
        splitter_->setStretchFactor(1, 1);

        auto mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(splitter_, 1);
        mainLayout->addLayout(footer);

        QObject::connect(list_, &QListWidget::currentRowChanged, [this](int row) {
            if (updating_)
                return;
            SelectRow(row);
        });

        list_->setCurrentRow(0);
        splitter_->setSizes({ 220, 740 });
        resize(InitialDialogSize(parent));
        setMinimumSize(560, 420);
        setSizeGripEnabled(true);
    }

    MultiOutputConfig& Config()
    {
        return config_;
    }
};


std::optional<std::string> showEditTargetsDialog(
    const std::string& configJson,
    const GlobalEditSnapshot& snapshot,
    QWidget* parent)
{
    auto config = DeserializeMultiOutputConfig(configJson);
    if (!config.has_value())
        return std::nullopt;

    EditTargetsDialogImpl dialog(std::move(*config), snapshot, parent);
    if (dialog.exec() != QDialog::Accepted)
        return std::nullopt;

    return SerializeMultiOutputConfig(dialog.Config());
}

/**
 * @file mainwindow.cpp
 * @brief Implementation of main GUI window layout, events, actions, and playback.
 * @date 2026
 */

#include "mainwindow.hpp"
#include "upload_dialog.hpp"
#include "sample_chopper_dialog.hpp"
#include "volsa2/audio.hpp"
#include "volsa2/package.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMenu>
#include <QStatusBar>
#include <QFileInfo>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QInputDialog>
#include <cmath>
#include <algorithm>
#include <unordered_set>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), slots_(200), patterns_(16) {
    setWindowTitle("VolSa 2 - KORG Volca Sample 2 Manager");
    resize(960, 680);
    setAcceptDrops(true);

    for (uint8_t i = 0; i < 200; ++i) {
        slots_[i] = volsa2::SampleHeader::empty(i);
    }
    for (uint8_t i = 0; i < 16; ++i) {
        patterns_[i] = volsa2::PatternData::create(i, "Pattern " + std::to_string(i + 1));
    }

    setupUi();
    setupWorker();

    audio_player_ = new AlsaAudioPlayer(this);
    connect(audio_player_, &AlsaAudioPlayer::positionChanged, waveform_widget_, &WaveformWidget::setPlayheadPosition);
    connect(audio_player_, &AlsaAudioPlayer::playbackFinished, this, [this]() {
        btn_stop_->setEnabled(false);
        btn_play_->setEnabled(!current_samples_.empty());
    });

    slot_selection_debounce_timer_.setSingleShot(true);
    slot_selection_debounce_timer_.setInterval(200);
    connect(&slot_selection_debounce_timer_, &QTimer::timeout, this, &MainWindow::onSlotSelectionDebounced);
}

MainWindow::~MainWindow() {
    onStopClicked();
    worker_thread_.quit();
    worker_thread_.wait();
}

/**
 * @brief Constructs the dark theme layout, connection bar, table, and audio preview controls.
 */
void MainWindow::setupUi() {
    setStyleSheet(R"(
        QMainWindow {
            background-color: #19191d;
            color: #e0e0e4;
        }
        QWidget {
            color: #e0e0e4;
            font-size: 13px;
        }
        QTableWidget {
            background-color: #222228;
            alternate-background-color: #282830;
            border: 1px solid #383844;
            gridline-color: #33333e;
            selection-background-color: #f07828;
            selection-color: #ffffff;
        }
        QHeaderView::section {
            background-color: #2b2b35;
            color: #c0c0cc;
            padding: 4px;
            border: 1px solid #383844;
            font-weight: bold;
        }
        QPushButton {
            background-color: #333340;
            border: 1px solid #4a4a5a;
            border-radius: 4px;
            padding: 6px 12px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #444455;
            border-color: #f07828;
        }
        QPushButton:pressed {
            background-color: #f07828;
            color: #ffffff;
        }
        QPushButton:disabled {
            background-color: #22222a;
            border-color: #33333e;
            color: #666677;
        }
        QLineEdit {
            background-color: #262630;
            border: 1px solid #3c3c4a;
            border-radius: 4px;
            padding: 4px 8px;
            color: #ffffff;
        }
        QProgressBar {
            background-color: #262630;
            border: 1px solid #3c3c4a;
            border-radius: 4px;
            text-align: center;
            font-weight: bold;
        }
        QProgressBar::chunk {
            background-color: #f07828;
            border-radius: 3px;
        }
        QSlider::groove:horizontal {
            background: #333340;
            height: 6px;
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: #f07828;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #ffffff;
            width: 14px;
            margin: -4px 0;
            border-radius: 7px;
        }
        QTabWidget::pane {
            border: 1px solid #383844;
            background-color: #19191d;
            border-radius: 4px;
        }
        QTabBar::tab {
            background-color: #2b2b35;
            color: #a0a0b0;
            padding: 7px 18px;
            margin-right: 3px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            font-weight: bold;
        }
        QTabBar::tab:selected {
            background-color: #353545;
            color: #ffffff;
            border-bottom: 2px solid #f07828;
        }
        QTabBar::tab:hover:!selected {
            background-color: #333340;
            color: #e0e0e0;
        }
    )");

    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* main_layout = new QVBoxLayout(central);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(10);

    // --- Top Bar: Connection & Storage Info ---
    auto* top_panel = new QHBoxLayout();
    top_panel->setSpacing(12);

    led_indicator_ = new QLabel();
    led_indicator_->setFixedSize(14, 14);
    led_indicator_->setStyleSheet("background-color: #666677; border-radius: 7px;");
    top_panel->addWidget(led_indicator_);

    dev_status_label_ = new QLabel("Disconnected");
    dev_status_label_->setStyleSheet("font-weight: bold; font-size: 14px;");
    top_panel->addWidget(dev_status_label_);

    btn_connect_ = new QPushButton("Connect");
    connect(btn_connect_, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    top_panel->addWidget(btn_connect_);

    btn_refresh_ = new QPushButton("Refresh All Slots");
    btn_refresh_->setEnabled(false);
    connect(btn_refresh_, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    top_panel->addWidget(btn_refresh_);

    top_panel->addSpacing(16);

    auto* space_box = new QHBoxLayout();
    space_box->addWidget(new QLabel("Memory:"));
    space_bar_ = new QProgressBar();
    space_bar_->setRange(0, 100);
    space_bar_->setValue(0);
    space_bar_->setFormat("No device");
    space_bar_->setFixedWidth(240);
    space_box->addWidget(space_bar_);
    top_panel->addLayout(space_box);

    top_panel->addStretch();
    main_layout->addLayout(top_panel);

    // --- Main Tabs: Samples (Tab 0) & Patterns (Tab 1) ---
    main_tabs_ = new QTabWidget(central);
    connect(main_tabs_, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    // ==========================================
    // Tab 1: Samples (200 Slots)
    // ==========================================
    auto* samples_tab = new QWidget();
    auto* samples_layout = new QVBoxLayout(samples_tab);
    samples_layout->setContentsMargins(4, 8, 4, 4);
    samples_layout->setSpacing(8);

    // Action Bar: Upload, Download, Erase, Filter
    auto* action_bar = new QHBoxLayout();
    action_bar->setSpacing(8);

    btn_upload_ = new QPushButton("Upload Sample...");
    btn_upload_->setStyleSheet("QPushButton { border-color: #f07828; color: #ff9944; }");
    connect(btn_upload_, &QPushButton::clicked, this, &MainWindow::onUploadClicked);
    action_bar->addWidget(btn_upload_);

    btn_chop_ = new QPushButton("Chop / Slice...");
    btn_chop_->setStyleSheet("QPushButton { border-color: #00b4d8; color: #48cae4; }");
    connect(btn_chop_, &QPushButton::clicked, this, &MainWindow::onChopClicked);
    action_bar->addWidget(btn_chop_);

    btn_download_ = new QPushButton("Download Selected...");
    connect(btn_download_, &QPushButton::clicked, this, &MainWindow::onDownloadClicked);
    action_bar->addWidget(btn_download_);

    btn_delete_ = new QPushButton("Erase Slot");
    btn_delete_->setStyleSheet("QPushButton:hover { border-color: #ff4444; color: #ff6666; }");
    connect(btn_delete_, &QPushButton::clicked, this, &MainWindow::onDeleteClicked);
    action_bar->addWidget(btn_delete_);

    btn_download_pkg_ = new QPushButton("Download Package (.ivlcsplpreset)...");
    btn_download_pkg_->setStyleSheet("QPushButton { border-color: #7209b7; color: #c77dff; }");
    connect(btn_download_pkg_, &QPushButton::clicked, this, &MainWindow::onDownloadPackageClicked);
    action_bar->addWidget(btn_download_pkg_);

    btn_upload_pkg_ = new QPushButton("Upload Package (.ivlcsplpreset)...");
    btn_upload_pkg_->setStyleSheet("QPushButton { border-color: #06d6a0; color: #70e000; }");
    connect(btn_upload_pkg_, &QPushButton::clicked, this, &MainWindow::onUploadPackageClicked);
    action_bar->addWidget(btn_upload_pkg_);

    action_bar->addStretch();

    // Filter controls
    search_edit_ = new QLineEdit();
    search_edit_->setPlaceholderText("Filter by name or slot...");
    search_edit_->setFixedWidth(180);
    connect(search_edit_, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    action_bar->addWidget(search_edit_);

    hide_empty_check_ = new QCheckBox("Hide Empty");
    connect(hide_empty_check_, &QCheckBox::toggled, this, &MainWindow::onFilterChanged);
    action_bar->addWidget(hide_empty_check_);

    samples_layout->addLayout(action_bar);

    // Main Slots Table
    table_ = new QTableWidget(200, 7, this);
    table_->setHorizontalHeaderLabels({"Slot", "Name", "Length", "Duration", "Speed", "Level", "Status"});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);

    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    for (int i = 0; i < 200; ++i) {
        updateTableItem(i, slots_[static_cast<size_t>(i)]);
    }

    connect(table_, &QTableWidget::itemSelectionChanged, this, &MainWindow::onTableSelectionChanged);
    connect(table_, &QTableWidget::customContextMenuRequested, this, &MainWindow::onTableCustomContextMenu);

    samples_layout->addWidget(table_, 1);

    // Bottom Panel: Waveform & Playback
    auto* bottom_panel = new QVBoxLayout();
    bottom_panel->setSpacing(6);

    waveform_widget_ = new WaveformWidget(this);
    connect(waveform_widget_, &WaveformWidget::seekRequested, this, &MainWindow::onWaveformSeek);
    bottom_panel->addWidget(waveform_widget_);

    auto* play_bar = new QHBoxLayout();
    btn_play_ = new QPushButton("▶ Play");
    btn_play_->setEnabled(false);
    btn_play_->setFixedWidth(80);
    connect(btn_play_, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    play_bar->addWidget(btn_play_);

    btn_stop_ = new QPushButton("■ Stop");
    btn_stop_->setEnabled(false);
    btn_stop_->setFixedWidth(80);
    connect(btn_stop_, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    play_bar->addWidget(btn_stop_);

    sample_detail_label_ = new QLabel("Select a slot to view and audition sample.");
    sample_detail_label_->setStyleSheet("color: #bbb;");
    play_bar->addWidget(sample_detail_label_, 1);

    play_bar->addWidget(new QLabel("Vol:"));
    volume_slider_ = new QSlider(Qt::Horizontal);
    volume_slider_->setRange(0, 100);
    volume_slider_->setValue(80);
    volume_slider_->setFixedWidth(100);
    connect(volume_slider_, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);
    play_bar->addWidget(volume_slider_);

    bottom_panel->addLayout(play_bar);
    samples_layout->addLayout(bottom_panel);

    main_tabs_->addTab(samples_tab, "Samples (200)");

    // ==========================================
    // Tab 2: Patterns (16 On-Board Sequencer Slots)
    // ==========================================
    auto* patterns_tab = new QWidget();
    auto* patterns_layout = new QVBoxLayout(patterns_tab);
    patterns_layout->setContentsMargins(4, 8, 4, 4);
    patterns_layout->setSpacing(8);

    auto* pat_action_bar = new QHBoxLayout();
    pat_action_bar->setSpacing(8);

    btn_refresh_patterns_ = new QPushButton("Refresh Patterns");
    btn_refresh_patterns_->setEnabled(false);
    connect(btn_refresh_patterns_, &QPushButton::clicked, this, &MainWindow::onRefreshPatternsClicked);
    pat_action_bar->addWidget(btn_refresh_patterns_);

    btn_rename_pattern_ = new QPushButton("Rename Pattern...");
    btn_rename_pattern_->setStyleSheet("QPushButton { border-color: #f07828; color: #ff9944; }");
    btn_rename_pattern_->setEnabled(false);
    connect(btn_rename_pattern_, &QPushButton::clicked, this, &MainWindow::onRenamePatternClicked);
    pat_action_bar->addWidget(btn_rename_pattern_);

    pat_action_bar->addStretch();

    auto* pat_hint = new QLabel("Double-click a pattern or click 'Rename Pattern...' to change its name on hardware.");
    pat_hint->setStyleSheet("color: #888899; font-style: italic;");
    pat_action_bar->addWidget(pat_hint);

    patterns_layout->addLayout(pat_action_bar);

    pattern_table_ = new QTableWidget(16, 4, this);
    pattern_table_->setHorizontalHeaderLabels({"Pattern", "Name", "Payload Size", "Status"});
    pattern_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    pattern_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    pattern_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pattern_table_->setAlternatingRowColors(true);
    pattern_table_->verticalHeader()->setVisible(false);
    pattern_table_->setContextMenuPolicy(Qt::CustomContextMenu);

    pattern_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    pattern_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    pattern_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    pattern_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    for (int i = 0; i < 16; ++i) {
        updatePatternTableItem(i, patterns_[static_cast<size_t>(i)]);
    }

    connect(pattern_table_, &QTableWidget::itemSelectionChanged, this, [this]() {
        btn_rename_pattern_->setEnabled(btn_connect_->text() != "Connect" && selectedPattern() >= 0);
    });
    connect(pattern_table_, &QTableWidget::cellDoubleClicked, this, &MainWindow::onPatternDoubleClicked);
    connect(pattern_table_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        int row = pattern_table_->rowAt(pos.y());
        if (row < 0 || static_cast<size_t>(row) >= patterns_.size()) return;
        pattern_table_->selectRow(row);
        QMenu menu(this);
        auto* rename_act = menu.addAction("Rename Pattern...");
        rename_act->setEnabled(btn_connect_->text() != "Connect");
        if (menu.exec(pattern_table_->viewport()->mapToGlobal(pos)) == rename_act) {
            onPatternDoubleClicked(row, 1);
        }
    });

    patterns_layout->addWidget(pattern_table_, 1);

    main_tabs_->addTab(patterns_tab, "Patterns (16)");

    main_layout->addWidget(main_tabs_, 1);

    // Operation progress bar
    operation_progress_ = new QProgressBar();
    operation_progress_->setRange(0, 100);
    operation_progress_->setValue(0);
    operation_progress_->setFixedHeight(16);
    operation_progress_->hide();
    main_layout->addWidget(operation_progress_);

    statusBar()->showMessage("Ready.");
}

/**
 * @brief Spawns the DeviceWorker on a separate thread and binds all signal/slot connections.
 */
void MainWindow::setupWorker() {
    worker_ = new DeviceWorker();
    worker_->moveToThread(&worker_thread_);

    connect(&worker_thread_, &QThread::finished, worker_, &QObject::deleteLater);

    connect(worker_, &DeviceWorker::deviceConnected, this, &MainWindow::onWorkerConnected);
    connect(worker_, &DeviceWorker::deviceDisconnected, this, &MainWindow::onWorkerDisconnected);
    connect(worker_, &DeviceWorker::deviceError, this, &MainWindow::onWorkerError);
    connect(worker_, &DeviceWorker::spaceUpdated, this, &MainWindow::onWorkerSpaceUpdated);
    connect(worker_, &DeviceWorker::slotLoaded, this, &MainWindow::onWorkerSlotLoaded);
    connect(worker_, &DeviceWorker::allSlotsLoaded, this, &MainWindow::onWorkerAllSlotsLoaded);
    connect(worker_, &DeviceWorker::patternLoaded, this, &MainWindow::onWorkerPatternLoaded);
    connect(worker_, &DeviceWorker::allPatternsLoaded, this, &MainWindow::onWorkerAllPatternsLoaded);
    connect(worker_, &DeviceWorker::patternUploaded, this, &MainWindow::onWorkerPatternUploaded);
    connect(worker_, &DeviceWorker::progress, this, &MainWindow::onWorkerProgress);
    connect(worker_, &DeviceWorker::sampleDataReady, this, &MainWindow::onWorkerSampleDataReady);
    connect(worker_, &DeviceWorker::sampleUploaded, this, &MainWindow::onWorkerSampleUploaded);
    connect(worker_, &DeviceWorker::sampleDeleted, this, &MainWindow::onWorkerSampleDeleted);
    connect(worker_, &DeviceWorker::sampleDownloaded, this, &MainWindow::onWorkerSampleDownloaded);
    connect(worker_, &DeviceWorker::batchFinished, this, &MainWindow::onWorkerBatchFinished);

    worker_thread_.start();
}

/**
 * @brief Updates a single row in the 200-slot table.
 * @param slot Row index (0-199).
 * @param header Sample metadata.
 */
void MainWindow::updateTableItem(int slot, const volsa2::SampleHeader& header) {
    QString slot_str = QString("%1").arg(slot, 3, 10, QChar('0'));
    table_->setItem(slot, 0, new QTableWidgetItem(slot_str));

    auto* name_item = new QTableWidgetItem(QString::fromStdString(header.name));
    table_->setItem(slot, 1, name_item);

    if (header.is_empty()) {
        table_->setItem(slot, 2, new QTableWidgetItem("-"));
        table_->setItem(slot, 3, new QTableWidgetItem("-"));
        table_->setItem(slot, 4, new QTableWidgetItem("-"));
        table_->setItem(slot, 5, new QTableWidgetItem("-"));
        auto* status_item = new QTableWidgetItem("Empty");
        status_item->setForeground(QColor(110, 110, 130));
        table_->setItem(slot, 6, status_item);
    } else {
        table_->setItem(slot, 2, new QTableWidgetItem(QString::number(header.length)));
        double dur = static_cast<double>(header.length) / volsa2::VOLCA_SAMPLERATE;
        table_->setItem(slot, 3, new QTableWidgetItem(QString("%1s").arg(QString::number(dur, 'f', 2))));
        table_->setItem(slot, 4, new QTableWidgetItem(QString::number(header.speed)));
        table_->setItem(slot, 5, new QTableWidgetItem(QString::number(header.level)));
        auto* status_item = new QTableWidgetItem("Occupied");
        status_item->setForeground(QColor(240, 120, 40));
        table_->setItem(slot, 6, status_item);
    }

    for (int col = 0; col < 7; ++col) {
        if (auto* it = table_->item(slot, col)) {
            if (col != 1) {
                it->setTextAlignment(Qt::AlignCenter);
            }
        }
    }
}

std::vector<int> MainWindow::selectedSlots() const {
    std::vector<int> result;
    if (!table_ || !table_->selectionModel()) return result;
    auto indexes = table_->selectionModel()->selectedRows();
    result.reserve(indexes.size());
    for (const auto& idx : indexes) {
        if (!table_->isRowHidden(idx.row())) {
            result.push_back(idx.row());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

int MainWindow::selectedSlot() const {
    auto sel = selectedSlots();
    if (sel.empty()) return -1;
    int current = table_->currentRow();
    if (std::find(sel.begin(), sel.end(), current) != sel.end()) {
        return current;
    }
    return sel.front();
}

int MainWindow::selectedPattern() const {
    if (!pattern_table_) return -1;
    auto items = pattern_table_->selectedItems();
    if (items.isEmpty()) return -1;
    return items.first()->row();
}

void MainWindow::updatePatternTableItem(int slot, const volsa2::PatternData& pattern) {
    if (!pattern_table_ || slot < 0 || slot >= 16) return;

    QString num_str = QString("Pattern %1").arg(slot + 1, 2, 10, QChar('0'));
    pattern_table_->setItem(slot, 0, new QTableWidgetItem(num_str));

    QString name_str = pattern.name.empty() ? QString("Pattern %1").arg(slot + 1) : QString::fromStdString(pattern.name);
    auto* name_item = new QTableWidgetItem(name_str);
    pattern_table_->setItem(slot, 1, name_item);

    QString size_str = QString("%1 B").arg(pattern.raw_data.size());
    pattern_table_->setItem(slot, 2, new QTableWidgetItem(size_str));

    auto* status_item = new QTableWidgetItem(patterns_loaded_ ? "Synced" : "Default");
    status_item->setForeground(patterns_loaded_ ? QColor(76, 175, 80) : QColor(110, 110, 130));
    pattern_table_->setItem(slot, 3, status_item);

    for (int col = 0; col < 4; ++col) {
        if (auto* it = pattern_table_->item(slot, col)) {
            if (col != 1) {
                it->setTextAlignment(Qt::AlignCenter);
            }
        }
    }
}

void MainWindow::onConnectClicked() {
    if (btn_connect_->text() == "Connect") {
        statusBar()->showMessage("Connecting to KORG Volca Sample 2...");
        QMetaObject::invokeMethod(worker_, "connectDevice", Qt::QueuedConnection, Q_ARG(int, 10));
    } else {
        QMetaObject::invokeMethod(worker_, "disconnectDevice", Qt::QueuedConnection);
    }
}

void MainWindow::onDisconnectClicked() {
    QMetaObject::invokeMethod(worker_, "disconnectDevice", Qt::QueuedConnection);
}

void MainWindow::onRefreshClicked() {
    btn_refresh_->setEnabled(false);
    sample_cache_.clear();
    operation_progress_->show();
    statusBar()->showMessage("Reading sample slots from Volca...");
    QMetaObject::invokeMethod(worker_, "refreshAllSlots", Qt::QueuedConnection);
}

void MainWindow::onTableSelectionChanged() {
    auto sel = selectedSlots();
    if (sel.empty()) {
        active_preview_slot_ = -1;
        slot_selection_debounce_timer_.stop();
        pending_fetch_slot_ = -1;
        sample_detail_label_->setText("Select a slot to view and audition sample.");
        waveform_widget_->clear();
        current_samples_.clear();
        btn_play_->setEnabled(false);
        btn_stop_->setEnabled(false);

        if (btn_download_) {
            btn_download_->setText("Download Selected...");
            btn_download_->setEnabled(false);
        }
        if (btn_delete_) {
            btn_delete_->setText("Erase Slot");
            btn_delete_->setEnabled(false);
        }
        if (btn_upload_) btn_upload_->setEnabled(true);
        if (btn_chop_) btn_chop_->setEnabled(true);
        return;
    }

    // Dynamic button text & state based on selection count
    if (sel.size() > 1) {
        int occ_count = 0;
        for (int s : sel) {
            if (s >= 0 && static_cast<size_t>(s) < slots_.size() && !slots_[s].is_empty()) {
                occ_count++;
            }
        }
        if (btn_download_) {
            btn_download_->setText(QString("Download Selected (%1)...").arg(occ_count));
            btn_download_->setEnabled(occ_count > 0);
        }
        if (btn_delete_) {
            btn_delete_->setText(QString("Erase Selected (%1)...").arg(occ_count));
            btn_delete_->setEnabled(occ_count > 0);
        }
        if (btn_upload_) btn_upload_->setEnabled(false);
        if (btn_chop_) btn_chop_->setEnabled(false);
        statusBar()->showMessage(QString("%1 slots selected (%2 occupied).").arg(sel.size()).arg(occ_count));
    } else {
        int s = sel.front();
        bool is_empty = (s >= 0 && static_cast<size_t>(s) < slots_.size()) ? slots_[s].is_empty() : true;
        if (btn_download_) {
            btn_download_->setText("Download Selected...");
            btn_download_->setEnabled(!is_empty);
        }
        if (btn_delete_) {
            btn_delete_->setText("Erase Slot");
            btn_delete_->setEnabled(!is_empty);
        }
        if (btn_upload_) btn_upload_->setEnabled(true);
        if (btn_chop_) btn_chop_->setEnabled(true);
    }

    int slot = selectedSlot();
    if (slot < 0 || static_cast<size_t>(slot) >= slots_.size()) {
        return;
    }

    active_preview_slot_ = slot;
    const auto& h = slots_[static_cast<size_t>(slot)];

    if (h.is_empty()) {
        slot_selection_debounce_timer_.stop();
        pending_fetch_slot_ = -1;
        if (sel.size() > 1) {
            sample_detail_label_->setText(QString("[%1 slots selected] Auditioning Slot %2: <EMPTY>").arg(sel.size()).arg(slot));
        } else {
            sample_detail_label_->setText(QString("Slot %1: <EMPTY>").arg(slot));
        }
        waveform_widget_->clear();
        current_samples_.clear();
        btn_play_->setEnabled(false);
        btn_stop_->setEnabled(false);
    } else {
        double dur = static_cast<double>(h.length) / volsa2::VOLCA_SAMPLERATE;
        if (sel.size() > 1) {
            sample_detail_label_->setText(QString("[%1 slots selected] Auditioning Slot %2: \"%3\" | %4 samples (%5s)")
                                              .arg(sel.size())
                                              .arg(slot)
                                              .arg(QString::fromStdString(h.name))
                                              .arg(h.length)
                                              .arg(QString::number(dur, 'f', 2)));
        } else {
            sample_detail_label_->setText(QString("Slot %1: \"%2\" | %3 samples (%4s) | Spd: %5 | Lvl: %6")
                                              .arg(slot)
                                              .arg(QString::fromStdString(h.name))
                                              .arg(h.length)
                                              .arg(QString::number(dur, 'f', 2))
                                              .arg(h.speed)
                                              .arg(h.level));
        }

        // Use cached samples if already downloaded, otherwise query ALSA with debounce
        auto it = sample_cache_.find(slot);
        if (it != sample_cache_.end()) {
            slot_selection_debounce_timer_.stop();
            pending_fetch_slot_ = -1;
            current_samples_ = it->second;
            waveform_widget_->setAudioData(current_samples_, volsa2::VOLCA_SAMPLERATE);
            btn_play_->setEnabled(!current_samples_.empty());
            if (sel.size() == 1) {
                statusBar()->showMessage(QString("Slot %1 loaded from cache (%2 samples).").arg(slot).arg(current_samples_.size()));
            }
        } else {
            current_samples_.clear();
            waveform_widget_->clear();
            btn_play_->setEnabled(false);
            if (sel.size() == 1) {
                statusBar()->showMessage(QString("Slot %1 selected. Loading waveform...").arg(slot));
            }
            pending_fetch_slot_ = slot;
            slot_selection_debounce_timer_.start();
        }
    }
}

void MainWindow::onTableCustomContextMenu(const QPoint& pos) {
    int row = table_->rowAt(pos.y());
    auto sel = selectedSlots();

    if (row >= 0 && std::find(sel.begin(), sel.end(), row) == sel.end()) {
        table_->selectRow(row);
        sel = {row};
    }

    QMenu menu(this);

    if (sel.size() > 1) {
        int occ_count = 0;
        for (int s : sel) {
            if (s >= 0 && static_cast<size_t>(s) < slots_.size() && !slots_[s].is_empty()) {
                occ_count++;
            }
        }
        auto* dl_act = menu.addAction(QString("Download Selected (%1 samples)...").arg(occ_count), this, &MainWindow::onDownloadClicked);
        dl_act->setEnabled(occ_count > 0);
        auto* del_act = menu.addAction(QString("Erase Selected (%1 slots)...").arg(occ_count), this, &MainWindow::onDeleteClicked);
        del_act->setEnabled(occ_count > 0);
        menu.addSeparator();
    } else if (sel.size() == 1) {
        int slot = sel.front();
        bool is_empty = (slot >= 0 && static_cast<size_t>(slot) < slots_.size()) ? slots_[slot].is_empty() : true;
        auto* play_act = menu.addAction("▶ Play Preview", this, &MainWindow::onPlayClicked);
        play_act->setEnabled(!is_empty);
        menu.addSeparator();
        menu.addAction(QString("Upload Sample to Slot %1...").arg(slot), this, &MainWindow::onUploadClicked);
        menu.addAction("Chop / Slice Sample...", this, &MainWindow::onChopClicked);
        auto* dl_act = menu.addAction("Download to WAV...", this, &MainWindow::onDownloadClicked);
        dl_act->setEnabled(!is_empty);
        auto* del_act = menu.addAction(QString("Erase Slot %1").arg(slot), this, &MainWindow::onDeleteClicked);
        del_act->setEnabled(!is_empty);
        menu.addSeparator();
    }

    auto* sel_menu = menu.addMenu("Selection");
    sel_menu->addAction("Select All (Ctrl+A)", table_, &QTableWidget::selectAll);
    sel_menu->addAction("Select All Occupied", this, [this]() {
        table_->clearSelection();
        for (int i = 0; i < 200; ++i) {
            if (!slots_[i].is_empty() && !table_->isRowHidden(i)) {
                table_->selectRow(i);
            }
        }
    });
    sel_menu->addAction("Select All Empty", this, [this]() {
        table_->clearSelection();
        for (int i = 0; i < 200; ++i) {
            if (slots_[i].is_empty() && !table_->isRowHidden(i)) {
                table_->selectRow(i);
            }
        }
    });
    sel_menu->addAction("Invert Selection", this, [this]() {
        auto current_sel = selectedSlots();
        std::unordered_set<int> sel_set(current_sel.begin(), current_sel.end());
        table_->clearSelection();
        for (int i = 0; i < 200; ++i) {
            if (!table_->isRowHidden(i) && sel_set.find(i) == sel_set.end()) {
                table_->selectRow(i);
            }
        }
    });
    sel_menu->addAction("Clear Selection", table_, &QTableWidget::clearSelection);

    menu.exec(table_->viewport()->mapToGlobal(pos));
}

void MainWindow::onFilterChanged() {
    QString filter = search_edit_->text().trimmed().toLower();
    bool hide_empty = hide_empty_check_->isChecked();

    for (int i = 0; i < 200; ++i) {
        bool empty = slots_[static_cast<size_t>(i)].is_empty();
        QString name = QString::fromStdString(slots_[static_cast<size_t>(i)].name).toLower();
        QString slot_str = QString::number(i);

        bool matches_filter = filter.isEmpty() || name.contains(filter) || slot_str.contains(filter);
        bool visible = matches_filter && (!hide_empty || !empty);

        table_->setRowHidden(i, !visible);
    }
    onTableSelectionChanged();
}

void MainWindow::onUploadClicked() {
    int target_slot = selectedSlot();
    UploadDialog dlg(slots_, target_slot, "", this);
    if (dlg.exec() == QDialog::Accepted) {
        int slot = dlg.targetSlot();
        QString name = dlg.sampleName();
        const auto& data = dlg.audioData();

        if (dlg.backupRequested()) {
            QString backup_path = QFileDialog::getSaveFileName(
                this, "Save Backup WAV",
                QString("slot_%1_backup.wav").arg(slot),
                "WAV Files (*.wav)"
            );
            if (!backup_path.isEmpty()) {
                QMetaObject::invokeMethod(worker_, "downloadSample", Qt::BlockingQueuedConnection,
                                          Q_ARG(int, slot), Q_ARG(QString, backup_path));
            } else {
                auto confirm = QMessageBox::question(
                    this, "Backup Cancelled",
                    "Backup was cancelled. Do you still want to overwrite the slot?",
                    QMessageBox::Yes | QMessageBox::No
                );
                if (confirm != QMessageBox::Yes) {
                    return;
                }
            }
        }

        sample_cache_[slot] = data;
        operation_progress_->show();
        statusBar()->showMessage(QString("Uploading to slot %1...").arg(slot));
        QMetaObject::invokeMethod(worker_, "uploadSample", Qt::QueuedConnection,
                                  Q_ARG(int, slot), Q_ARG(QString, name),
                                  Q_ARG(std::vector<int16_t>, data));
    }
}

void MainWindow::onChopClicked() {
    int slot = selectedSlot();
    std::vector<int16_t> samples;
    QString name;

    if (slot >= 0 && !slots_[static_cast<size_t>(slot)].is_empty()) {
        const auto& h = slots_[static_cast<size_t>(slot)];
        name = QString::fromStdString(h.name.empty() ? ("sample_" + std::to_string(slot)) : h.name);
        auto it = sample_cache_.find(slot);
        if (it != sample_cache_.end()) {
            samples = it->second;
        } else if (slot == active_preview_slot_ && !current_samples_.empty()) {
            samples = current_samples_;
        } else {
            statusBar()->showMessage(QString("Loading sample %1 for chopping...").arg(slot));
            operation_progress_->show();
            QMetaObject::invokeMethod(worker_, "fetchSample", Qt::QueuedConnection, Q_ARG(int, slot));
            QMessageBox::information(this, "Chop / Slice",
                QString("Sample data for slot %1 is being downloaded from device. Please click 'Chop / Slice...' again in a moment once loaded.").arg(slot));
            return;
        }
    } else {
        auto res = QMessageBox::question(this, "Chop / Slice",
            "No occupied slot is selected. Would you like to select an audio file from your computer to chop/slice?",
            QMessageBox::Yes | QMessageBox::No);
        if (res != QMessageBox::Yes) {
            return;
        }

        QString file_path = QFileDialog::getOpenFileName(this, "Select Audio File to Chop / Slice",
            QString(), "Audio Files (*.wav *.aiff *.aif *.flac *.ogg)");
        if (file_path.isEmpty()) {
            return;
        }

        try {
            samples = volsa2::load_and_convert_audio(file_path.toStdString(), volsa2::MonoMode::Mid);
            name = QFileInfo(file_path).baseName();
            slot = -1;
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Audio Error", QString("Failed to load audio file:\n%1").arg(e.what()));
            return;
        }
    }

    if (samples.empty()) {
        QMessageBox::warning(this, "Chop / Slice", "No audio data available.");
        return;
    }

    SampleChopperDialog dlg(slots_, samples, name, slot, this);

    connect(&dlg, &SampleChopperDialog::cropAndSaveRequested, this, [this](int target_slot, const QString& sample_name, const std::vector<int16_t>& audio) {
        if (target_slot >= 0) {
            sample_cache_[target_slot] = audio;
            operation_progress_->show();
            statusBar()->showMessage(QString("Uploading cropped audio to slot %1...").arg(target_slot));
            QMetaObject::invokeMethod(worker_, "uploadSample", Qt::QueuedConnection,
                                      Q_ARG(int, target_slot), Q_ARG(QString, sample_name),
                                      Q_ARG(std::vector<int16_t>, audio));
        }
    });

    connect(&dlg, &SampleChopperDialog::batchExportRequested, this, [this](int start_slot, const QString& base_name, const std::vector<std::vector<int16_t>>& slices) {
        for (size_t i = 0; i < slices.size(); ++i) {
            int target_slot = start_slot + static_cast<int>(i);
            QString slice_name = QString("%1_%2").arg(base_name.left(18)).arg(i + 1);
            sample_cache_[target_slot] = slices[i];
            QMetaObject::invokeMethod(worker_, "uploadSample", Qt::QueuedConnection,
                                      Q_ARG(int, target_slot), Q_ARG(QString, slice_name),
                                      Q_ARG(std::vector<int16_t>, slices[i]));
        }
        operation_progress_->show();
        statusBar()->showMessage(QString("Batch exporting %1 slices starting from slot %2...").arg(slices.size()).arg(start_slot));
    });

    dlg.exec();
}

void MainWindow::onDownloadClicked() {
    auto sel = selectedSlots();
    if (sel.empty()) {
        QMessageBox::information(this, "Download", "Please select slot(s) to download.");
        return;
    }

    std::vector<int> occupied_slots;
    for (int s : sel) {
        if (s >= 0 && static_cast<size_t>(s) < slots_.size() && !slots_[s].is_empty()) {
            occupied_slots.push_back(s);
        }
    }

    if (occupied_slots.empty()) {
        QMessageBox::information(this, "Download", "All selected slots are empty.");
        return;
    }

    if (occupied_slots.size() == 1) {
        int slot = occupied_slots.front();
        const auto& h = slots_[static_cast<size_t>(slot)];
        QString default_name = QString::fromStdString(h.name.empty() ? ("sample_" + std::to_string(slot)) : h.name) + ".wav";
        QString file_path = QFileDialog::getSaveFileName(this, "Save WAV File", default_name, "WAV Files (*.wav)");
        if (!file_path.isEmpty()) {
            operation_progress_->show();
            statusBar()->showMessage(QString("Downloading slot %1 to %2...").arg(slot).arg(file_path));
            QMetaObject::invokeMethod(worker_, "downloadSample", Qt::QueuedConnection,
                                      Q_ARG(int, slot), Q_ARG(QString, file_path));
        }
    } else {
        QString dir_path = QFileDialog::getExistingDirectory(
            this,
            tr("Select Destination Directory to Export %1 Samples").arg(occupied_slots.size()),
            QString(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
        if (!dir_path.isEmpty()) {
            operation_progress_->show();
            statusBar()->showMessage(QString("Downloading %1 selected samples to %2...").arg(occupied_slots.size()).arg(dir_path));
            QMetaObject::invokeMethod(worker_, "downloadSamples", Qt::QueuedConnection,
                                      Q_ARG(std::vector<int>, occupied_slots),
                                      Q_ARG(QString, dir_path));
        }
    }
}

void MainWindow::onDeleteClicked() {
    auto sel = selectedSlots();
    if (sel.empty()) {
        QMessageBox::information(this, "Erase Slot", "Please select slot(s) to erase.");
        return;
    }

    std::vector<int> occupied_slots;
    for (int s : sel) {
        if (s >= 0 && static_cast<size_t>(s) < slots_.size() && !slots_[s].is_empty()) {
            occupied_slots.push_back(s);
        }
    }

    if (occupied_slots.empty()) {
        QMessageBox::information(this, "Erase Slot", "All selected slots are already empty.");
        return;
    }

    if (occupied_slots.size() == 1) {
        int slot = occupied_slots.front();
        const auto& h = slots_[static_cast<size_t>(slot)];
        auto reply = QMessageBox::question(
            this, "Confirm Erase",
            QString("Are you sure you want to erase slot %1 (\"%2\") from Volca?").arg(slot).arg(QString::fromStdString(h.name)),
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            sample_cache_.erase(slot);
            operation_progress_->show();
            statusBar()->showMessage(QString("Erasing slot %1...").arg(slot));
            QMetaObject::invokeMethod(worker_, "deleteSample", Qt::QueuedConnection, Q_ARG(int, slot));
        }
    } else {
        auto reply = QMessageBox::question(
            this, "Confirm Batch Erase",
            QString("Are you sure you want to erase %1 selected sample slots from Volca?").arg(occupied_slots.size()),
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            for (int s : occupied_slots) {
                sample_cache_.erase(s);
            }
            operation_progress_->show();
            statusBar()->showMessage(QString("Erasing %1 slots...").arg(occupied_slots.size()));
            QMetaObject::invokeMethod(worker_, "deleteSamples", Qt::QueuedConnection,
                                      Q_ARG(std::vector<int>, occupied_slots));
        }
    }
}

void MainWindow::onDownloadPackageClicked() {
    if (!dev_status_label_->text().contains("Connected") && !dev_status_label_->text().contains("Firmware")) {
        QMessageBox::warning(this, "Download Package", "Please connect to the Volca Sample 2 first.");
        return;
    }

    QString file_path = QFileDialog::getSaveFileName(
        this,
        "Save Preset Package",
        "volca_sample2_backup.ivlcsplpreset",
        "Volca Sample 2 Preset (*.ivlcsplpreset)"
    );
    if (file_path.isEmpty()) {
        return;
    }
    if (!file_path.endsWith(".ivlcsplpreset", Qt::CaseInsensitive)) {
        file_path += ".ivlcsplpreset";
    }

    QDialog meta_dlg(this);
    meta_dlg.setWindowTitle("Package Information");
    meta_dlg.setMinimumWidth(380);
    auto* layout = new QVBoxLayout(&meta_dlg);

    layout->addWidget(new QLabel("Preset Collection Name:"));
    auto* edit_name = new QLineEdit(&meta_dlg);
    edit_name->setText(QFileInfo(file_path).baseName());
    layout->addWidget(edit_name);

    layout->addWidget(new QLabel("Author / Creator:"));
    auto* edit_author = new QLineEdit(&meta_dlg);
    edit_author->setPlaceholderText("Optional author name");
    layout->addWidget(edit_author);

    auto* btn_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &meta_dlg);
    connect(btn_box, &QDialogButtonBox::accepted, &meta_dlg, &QDialog::accept);
    connect(btn_box, &QDialogButtonBox::rejected, &meta_dlg, &QDialog::reject);
    layout->addWidget(btn_box);

    if (meta_dlg.exec() != QDialog::Accepted) {
        return;
    }

    QString preset_name = edit_name->text().trimmed();
    if (preset_name.isEmpty()) {
        preset_name = QFileInfo(file_path).baseName();
    }
    QString author_name = edit_author->text().trimmed();

    operation_progress_->show();
    statusBar()->showMessage("Downloading package (all 16 patterns and 200 samples)...");
    QMetaObject::invokeMethod(worker_, "downloadPackage", Qt::QueuedConnection,
                              Q_ARG(QString, file_path),
                              Q_ARG(QString, preset_name),
                              Q_ARG(QString, author_name));
}

void MainWindow::onUploadPackageClicked() {
    if (!dev_status_label_->text().contains("Connected") && !dev_status_label_->text().contains("Firmware")) {
        QMessageBox::warning(this, "Upload Package", "Please connect to the Volca Sample 2 first.");
        return;
    }

    QString file_path = QFileDialog::getOpenFileName(
        this,
        "Select Preset Package to Restore",
        QString(),
        "Volca Sample 2 Preset (*.ivlcsplpreset)"
    );
    if (file_path.isEmpty()) {
        return;
    }

    volsa2::PackageData pkg;
    try {
        pkg = volsa2::load_package(file_path.toStdString());
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Package Error",
                              QString("Failed to open or parse preset package:\n%1").arg(e.what()));
        return;
    }

    int active_samples = 0;
    for (const auto& s : pkg.samples) {
        if (s.data.has_value() && !s.data->data.empty()) {
            active_samples++;
        }
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Restore Package to Volca Sample 2");
    dlg.setMinimumWidth(440);
    auto* layout = new QVBoxLayout(&dlg);

    // Package details group
    auto* info_group = new QGroupBox("Package Information", &dlg);
    auto* info_layout = new QFormLayout(info_group);
    info_layout->addRow("Collection Name:", new QLabel(QString::fromStdString(pkg.info.name.empty() ? "(none)" : pkg.info.name)));
    if (!pkg.info.author.empty()) {
        info_layout->addRow("Author / Creator:", new QLabel(QString::fromStdString(pkg.info.author)));
    }
    if (!pkg.info.date.empty()) {
        info_layout->addRow("Date Created:", new QLabel(QString::fromStdString(pkg.info.date)));
    }
    info_layout->addRow("Patterns Found:", new QLabel(QString("%1 Sequencer Patterns").arg(pkg.programs.size())));
    info_layout->addRow("Active Samples:", new QLabel(QString("%1 of 200 Slots").arg(active_samples)));
    layout->addWidget(info_group);

    // Transfer options group
    auto* opt_group = new QGroupBox("Transfer Options", &dlg);
    auto* opt_layout = new QVBoxLayout(opt_group);
    auto* chk_samples = new QCheckBox("Upload Samples (Audio and settings)", opt_group);
    chk_samples->setChecked(true);
    auto* chk_patterns = new QCheckBox("Upload Patterns (All 16 sequence memories)", opt_group);
    chk_patterns->setChecked(true);
    auto* chk_erase_empty = new QCheckBox("Clean Restore: Erase unused old slots first (frees memory)", opt_group);
    chk_erase_empty->setToolTip("Scans and erases old occupied device slots that are empty in this package before uploading audio, preventing out-of-memory errors.");
    chk_erase_empty->setChecked(true);
    opt_layout->addWidget(chk_samples);
    opt_layout->addWidget(chk_patterns);
    opt_layout->addWidget(chk_erase_empty);
    layout->addWidget(opt_group);

    // Prominent Warning
    auto* warn_label = new QLabel(
        "<div style='background-color: #3d2414; border: 1px solid #f07828; border-radius: 4px; padding: 8px; color: #ffbe76;'>"
        "<b>⚠️ Memory Overwrite Warning:</b><br/>"
        "This operation will overwrite patterns and samples in your Volca Sample 2 hardware memory."
        "</div>",
        &dlg
    );
    warn_label->setWordWrap(true);
    layout->addWidget(warn_label);

    auto* btn_box = new QDialogButtonBox(&dlg);
    auto* btn_upload = btn_box->addButton("Upload to Device", QDialogButtonBox::AcceptRole);
    btn_upload->setStyleSheet("QPushButton { background-color: #f07828; color: #ffffff; font-weight: bold; padding: 6px 14px; }");
    btn_box->addButton("Cancel", QDialogButtonBox::RejectRole);
    connect(btn_box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btn_box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(btn_box);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    bool upload_samples = chk_samples->isChecked();
    bool upload_patterns = chk_patterns->isChecked();
    bool erase_empty = chk_erase_empty->isChecked();

    if (!upload_samples && !upload_patterns) {
        QMessageBox::information(this, "Upload Package", "Neither samples nor patterns were selected to upload.");
        return;
    }

    sample_cache_.clear();
    operation_progress_->show();
    statusBar()->showMessage(QString("Uploading package %1 to Volca Sample 2...").arg(QFileInfo(file_path).fileName()));
    QMetaObject::invokeMethod(worker_, "uploadPackage", Qt::QueuedConnection,
                              Q_ARG(QString, file_path),
                              Q_ARG(bool, upload_samples),
                              Q_ARG(bool, upload_patterns),
                              Q_ARG(bool, erase_empty));
}

void MainWindow::onPlayClicked() {
    if (current_samples_.empty() || !audio_player_) return;
    btn_stop_->setEnabled(true);
    btn_play_->setEnabled(false);
    audio_player_->play(current_samples_, volsa2::VOLCA_SAMPLERATE, 0.0);
}

void MainWindow::onStopClicked() {
    if (audio_player_) {
        audio_player_->stop();
    }
    waveform_widget_->setPlayheadPosition(-1.0);
    btn_stop_->setEnabled(false);
    btn_play_->setEnabled(!current_samples_.empty());
}

void MainWindow::onVolumeChanged(int value) {
    if (audio_player_) {
        audio_player_->setVolume(value);
    }
}

void MainWindow::onWaveformSeek(double pos) {
    if (!current_samples_.empty() && audio_player_) {
        btn_stop_->setEnabled(true);
        btn_play_->setEnabled(false);
        audio_player_->play(current_samples_, volsa2::VOLCA_SAMPLERATE, pos);
    }
}

void MainWindow::onSlotSelectionDebounced() {
    if (pending_fetch_slot_ >= 0) {
        int slot = pending_fetch_slot_;
        pending_fetch_slot_ = -1;
        QMetaObject::invokeMethod(worker_, "fetchSample", Qt::QueuedConnection, Q_ARG(int, slot));
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    auto urls = event->mimeData()->urls();
    if (urls.isEmpty()) return;

    QStringList audio_files;
    QStringList valid_exts = {"wav", "aiff", "aif", "flac", "ogg"};
    for (const auto& url : urls) {
        QString fp = url.toLocalFile();
        if (!fp.isEmpty() && valid_exts.contains(QFileInfo(fp).suffix().toLower())) {
            audio_files.append(fp);
        }
    }

    if (audio_files.isEmpty()) {
        audio_files.append(urls.first().toLocalFile());
    }

    int slot_at_drop = -1;
    QPoint table_pos = table_->viewport()->mapFrom(this, event->position().toPoint());
    int row = table_->rowAt(table_pos.y());
    if (row >= 0 && row < 200) {
        slot_at_drop = row;
    } else {
        slot_at_drop = selectedSlot();
        if (slot_at_drop < 0) slot_at_drop = 0;
    }

    if (audio_files.size() == 1) {
        UploadDialog dlg(slots_, slot_at_drop, audio_files.first(), this);
        if (dlg.exec() == QDialog::Accepted) {
            int slot = dlg.targetSlot();
            QString name = dlg.sampleName();
            const auto& data = dlg.audioData();

            sample_cache_[slot] = data;
            operation_progress_->show();
            statusBar()->showMessage(QString("Uploading dropped file to slot %1...").arg(slot));
            QMetaObject::invokeMethod(worker_, "uploadSample", Qt::QueuedConnection,
                                      Q_ARG(int, slot), Q_ARG(QString, name),
                                      Q_ARG(std::vector<int16_t>, data));
        }
    } else {
        auto reply = QMessageBox::question(
            this, "Batch Upload",
            QString("Upload %1 audio files sequentially starting from slot %2?").arg(audio_files.size()).arg(slot_at_drop),
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            int cur_slot = slot_at_drop;
            for (const auto& file_path : audio_files) {
                if (cur_slot >= 200) break;
                try {
                    auto data = volsa2::load_and_convert_audio(file_path.toStdString(), volsa2::MonoMode::Mid);
                    QString name = QFileInfo(file_path).baseName().left(24);
                    sample_cache_[cur_slot] = data;
                    QMetaObject::invokeMethod(worker_, "uploadSample", Qt::QueuedConnection,
                                              Q_ARG(int, cur_slot), Q_ARG(QString, name),
                                              Q_ARG(std::vector<int16_t>, data));
                    cur_slot++;
                } catch (const std::exception& e) {
                    statusBar()->showMessage(QString("Failed to convert %1: %2").arg(file_path).arg(e.what()));
                }
            }
            operation_progress_->show();
            statusBar()->showMessage(QString("Uploading %1 files starting from slot %2...").arg(audio_files.size()).arg(slot_at_drop));
        }
    }
}

// --- Worker Handlers ---

void MainWindow::onWorkerConnected(const QString& version, int channel) {
    led_indicator_->setStyleSheet("background-color: #20e040; border-radius: 7px;");
    dev_status_label_->setText(QString("Connected (v%1, Ch %2)").arg(version).arg(channel));
    btn_connect_->setText("Disconnect");
    btn_refresh_->setEnabled(true);
    btn_refresh_patterns_->setEnabled(true);
    btn_rename_pattern_->setEnabled(selectedPattern() >= 0);
    statusBar()->showMessage("Connected to Volca Sample 2. Refreshing slot list...");

    onRefreshClicked();
}

void MainWindow::onWorkerDisconnected() {
    led_indicator_->setStyleSheet("background-color: #666677; border-radius: 7px;");
    dev_status_label_->setText("Disconnected");
    btn_connect_->setText("Connect");
    btn_refresh_->setEnabled(false);
    btn_refresh_patterns_->setEnabled(false);
    btn_rename_pattern_->setEnabled(false);
    patterns_loaded_ = false;
    for (int i = 0; i < 16 && static_cast<size_t>(i) < patterns_.size(); ++i) {
        updatePatternTableItem(i, patterns_[i]);
    }
    space_bar_->setFormat("No device");
    space_bar_->setValue(0);
    sample_cache_.clear();
    statusBar()->showMessage("Disconnected from device.");
}

void MainWindow::onWorkerError(const QString& error) {
    operation_progress_->hide();
    btn_refresh_->setEnabled(btn_connect_->text() != "Connect");
    btn_refresh_patterns_->setEnabled(btn_connect_->text() != "Connect");
    btn_rename_pattern_->setEnabled(btn_connect_->text() != "Connect" && selectedPattern() >= 0);
    statusBar()->showMessage(error, 5000);
    QMessageBox::warning(this, "Device Error", error);
}

void MainWindow::onWorkerSpaceUpdated(double occupied, int used_sectors, int all_sectors) {
    int pct = static_cast<int>(occupied * 100.0);
    space_bar_->setRange(0, all_sectors);
    space_bar_->setValue(used_sectors);
    space_bar_->setFormat(QString("%1% occupied (%2 / %3 sectors)").arg(pct).arg(used_sectors).arg(all_sectors));
}

void MainWindow::onWorkerSlotLoaded(int slot, const volsa2::SampleHeader& header) {
    if (slot >= 0 && static_cast<size_t>(slot) < slots_.size()) {
        slots_[static_cast<size_t>(slot)] = header;
        updateTableItem(slot, header);
    }
}

void MainWindow::onWorkerAllSlotsLoaded(const std::vector<volsa2::SampleHeader>& headers) {
    slots_ = headers;
    for (int i = 0; i < 200 && static_cast<size_t>(i) < headers.size(); ++i) {
        updateTableItem(i, headers[static_cast<size_t>(i)]);
    }
    btn_refresh_->setEnabled(true);
    operation_progress_->hide();
    statusBar()->showMessage("All slots refreshed.");
    onFilterChanged();
}

void MainWindow::onWorkerProgress(int current, int total, const QString& statusText) {
    operation_progress_->show();
    operation_progress_->setRange(0, total);
    operation_progress_->setValue(current);
    statusBar()->showMessage(statusText);
    if (current >= total) {
        operation_progress_->hide();
    }
}

void MainWindow::onWorkerSampleDataReady(int slot, const std::vector<int16_t>& samples) {
    sample_cache_[slot] = samples;
    if (slot == active_preview_slot_) {
        current_samples_ = samples;
        waveform_widget_->setAudioData(current_samples_, volsa2::VOLCA_SAMPLERATE);
        btn_play_->setEnabled(!current_samples_.empty());
        statusBar()->showMessage(QString("Sample %1 loaded for playback (%2 samples).").arg(slot).arg(samples.size()));
    }
}

void MainWindow::onWorkerSampleUploaded(int slot, const QString& name) {
    operation_progress_->hide();
    statusBar()->showMessage(QString("Sample \"%1\" uploaded to slot %2.").arg(name).arg(slot), 4000);

    // Refresh slot header in cached slot list if audio is known in cache
    auto it = sample_cache_.find(slot);
    if (it != sample_cache_.end() && slot >= 0 && static_cast<size_t>(slot) < slots_.size()) {
        slots_[slot].name = name.toStdString();
        slots_[slot].length = static_cast<uint32_t>(it->second.size());
        updateTableItem(slot, slots_[slot]);
    }

    // If this slot is the active preview slot or currently selected, immediately update waveform and playback!
    if (slot == selectedSlot() || slot == active_preview_slot_) {
        active_preview_slot_ = slot;
        if (it != sample_cache_.end()) {
            current_samples_ = it->second;
            waveform_widget_->setAudioData(current_samples_, volsa2::VOLCA_SAMPLERATE);
            waveform_widget_->clearSelection();
            btn_play_->setEnabled(!current_samples_.empty());
            double dur = static_cast<double>(current_samples_.size()) / volsa2::VOLCA_SAMPLERATE;
            sample_detail_label_->setText(QString("Slot %1: \"%2\" | %3 samples (%4s)")
                                              .arg(slot)
                                              .arg(name)
                                              .arg(current_samples_.size())
                                              .arg(QString::number(dur, 'f', 2)));
        }
    }

    table_->selectRow(slot);
}

void MainWindow::onWorkerSampleDeleted(int slot) {
    sample_cache_.erase(slot);
    operation_progress_->hide();
    statusBar()->showMessage(QString("Slot %1 erased.").arg(slot), 4000);
    if (active_preview_slot_ == slot) {
        waveform_widget_->clear();
        current_samples_.clear();
        btn_play_->setEnabled(false);
    }
}

void MainWindow::onWorkerSampleDownloaded(int slot, const QString& filePath) {
    operation_progress_->hide();
    statusBar()->showMessage(QString("Slot %1 saved to %2").arg(slot).arg(filePath), 4000);
    QMessageBox::information(this, "Download Complete", QString("Sample saved to:\n%1").arg(filePath));
}

void MainWindow::onWorkerBatchFinished(const QString& message) {
    operation_progress_->hide();
    statusBar()->showMessage(message, 5000);
    QMessageBox::information(this, "Operation Complete", message);
}

// --- Pattern Operations & Slots ---

void MainWindow::onTabChanged(int index) {
    if (index == 1) { // Patterns tab
        if (!patterns_loaded_ && btn_connect_->text() != "Connect") {
            onRefreshPatternsClicked();
        }
        btn_rename_pattern_->setEnabled(btn_connect_->text() != "Connect" && selectedPattern() >= 0);
    }
}

void MainWindow::onRefreshPatternsClicked() {
    if (btn_connect_->text() == "Connect") {
        QMessageBox::information(this, "Not Connected", "Please connect to the Volca Sample 2 first.");
        return;
    }
    btn_refresh_patterns_->setEnabled(false);
    btn_rename_pattern_->setEnabled(false);
    operation_progress_->show();
    statusBar()->showMessage("Reading sequencer patterns from Volca...");
    QMetaObject::invokeMethod(worker_, "refreshAllPatterns", Qt::QueuedConnection);
}

void MainWindow::onRenamePatternClicked() {
    int row = selectedPattern();
    if (row < 0 || static_cast<size_t>(row) >= patterns_.size()) {
        QMessageBox::information(this, "Select Pattern", "Please select a pattern to rename.");
        return;
    }
    onPatternDoubleClicked(row, 1);
}

void MainWindow::onPatternDoubleClicked(int row, int /*col*/) {
    if (btn_connect_->text() == "Connect") {
        QMessageBox::information(this, "Not Connected", "Please connect to the Volca Sample 2 first.");
        return;
    }
    if (row < 0 || static_cast<size_t>(row) >= patterns_.size()) {
        return;
    }

    QString current_name = QString::fromStdString(patterns_[row].name);
    if (current_name.isEmpty()) {
        current_name = QString("Pattern %1").arg(row + 1);
    }

    bool ok = false;
    QString new_name = QInputDialog::getText(
        this,
        tr("Rename Pattern %1").arg(row + 1),
        tr("Enter new pattern name (max 48 chars):"),
        QLineEdit::Normal,
        current_name,
        &ok
    );

    if (ok && !new_name.trimmed().isEmpty()) {
        new_name = new_name.trimmed();
        if (new_name.length() > 48) {
            new_name = new_name.left(48);
        }

        // Recreate pattern with new name preserving existing raw sequence binary
        patterns_[row] = volsa2::PatternData::create(
            static_cast<uint8_t>(row),
            new_name.toStdString(),
            patterns_[row].raw_data
        );

        updatePatternTableItem(row, patterns_[row]);
        operation_progress_->show();
        statusBar()->showMessage(QString("Writing pattern %1 to Volca...").arg(row + 1));

        QMetaObject::invokeMethod(
            worker_,
            "uploadPattern",
            Qt::QueuedConnection,
            Q_ARG(int, row),
            Q_ARG(volsa2::PatternData, patterns_[row])
        );
    }
}

void MainWindow::onWorkerPatternLoaded(int slot, const volsa2::PatternData& pattern) {
    if (slot >= 0 && static_cast<size_t>(slot) < patterns_.size()) {
        patterns_[static_cast<size_t>(slot)] = pattern;
        updatePatternTableItem(slot, pattern);
    }
}

void MainWindow::onWorkerAllPatternsLoaded(const std::vector<volsa2::PatternData>& patterns) {
    patterns_ = patterns;
    patterns_loaded_ = true;
    for (int i = 0; i < 16 && static_cast<size_t>(i) < patterns.size(); ++i) {
        updatePatternTableItem(i, patterns[static_cast<size_t>(i)]);
    }
    btn_refresh_patterns_->setEnabled(true);
    btn_rename_pattern_->setEnabled(selectedPattern() >= 0);
    operation_progress_->hide();
    statusBar()->showMessage("All 16 patterns loaded.");
}

void MainWindow::onWorkerPatternUploaded(int slot, const QString& name) {
    operation_progress_->hide();
    btn_rename_pattern_->setEnabled(selectedPattern() >= 0);
    statusBar()->showMessage(QString("Pattern %1 (\"%2\") uploaded to device.").arg(slot + 1).arg(name), 4000);
}

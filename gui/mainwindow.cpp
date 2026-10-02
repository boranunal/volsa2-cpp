/**
 * @file mainwindow.cpp
 * @brief Implementation of main GUI window layout, events, actions, and playback.
 * @author Volsa2 Project Team
 * @date 2026
 */

#include "mainwindow.hpp"
#include "upload_dialog.hpp"
#include "volsa2/audio.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMenu>
#include <QStatusBar>
#include <QFileInfo>
#include <cmath>
#include <algorithm>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), slots_(200) {
    setWindowTitle("VolSa 2 - KORG Volca Sample 2 Manager");
    resize(960, 680);
    setAcceptDrops(true);

    for (uint8_t i = 0; i < 200; ++i) {
        slots_[i] = volsa2::SampleHeader::empty(i);
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

    // --- Action Bar: Upload, Download, Erase, Filter ---
    auto* action_bar = new QHBoxLayout();
    action_bar->setSpacing(8);

    auto* btn_upload = new QPushButton("Upload Sample...");
    btn_upload->setStyleSheet("QPushButton { border-color: #f07828; color: #ff9944; }");
    connect(btn_upload, &QPushButton::clicked, this, &MainWindow::onUploadClicked);
    action_bar->addWidget(btn_upload);

    auto* btn_download = new QPushButton("Download Selected...");
    connect(btn_download, &QPushButton::clicked, this, &MainWindow::onDownloadClicked);
    action_bar->addWidget(btn_download);

    auto* btn_delete = new QPushButton("Erase Slot");
    btn_delete->setStyleSheet("QPushButton:hover { border-color: #ff4444; color: #ff6666; }");
    connect(btn_delete, &QPushButton::clicked, this, &MainWindow::onDeleteClicked);
    action_bar->addWidget(btn_delete);

    auto* btn_export_all = new QPushButton("Export All Occupied...");
    connect(btn_export_all, &QPushButton::clicked, this, &MainWindow::onExportAllClicked);
    action_bar->addWidget(btn_export_all);

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

    main_layout->addLayout(action_bar);

    // --- Main Slots Table ---
    table_ = new QTableWidget(200, 7, this);
    table_->setHorizontalHeaderLabels({"Slot", "Name", "Length", "Duration", "Speed", "Level", "Status"});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
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

    main_layout->addWidget(table_, 1);

    // --- Bottom Panel: Waveform & Playback ---
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
    main_layout->addLayout(bottom_panel);

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

int MainWindow::selectedSlot() const {
    auto items = table_->selectedItems();
    if (items.isEmpty()) return -1;
    return items.first()->row();
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
    int slot = selectedSlot();
    if (slot < 0 || static_cast<size_t>(slot) >= slots_.size()) {
        return;
    }

    active_preview_slot_ = slot;
    const auto& h = slots_[static_cast<size_t>(slot)];

    if (h.is_empty()) {
        slot_selection_debounce_timer_.stop();
        pending_fetch_slot_ = -1;
        sample_detail_label_->setText(QString("Slot %1: <EMPTY>").arg(slot));
        waveform_widget_->clear();
        current_samples_.clear();
        btn_play_->setEnabled(false);
        btn_stop_->setEnabled(false);
    } else {
        double dur = static_cast<double>(h.length) / volsa2::VOLCA_SAMPLERATE;
        sample_detail_label_->setText(QString("Slot %1: \"%2\" | %3 samples (%4s) | Spd: %5 | Lvl: %6")
                                          .arg(slot)
                                          .arg(QString::fromStdString(h.name))
                                          .arg(h.length)
                                          .arg(QString::number(dur, 'f', 2))
                                          .arg(h.speed)
                                          .arg(h.level));

        // Use cached samples if already downloaded, otherwise query ALSA with debounce
        auto it = sample_cache_.find(slot);
        if (it != sample_cache_.end()) {
            slot_selection_debounce_timer_.stop();
            pending_fetch_slot_ = -1;
            current_samples_ = it->second;
            waveform_widget_->setAudioData(current_samples_, volsa2::VOLCA_SAMPLERATE);
            btn_play_->setEnabled(!current_samples_.empty());
            statusBar()->showMessage(QString("Slot %1 loaded from cache (%2 samples).").arg(slot).arg(current_samples_.size()));
        } else {
            current_samples_.clear();
            waveform_widget_->clear();
            btn_play_->setEnabled(false);
            statusBar()->showMessage(QString("Slot %1 selected. Loading waveform...").arg(slot));
            pending_fetch_slot_ = slot;
            slot_selection_debounce_timer_.start();
        }
    }
}

void MainWindow::onTableCustomContextMenu(const QPoint& pos) {
    int row = table_->rowAt(pos.y());
    if (row < 0) return;
    table_->selectRow(row);

    QMenu menu(this);
    menu.addAction("▶ Play Preview", this, &MainWindow::onPlayClicked);
    menu.addSeparator();
    menu.addAction("Upload Sample to Slot...", this, &MainWindow::onUploadClicked);
    menu.addAction("Download to WAV...", this, &MainWindow::onDownloadClicked);
    menu.addAction("Erase Slot", this, &MainWindow::onDeleteClicked);

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

        sample_cache_.erase(slot);
        operation_progress_->show();
        statusBar()->showMessage(QString("Uploading to slot %1...").arg(slot));
        QMetaObject::invokeMethod(worker_, "uploadSample", Qt::QueuedConnection,
                                  Q_ARG(int, slot), Q_ARG(QString, name),
                                  Q_ARG(std::vector<int16_t>, data));
    }
}

void MainWindow::onDownloadClicked() {
    int slot = selectedSlot();
    if (slot < 0) {
        QMessageBox::information(this, "Download", "Please select a slot to download.");
        return;
    }
    const auto& h = slots_[static_cast<size_t>(slot)];
    if (h.is_empty()) {
        QMessageBox::information(this, "Download", "Selected slot is empty.");
        return;
    }

    QString default_name = QString::fromStdString(h.name.empty() ? ("sample_" + std::to_string(slot)) : h.name) + ".wav";
    QString file_path = QFileDialog::getSaveFileName(this, "Save WAV File", default_name, "WAV Files (*.wav)");
    if (!file_path.isEmpty()) {
        operation_progress_->show();
        statusBar()->showMessage(QString("Downloading slot %1 to %2...").arg(slot).arg(file_path));
        QMetaObject::invokeMethod(worker_, "downloadSample", Qt::QueuedConnection,
                                  Q_ARG(int, slot), Q_ARG(QString, file_path));
    }
}

void MainWindow::onDeleteClicked() {
    int slot = selectedSlot();
    if (slot < 0) {
        QMessageBox::information(this, "Erase Slot", "Please select a slot to erase.");
        return;
    }

    const auto& h = slots_[static_cast<size_t>(slot)];
    if (h.is_empty()) {
        QMessageBox::information(this, "Erase Slot", "Slot is already empty.");
        return;
    }

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
}

void MainWindow::onExportAllClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Destination Directory to Export All Samples");
    if (!dir.isEmpty()) {
        operation_progress_->show();
        statusBar()->showMessage("Exporting all occupied samples...");
        QMetaObject::invokeMethod(worker_, "downloadAllSamples", Qt::QueuedConnection, Q_ARG(QString, dir));
    }
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

    QString file_path = urls.first().toLocalFile();
    if (file_path.isEmpty()) return;

    int slot_at_drop = -1;
    QPoint table_pos = table_->viewport()->mapFrom(this, event->position().toPoint());
    int row = table_->rowAt(table_pos.y());
    if (row >= 0 && row < 200) {
        slot_at_drop = row;
    } else {
        slot_at_drop = selectedSlot();
    }

    UploadDialog dlg(slots_, slot_at_drop, file_path, this);
    if (dlg.exec() == QDialog::Accepted) {
        int slot = dlg.targetSlot();
        QString name = dlg.sampleName();
        const auto& data = dlg.audioData();

        sample_cache_.erase(slot);
        operation_progress_->show();
        statusBar()->showMessage(QString("Uploading dropped file to slot %1...").arg(slot));
        QMetaObject::invokeMethod(worker_, "uploadSample", Qt::QueuedConnection,
                                  Q_ARG(int, slot), Q_ARG(QString, name),
                                  Q_ARG(std::vector<int16_t>, data));
    }
}

// --- Worker Handlers ---

void MainWindow::onWorkerConnected(const QString& version, int channel) {
    led_indicator_->setStyleSheet("background-color: #20e040; border-radius: 7px;");
    dev_status_label_->setText(QString("Connected (v%1, Ch %2)").arg(version).arg(channel));
    btn_connect_->setText("Disconnect");
    btn_refresh_->setEnabled(true);
    statusBar()->showMessage("Connected to Volca Sample 2. Refreshing slot list...");

    onRefreshClicked();
}

void MainWindow::onWorkerDisconnected() {
    led_indicator_->setStyleSheet("background-color: #666677; border-radius: 7px;");
    dev_status_label_->setText("Disconnected");
    btn_connect_->setText("Connect");
    btn_refresh_->setEnabled(false);
    space_bar_->setFormat("No device");
    space_bar_->setValue(0);
    sample_cache_.clear();
    statusBar()->showMessage("Disconnected from device.");
}

void MainWindow::onWorkerError(const QString& error) {
    operation_progress_->hide();
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
    sample_cache_.erase(slot);
    operation_progress_->hide();
    statusBar()->showMessage(QString("Sample \"%1\" uploaded to slot %2.").arg(name).arg(slot), 4000);
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
    QMessageBox::information(this, "Export Complete", message);
}

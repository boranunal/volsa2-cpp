/**
 * @file upload_dialog.cpp
 * @brief Implementation of sample upload wizard dialog with interactive sample chopping.
 * @date 2026
 */

#include "upload_dialog.hpp"
#include "volsa2/audio.hpp"
#include "volsa2/sample_chopper.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

UploadDialog::UploadDialog(const std::vector<volsa2::SampleHeader>& current_slots,
                           int initial_slot,
                           const QString& initial_file,
                           QWidget* parent)
    : QDialog(parent), current_slots_(current_slots) {
    setWindowTitle("Upload Sample to Volca Sample 2");
    setMinimumWidth(680);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(10);

    auto* grid = new QGridLayout();
    grid->setSpacing(8);

    // 1. File selection row
    grid->addWidget(new QLabel("Audio File:"), 0, 0);
    file_path_edit_ = new QLineEdit();
    file_path_edit_->setPlaceholderText("Select WAV, AIFF, FLAC, MP3, etc.");
    grid->addWidget(file_path_edit_, 0, 1);
    auto* browse_btn = new QPushButton("Browse...");
    connect(browse_btn, &QPushButton::clicked, this, &UploadDialog::onBrowseFile);
    grid->addWidget(browse_btn, 0, 2);

    // 2. Target Slot row
    grid->addWidget(new QLabel("Target Slot (0-199):"), 1, 0);
    auto* slot_box = new QHBoxLayout();
    slot_spin_ = new QSpinBox();
    slot_spin_->setRange(0, 199);
    btn_find_empty_ = new QPushButton("Find First Empty");
    connect(btn_find_empty_, &QPushButton::clicked, this, &UploadDialog::onFindEmptySlot);
    slot_box->addWidget(slot_spin_);
    slot_box->addWidget(btn_find_empty_);
    grid->addLayout(slot_box, 1, 1);

    // 3. Sample Name row
    grid->addWidget(new QLabel("Sample Name:"), 2, 0);
    name_edit_ = new QLineEdit();
    name_edit_->setMaxLength(volsa2::SampleHeader::NAME_LEN);
    grid->addWidget(name_edit_, 2, 1);

    // 4. Mono Mode selector row
    grid->addWidget(new QLabel("Mono Mode:"), 3, 0);
    mono_combo_ = new QComboBox();
    mono_combo_->addItem("Mid (Mono Mix (L+R)/2)", static_cast<int>(volsa2::MonoMode::Mid));
    mono_combo_->addItem("Left Channel", static_cast<int>(volsa2::MonoMode::Left));
    mono_combo_->addItem("Right Channel", static_cast<int>(volsa2::MonoMode::Right));
    mono_combo_->addItem("Side Channel ((L-R)/2)", static_cast<int>(volsa2::MonoMode::Side));
    grid->addWidget(mono_combo_, 3, 1);

    main_layout->addLayout(grid);

    // Specifications & diagnostic readout
    info_label_ = new QLabel("No file selected.");
    info_label_->setStyleSheet("color: #aaa; font-style: italic;");
    main_layout->addWidget(info_label_);

    // Overwrite warning banner
    overwrite_warning_ = new QLabel("");
    overwrite_warning_->setStyleSheet("color: #ff9944; font-weight: bold;");
    overwrite_warning_->setWordWrap(true);
    overwrite_warning_->hide();
    main_layout->addWidget(overwrite_warning_);

    // Automatic backup toggle
    backup_checkbox_ = new QCheckBox("Backup current slot audio to disk before overwriting");
    backup_checkbox_->setChecked(true);
    backup_checkbox_->hide();
    main_layout->addWidget(backup_checkbox_);

    // Waveform Preview with Crop Handles
    main_layout->addWidget(new QLabel("Converted Audio & Region Chopping (Drag [S] / [E] handles to crop):"));
    waveform_widget_ = new WaveformWidget(this);
    waveform_widget_->setMinimumHeight(100);
    main_layout->addWidget(waveform_widget_);

    // Chopping controls bar
    auto* chop_bar = new QHBoxLayout();
    chop_bar->addWidget(new QLabel("Crop Start:"));
    start_crop_spin_ = new QDoubleSpinBox();
    start_crop_spin_->setRange(0.0, 3600.0);
    start_crop_spin_->setDecimals(3);
    start_crop_spin_->setSingleStep(0.010);
    start_crop_spin_->setSuffix(" s");
    connect(start_crop_spin_, &QDoubleSpinBox::valueChanged, this, &UploadDialog::onCropSpinChanged);
    chop_bar->addWidget(start_crop_spin_);

    chop_bar->addWidget(new QLabel("Crop End:"));
    end_crop_spin_ = new QDoubleSpinBox();
    end_crop_spin_->setRange(0.0, 3600.0);
    end_crop_spin_->setDecimals(3);
    end_crop_spin_->setSingleStep(0.010);
    end_crop_spin_->setSuffix(" s");
    connect(end_crop_spin_, &QDoubleSpinBox::valueChanged, this, &UploadDialog::onCropSpinChanged);
    chop_bar->addWidget(end_crop_spin_);

    btn_auto_trim_ = new QPushButton("Auto-Trim Silence");
    btn_auto_trim_->setToolTip("Scans audio and crops leading/trailing silence automatically.");
    connect(btn_auto_trim_, &QPushButton::clicked, this, &UploadDialog::onAutoTrimSilence);
    chop_bar->addWidget(btn_auto_trim_);

    btn_reset_crop_ = new QPushButton("Reset Crop");
    connect(btn_reset_crop_, &QPushButton::clicked, this, &UploadDialog::onResetCrop);
    chop_bar->addWidget(btn_reset_crop_);

    chop_bar->addStretch();
    main_layout->addLayout(chop_bar);

    crop_info_label_ = new QLabel("");
    crop_info_label_->setStyleSheet("color: #66ccaa; font-weight: bold;");
    main_layout->addWidget(crop_info_label_);

    // Audition playback button
    auto* preview_bar = new QHBoxLayout();
    btn_play_ = new QPushButton("Play Preview");
    btn_play_->setEnabled(false);
    connect(btn_play_, &QPushButton::clicked, this, &UploadDialog::onPlayPreview);

    connect(&audio_player_, &AlsaAudioPlayer::positionChanged, this, [this](double norm_pos) {
        if (converted_samples_.empty()) return;
        size_t s = static_cast<size_t>(std::clamp(std::round(start_crop_spin_->value() * volsa2::VOLCA_SAMPLERATE), 0.0, static_cast<double>(converted_samples_.size())));
        size_t e = static_cast<size_t>(std::clamp(std::round(end_crop_spin_->value() * volsa2::VOLCA_SAMPLERATE), 0.0, static_cast<double>(converted_samples_.size())));
        if (e <= s) {
            waveform_widget_->setPlayheadPosition(norm_pos);
            return;
        }
        double full_norm = (static_cast<double>(s) + norm_pos * (e - s)) / converted_samples_.size();
        waveform_widget_->setPlayheadPosition(full_norm);
    });
    connect(&audio_player_, &AlsaAudioPlayer::playbackFinished, this, [this]() {
        btn_play_->setText("Play Preview");
        waveform_widget_->setPlayheadPosition(-1.0);
    });
    preview_bar->addWidget(btn_play_);
    preview_bar->addStretch();
    main_layout->addLayout(preview_bar);

    // Action button row
    auto* btn_box = new QHBoxLayout();
    btn_box->addStretch();
    auto* cancel_btn = new QPushButton("Cancel");
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    btn_upload_ = new QPushButton("Upload");
    btn_upload_->setEnabled(false);
    btn_upload_->setDefault(true);
    btn_upload_->setStyleSheet("background-color: #f07828; color: white; font-weight: bold; padding: 6px 16px;");
    connect(btn_upload_, &QPushButton::clicked, this, &QDialog::accept);

    btn_box->addWidget(cancel_btn);
    btn_box->addWidget(btn_upload_);
    main_layout->addLayout(btn_box);

    connect(slot_spin_, &QSpinBox::valueChanged, this, &UploadDialog::onSlotChanged);
    connect(mono_combo_, &QComboBox::currentIndexChanged, this, &UploadDialog::onMonoModeChanged);
    connect(file_path_edit_, &QLineEdit::textChanged, this, &UploadDialog::updateFileInfoAndConversion);
    connect(waveform_widget_, &WaveformWidget::selectionChanged, this, &UploadDialog::onWaveformSelectionChanged);

    if (initial_slot >= 0) {
        slot_spin_->setValue(initial_slot);
    } else {
        onFindEmptySlot();
    }

    if (!initial_file.isEmpty()) {
        file_path_edit_->setText(initial_file);
    }

    onSlotChanged(slot_spin_->value());
}

int UploadDialog::targetSlot() const {
    return slot_spin_->value();
}

QString UploadDialog::sampleName() const {
    return name_edit_->text().trimmed();
}

std::vector<int16_t> UploadDialog::audioData() const {
    if (converted_samples_.empty()) return {};
    size_t s = static_cast<size_t>(std::clamp(std::round(start_crop_spin_->value() * volsa2::VOLCA_SAMPLERATE), 0.0, static_cast<double>(converted_samples_.size())));
    size_t e = static_cast<size_t>(std::clamp(std::round(end_crop_spin_->value() * volsa2::VOLCA_SAMPLERATE), 0.0, static_cast<double>(converted_samples_.size())));
    return volsa2::crop_audio(converted_samples_, s, e, true);
}

bool UploadDialog::backupRequested() const {
    return backup_checkbox_->isVisible() && backup_checkbox_->isChecked();
}

void UploadDialog::onBrowseFile() {
    QString path = QFileDialog::getOpenFileName(
        this, "Select Audio File", "",
        "Audio Files (*.wav *.wave *.aif *.aiff *.flac *.ogg *.mp3);;All Files (*)"
    );
    if (!path.isEmpty()) {
        file_path_edit_->setText(path);
    }
}

void UploadDialog::onFindEmptySlot() {
    for (size_t i = 0; i < current_slots_.size(); ++i) {
        if (current_slots_[i].is_empty()) {
            slot_spin_->setValue(static_cast<int>(i));
            return;
        }
    }
}

void UploadDialog::onSlotChanged(int slot) {
    if (slot >= 0 && static_cast<size_t>(slot) < current_slots_.size()) {
        const auto& h = current_slots_[slot];
        if (!h.is_empty()) {
            overwrite_warning_->setText(QString("Warning: Slot %1 is already occupied by \"%2\" (length: %3 samples).")
                                            .arg(slot)
                                            .arg(QString::fromStdString(h.name))
                                            .arg(h.length));
            overwrite_warning_->show();
            backup_checkbox_->show();
        } else {
            overwrite_warning_->hide();
            backup_checkbox_->hide();
        }
    }
}

void UploadDialog::onMonoModeChanged() {
    updateFileInfoAndConversion();
}

void UploadDialog::onWaveformSelectionChanged(size_t start, size_t end) {
    double s_sec = static_cast<double>(start) / volsa2::VOLCA_SAMPLERATE;
    double e_sec = static_cast<double>(end) / volsa2::VOLCA_SAMPLERATE;
    start_crop_spin_->blockSignals(true);
    end_crop_spin_->blockSignals(true);
    start_crop_spin_->setValue(s_sec);
    end_crop_spin_->setValue(e_sec);
    start_crop_spin_->blockSignals(false);
    end_crop_spin_->blockSignals(false);
    updateCropReadout();
}

void UploadDialog::onCropSpinChanged() {
    double s = start_crop_spin_->value();
    double e = end_crop_spin_->value();
    if (s > e) {
        start_crop_spin_->blockSignals(true);
        start_crop_spin_->setValue(e);
        start_crop_spin_->blockSignals(false);
        s = e;
    }
    size_t s_smpls = static_cast<size_t>(std::clamp(std::round(s * volsa2::VOLCA_SAMPLERATE), 0.0, static_cast<double>(converted_samples_.size())));
    size_t e_smpls = static_cast<size_t>(std::clamp(std::round(e * volsa2::VOLCA_SAMPLERATE), 0.0, static_cast<double>(converted_samples_.size())));
    waveform_widget_->setSelection(s_smpls, e_smpls);
    updateCropReadout();
}

void UploadDialog::onAutoTrimSilence() {
    if (converted_samples_.empty()) return;
    auto [start_bound, end_bound] = volsa2::find_silence_bounds(converted_samples_, -48.0);
    double s_sec = static_cast<double>(start_bound) / volsa2::VOLCA_SAMPLERATE;
    double e_sec = static_cast<double>(end_bound) / volsa2::VOLCA_SAMPLERATE;

    start_crop_spin_->blockSignals(true);
    end_crop_spin_->blockSignals(true);
    start_crop_spin_->setValue(s_sec);
    end_crop_spin_->setValue(e_sec);
    start_crop_spin_->blockSignals(false);
    end_crop_spin_->blockSignals(false);

    waveform_widget_->setSelection(start_bound, end_bound);
    updateCropReadout();
}

void UploadDialog::onResetCrop() {
    if (converted_samples_.empty()) return;
    double total_sec = static_cast<double>(converted_samples_.size()) / volsa2::VOLCA_SAMPLERATE;
    start_crop_spin_->blockSignals(true);
    end_crop_spin_->blockSignals(true);
    start_crop_spin_->setValue(0.0);
    end_crop_spin_->setValue(total_sec);
    start_crop_spin_->blockSignals(false);
    end_crop_spin_->blockSignals(false);

    waveform_widget_->clearSelection();
    updateCropReadout();
}

void UploadDialog::updateCropReadout() {
    if (converted_samples_.empty()) {
        crop_info_label_->setText("");
        return;
    }
    double s = start_crop_spin_->value();
    double e = end_crop_spin_->value();
    double crop_sec = (e > s) ? (e - s) : 0.0;
    size_t crop_smpls = static_cast<size_t>(std::round(crop_sec * volsa2::VOLCA_SAMPLERATE));
    double orig_sec = static_cast<double>(converted_samples_.size()) / volsa2::VOLCA_SAMPLERATE;
    crop_info_label_->setText(
        QString("Cropped Duration: %1s (%2 samples) | Total: %3s (%4 samples)")
            .arg(QString::number(crop_sec, 'f', 3))
            .arg(crop_smpls)
            .arg(QString::number(orig_sec, 'f', 3))
            .arg(converted_samples_.size())
    );
}

void UploadDialog::updateFileInfoAndConversion() {
    QString file_path = file_path_edit_->text().trimmed();
    if (file_path.isEmpty() || !QFileInfo::exists(file_path)) {
        waveform_widget_->clear();
        info_label_->setText("No valid file selected.");
        btn_upload_->setEnabled(false);
        btn_play_->setEnabled(false);
        converted_samples_.clear();
        updateCropReadout();
        return;
    }

    QFileInfo fi(file_path);
    if (name_edit_->text().isEmpty()) {
        name_edit_->setText(fi.baseName().left(volsa2::SampleHeader::NAME_LEN));
    }

    try {
        auto info = volsa2::inspect_audio_file(file_path.toStdString());
        auto mode = static_cast<volsa2::MonoMode>(mono_combo_->currentData().toInt());
        converted_samples_ = volsa2::load_and_convert_audio(file_path.toStdString(), mode);

        double total_sec = static_cast<double>(converted_samples_.size()) / volsa2::VOLCA_SAMPLERATE;
        start_crop_spin_->blockSignals(true);
        end_crop_spin_->blockSignals(true);
        start_crop_spin_->setRange(0.0, total_sec);
        end_crop_spin_->setRange(0.0, total_sec);
        start_crop_spin_->setValue(0.0);
        end_crop_spin_->setValue(total_sec);
        start_crop_spin_->blockSignals(false);
        end_crop_spin_->blockSignals(false);

        waveform_widget_->setSelectionEnabled(true);
        waveform_widget_->setAudioData(converted_samples_, volsa2::VOLCA_SAMPLERATE);
        waveform_widget_->clearSelection();
        updateCropReadout();

        double src_dur = info.duration_seconds;
        double dest_dur = static_cast<double>(converted_samples_.size()) / volsa2::VOLCA_SAMPLERATE;

        info_label_->setText(QString("Source: %1 Hz, %2 ch, %3s -> Volca: 31250 Hz mono, %4 samples (%5s)")
                                 .arg(info.sample_rate)
                                 .arg(info.channels)
                                 .arg(QString::number(src_dur, 'f', 2))
                                 .arg(converted_samples_.size())
                                 .arg(QString::number(dest_dur, 'f', 2)));

        btn_upload_->setEnabled(!converted_samples_.empty());
        btn_play_->setEnabled(!converted_samples_.empty());

    } catch (const std::exception& e) {
        info_label_->setText(QString("Error reading audio: %1").arg(e.what()));
        waveform_widget_->clear();
        converted_samples_.clear();
        btn_upload_->setEnabled(false);
        btn_play_->setEnabled(false);
        updateCropReadout();
    }
}

void UploadDialog::onPlayPreview() {
    auto data = audioData();
    if (data.empty()) return;

    if (audio_player_.isPlaying()) {
        audio_player_.stop();
        btn_play_->setText("Play Preview");
        waveform_widget_->setPlayheadPosition(-1.0);
    } else {
        btn_play_->setText("Stop Preview");
        audio_player_.play(data, volsa2::VOLCA_SAMPLERATE, 0.0);
    }
}

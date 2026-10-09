/**
 * @file sample_chopper_dialog.cpp
 * @brief Implementation of interactive beat slicer and sample chopper dialog.
 * @author Volsa2 Project Team
 * @date 2026
 */

#include "sample_chopper_dialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>

SampleChopperDialog::SampleChopperDialog(const std::vector<volsa2::SampleHeader>& current_slots,
                                         const std::vector<int16_t>& samples,
                                         const QString& sample_name,
                                         int source_slot,
                                         QWidget* parent)
    : QDialog(parent),
      current_slots_(current_slots),
      original_samples_(samples),
      original_name_(sample_name),
      source_slot_(source_slot) {
    setWindowTitle(QString("Sample Chopper & Beat Slicer — \"%1\"").arg(sample_name));
    setMinimumWidth(820);
    setMinimumHeight(640);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(10);

    // Header info banner
    double dur_secs = static_cast<double>(samples.size()) / volsa2::VOLCA_SAMPLERATE;
    QString header_text = QString("Sample: \"%1\" | %2 samples (%3s @ 31,250 Hz)")
                              .arg(sample_name)
                              .arg(samples.size())
                              .arg(QString::number(dur_secs, 'f', 2));
    if (source_slot >= 0) {
        header_text += QString(" | Origin: Slot %1").arg(source_slot);
    }
    auto* header_label = new QLabel(header_text);
    header_label->setStyleSheet("font-size: 14px; font-weight: bold; color: #f07828; padding: 4px;");
    main_layout->addWidget(header_label);

    // Waveform & Slice Markers
    waveform_widget_ = new WaveformWidget(this);
    waveform_widget_->setMinimumHeight(130);
    waveform_widget_->setAudioData(samples, volsa2::VOLCA_SAMPLERATE);
    main_layout->addWidget(waveform_widget_);

    // Middle panel: Slicing options and Slice List
    auto* middle_box = new QHBoxLayout();

    // Left group: Slice configuration & controls
    auto* slice_config_box = new QGroupBox("Slicing Engine");
    auto* slice_config_layout = new QVBoxLayout(slice_config_box);

    auto* row1 = new QHBoxLayout();
    row1->addWidget(new QLabel("Mode:"));
    slice_type_combo_ = new QComboBox();
    slice_type_combo_->addItem("Equal Grid Division", 0);
    slice_type_combo_->addItem("Transient Onset Detection", 1);
    row1->addWidget(slice_type_combo_);
    slice_config_layout->addLayout(row1);

    auto* row2 = new QHBoxLayout();
    row2->addWidget(new QLabel("Slice Count:"));
    num_slices_spin_ = new QSpinBox();
    num_slices_spin_->setRange(2, 32);
    num_slices_spin_->setValue(4);
    row2->addWidget(num_slices_spin_);
    slice_config_layout->addLayout(row2);

    auto* btn_recompute = new QPushButton("Recompute Slices");
    connect(btn_recompute, &QPushButton::clicked, this, &SampleChopperDialog::onRecomputeSlices);
    slice_config_layout->addWidget(btn_recompute);

    // Manual Trimming sub-panel
    auto* crop_box = new QGroupBox("Region Crop (Start / End)");
    auto* crop_layout = new QGridLayout(crop_box);
    crop_layout->addWidget(new QLabel("Start:"), 0, 0);
    start_crop_spin_ = new QSpinBox();
    start_crop_spin_->setRange(0, static_cast<int>(samples.size()));
    crop_layout->addWidget(start_crop_spin_, 0, 1);

    crop_layout->addWidget(new QLabel("End:"), 1, 0);
    end_crop_spin_ = new QSpinBox();
    end_crop_spin_->setRange(0, static_cast<int>(samples.size()));
    end_crop_spin_->setValue(static_cast<int>(samples.size()));
    crop_layout->addWidget(end_crop_spin_, 1, 1);

    btn_auto_trim_ = new QPushButton("Auto-Trim Silence");
    connect(btn_auto_trim_, &QPushButton::clicked, this, &SampleChopperDialog::onAutoTrimSilence);
    crop_layout->addWidget(btn_auto_trim_, 2, 0, 1, 2);

    if (source_slot >= 0) {
        auto* btn_crop_in_place = new QPushButton("Crop & Overwrite Slot");
        btn_crop_in_place->setStyleSheet("font-weight: bold; color: #ffbb44;");
        connect(btn_crop_in_place, &QPushButton::clicked, this, &SampleChopperDialog::onCropInPlace);
        crop_layout->addWidget(btn_crop_in_place, 3, 0, 1, 2);
    }

    slice_config_layout->addWidget(crop_box);
    slice_config_layout->addStretch();
    middle_box->addWidget(slice_config_box, 1);

    // Right group: Slice Items list
    auto* slice_list_box = new QGroupBox("Detected Slices (Click to Audition)");
    auto* slice_list_layout = new QVBoxLayout(slice_list_box);

    slice_list_ = new QListWidget();
    slice_list_layout->addWidget(slice_list_);

    auto* slice_btn_bar = new QHBoxLayout();
    btn_play_slice_ = new QPushButton("Play Selected Slice");
    btn_play_slice_->setEnabled(false);
    connect(btn_play_slice_, &QPushButton::clicked, this, &SampleChopperDialog::onPlaySelectedSlice);
    slice_btn_bar->addWidget(btn_play_slice_);

    btn_play_full_ = new QPushButton("Play Full Audio");
    connect(btn_play_full_, &QPushButton::clicked, this, &SampleChopperDialog::onPlayFull);
    slice_btn_bar->addWidget(btn_play_full_);

    slice_list_layout->addLayout(slice_btn_bar);
    middle_box->addWidget(slice_list_box, 2);

    main_layout->addLayout(middle_box);

    // Bottom export bar: Batch export to consecutive slots
    auto* export_box = new QGroupBox("Batch Export Slices to Volca Memory");
    auto* export_layout = new QHBoxLayout(export_box);

    export_layout->addWidget(new QLabel("Starting Slot:"));
    start_export_slot_spin_ = new QSpinBox();
    start_export_slot_spin_->setRange(0, 199);

    // Find first empty slot for starting export
    int first_empty = 0;
    for (size_t i = 0; i < current_slots_.size(); ++i) {
        if (current_slots_[i].is_empty()) {
            first_empty = static_cast<int>(i);
            break;
        }
    }
    start_export_slot_spin_->setValue(first_empty);
    export_layout->addWidget(start_export_slot_spin_);

    auto* btn_export = new QPushButton("Export All Slices to Consecutive Slots");
    btn_export->setStyleSheet("background-color: #00aa88; color: white; font-weight: bold; padding: 6px 16px;");
    connect(btn_export, &QPushButton::clicked, this, &SampleChopperDialog::onExportSlices);
    export_layout->addWidget(btn_export);

    export_layout->addStretch();
    auto* btn_close = new QPushButton("Close");
    connect(btn_close, &QPushButton::clicked, this, &QDialog::reject);
    export_layout->addWidget(btn_close);

    main_layout->addWidget(export_box);

    // Connections
    connect(slice_type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SampleChopperDialog::onSliceModeChanged);
    connect(slice_list_, &QListWidget::itemClicked, this, &SampleChopperDialog::onSliceItemClicked);
    connect(waveform_widget_, &WaveformWidget::selectionChanged, this, &SampleChopperDialog::onWaveformSelectionChanged);
    connect(waveform_widget_, &WaveformWidget::sliceClicked, this, [this](int s) {
        if (s >= 0 && s < slice_list_->count()) {
            slice_list_->setCurrentRow(s);
            onPlaySelectedSlice();
        }
    });
    connect(&audio_player_, &AlsaAudioPlayer::positionChanged, waveform_widget_, &WaveformWidget::setPlayheadPosition);
    connect(&audio_player_, &AlsaAudioPlayer::playbackFinished, this, [this]() {
        btn_play_full_->setText("Play Full Audio");
        waveform_widget_->setPlayheadPosition(-1.0);
    });

    // Initial slice computation
    onRecomputeSlices();
}

void SampleChopperDialog::onSliceModeChanged() {
    onRecomputeSlices();
}

void SampleChopperDialog::onRecomputeSlices() {
    if (original_samples_.empty()) return;

    int mode = slice_type_combo_->currentIndex();
    size_t count = static_cast<size_t>(num_slices_spin_->value());

    if (mode == 0) {
        current_slice_points_ = volsa2::divide_equal_slices(original_samples_.size(), count, volsa2::VOLCA_SAMPLERATE);
    } else {
        current_slice_points_ = volsa2::detect_transient_slices(original_samples_, volsa2::VOLCA_SAMPLERATE, count, 0.5);
    }

    // Set markers on waveform widget
    std::vector<size_t> markers;
    for (const auto& sp : current_slice_points_) {
        markers.push_back(sp.start_sample);
    }
    if (!current_slice_points_.empty()) {
        markers.push_back(current_slice_points_.back().end_sample);
    }
    waveform_widget_->setSliceMarkers(markers);

    updateSliceList();
}

void SampleChopperDialog::updateSliceList() {
    slice_list_->clear();
    for (size_t i = 0; i < current_slice_points_.size(); ++i) {
        const auto& sp = current_slice_points_[i];
        size_t len = sp.end_sample - sp.start_sample;
        double dur = static_cast<double>(len) / volsa2::VOLCA_SAMPLERATE;

        QString item_text = QString("Slice %1: [%2 - %3] (%4 smpls, %5s)")
                                .arg(i + 1)
                                .arg(sp.start_sample)
                                .arg(sp.end_sample)
                                .arg(len)
                                .arg(QString::number(dur, 'f', 2));
        slice_list_->addItem(item_text);
    }
    btn_play_slice_->setEnabled(slice_list_->count() > 0);
}

void SampleChopperDialog::onSliceItemClicked(QListWidgetItem* item) {
    int row = slice_list_->row(item);
    if (row >= 0 && static_cast<size_t>(row) < current_slice_points_.size()) {
        const auto& sp = current_slice_points_[row];
        waveform_widget_->setSelection(sp.start_sample, sp.end_sample);
        btn_play_slice_->setEnabled(true);
    }
}

void SampleChopperDialog::onPlaySelectedSlice() {
    int row = slice_list_->currentRow();
    if (row < 0 || static_cast<size_t>(row) >= current_slice_points_.size()) return;

    const auto& sp = current_slice_points_[row];
    auto slice_audio = volsa2::crop_audio(original_samples_, sp.start_sample, sp.end_sample, true);

    audio_player_.stop();
    audio_player_.play(slice_audio, volsa2::VOLCA_SAMPLERATE, 0.0);
}

void SampleChopperDialog::onPlayFull() {
    if (original_samples_.empty()) return;
    if (audio_player_.isPlaying()) {
        audio_player_.stop();
        btn_play_full_->setText("Play Full Audio");
        waveform_widget_->setPlayheadPosition(-1.0);
    } else {
        btn_play_full_->setText("Stop");
        audio_player_.play(original_samples_, volsa2::VOLCA_SAMPLERATE, 0.0);
    }
}

void SampleChopperDialog::onAutoTrimSilence() {
    if (original_samples_.empty()) return;
    auto [start_bound, end_bound] = volsa2::find_silence_bounds(original_samples_, -48.0);
    start_crop_spin_->setValue(static_cast<int>(start_bound));
    end_crop_spin_->setValue(static_cast<int>(end_bound));
    waveform_widget_->setSelection(start_bound, end_bound);
}

void SampleChopperDialog::onWaveformSelectionChanged(size_t start, size_t end) {
    start_crop_spin_->blockSignals(true);
    end_crop_spin_->blockSignals(true);
    start_crop_spin_->setValue(static_cast<int>(start));
    end_crop_spin_->setValue(static_cast<int>(end));
    start_crop_spin_->blockSignals(false);
    end_crop_spin_->blockSignals(false);
}

void SampleChopperDialog::onCropInPlace() {
    if (source_slot_ < 0) {
        QMessageBox::warning(this, "Crop in Place", "This sample did not originate from a device slot.");
        return;
    }

    size_t s = static_cast<size_t>(start_crop_spin_->value());
    size_t e = static_cast<size_t>(end_crop_spin_->value());
    auto cropped = volsa2::crop_audio(original_samples_, s, e, true);

    emit cropAndSaveRequested(source_slot_, original_name_, cropped);
    accept();
}

void SampleChopperDialog::onExportSlices() {
    if (current_slice_points_.empty()) {
        QMessageBox::warning(this, "Batch Export", "No slices available to export.");
        return;
    }

    int start_slot = start_export_slot_spin_->value();
    size_t num_slices = current_slice_points_.size();

    if (start_slot + num_slices > 200) {
        QMessageBox::warning(this, "Batch Export",
            QString("Cannot export %1 slices starting from slot %2 (exceeds 200 slot limit).")
                .arg(num_slices)
                .arg(start_slot));
        return;
    }

    // Check for occupied slots in range
    QStringList occupied;
    for (size_t i = 0; i < num_slices; ++i) {
        int target = start_slot + static_cast<int>(i);
        if (target < static_cast<int>(current_slots_.size()) && !current_slots_[target].is_empty()) {
            occupied.append(QString("Slot %1: \"%2\"")
                                .arg(target)
                                .arg(QString::fromStdString(current_slots_[target].name)));
        }
    }

    if (!occupied.isEmpty()) {
        auto res = QMessageBox::question(
            this, "Confirm Overwrite",
            QString("The following %1 slots will be overwritten:\n\n%2\n\nProceed with batch export?")
                .arg(occupied.size())
                .arg(occupied.join("\n")),
            QMessageBox::Yes | QMessageBox::No
        );
        if (res != QMessageBox::Yes) {
            return;
        }
    }

    // Extract slice audio buffers
    std::vector<std::vector<int16_t>> slices_audio;
    slices_audio.reserve(num_slices);

    for (const auto& sp : current_slice_points_) {
        slices_audio.push_back(volsa2::crop_audio(original_samples_, sp.start_sample, sp.end_sample, true));
    }

    emit batchExportRequested(start_slot, original_name_, slices_audio);
    accept();
}

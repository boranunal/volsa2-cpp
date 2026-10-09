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
    setMinimumWidth(860);
    setMinimumHeight(680);

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
    // Selected slice boundary controls
    auto* crop_box = new QGroupBox("Selected Slice Boundaries");
    auto* crop_layout = new QGridLayout(crop_box);
    crop_layout->addWidget(new QLabel("Start:"), 0, 0);
    start_crop_spin_ = new QDoubleSpinBox();
    start_crop_spin_->setRange(0.0, dur_secs);
    start_crop_spin_->setDecimals(3);
    start_crop_spin_->setSingleStep(0.010);
    start_crop_spin_->setSuffix(" s");
    connect(start_crop_spin_, &QDoubleSpinBox::valueChanged, this, &SampleChopperDialog::onCropSpinChanged);
    crop_layout->addWidget(start_crop_spin_, 0, 1);

    crop_layout->addWidget(new QLabel("End:"), 1, 0);
    end_crop_spin_ = new QDoubleSpinBox();
    end_crop_spin_->setRange(0.0, dur_secs);
    end_crop_spin_->setDecimals(3);
    end_crop_spin_->setSingleStep(0.010);
    end_crop_spin_->setSuffix(" s");
    end_crop_spin_->setValue(dur_secs);
    connect(end_crop_spin_, &QDoubleSpinBox::valueChanged, this, &SampleChopperDialog::onCropSpinChanged);
    crop_layout->addWidget(end_crop_spin_, 1, 1);

    link_slices_check_ = new QCheckBox("Link adjacent slice boundaries");
    link_slices_check_->setChecked(true);
    link_slices_check_->setToolTip("When checked, adjusting a slice boundary also adjusts the neighboring slice so slices stay seamless.");
    crop_layout->addWidget(link_slices_check_, 2, 0, 1, 2);

    slice_info_label_ = new QLabel();
    slice_info_label_->setStyleSheet("color: #00e5b0; font-size: 11px;");
    crop_layout->addWidget(slice_info_label_, 3, 0, 1, 2);

    btn_auto_trim_ = new QPushButton("Auto-Trim Slice Silence");
    btn_auto_trim_->setToolTip("Snaps current slice start and end points tightly around audible sound in this slice.");
    connect(btn_auto_trim_, &QPushButton::clicked, this, &SampleChopperDialog::onAutoTrimSilence);
    crop_layout->addWidget(btn_auto_trim_, 4, 0, 1, 2);

    slice_config_layout->addWidget(crop_box);

    // Save Selected Slice to Custom Slot
    auto* save_slice_box = new QGroupBox("Save Selected Slice to Custom Slot");
    auto* save_layout = new QGridLayout(save_slice_box);

    save_layout->addWidget(new QLabel("Target Slot:"), 0, 0);
    auto* slot_picker_layout = new QHBoxLayout();
    target_slot_spin_ = new QSpinBox();
    target_slot_spin_->setRange(0, 199);
    int first_empty = findNextEmptySlot(0);
    target_slot_spin_->setValue(first_empty >= 0 ? first_empty : (source_slot >= 0 ? source_slot : 0));
    connect(target_slot_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &SampleChopperDialog::onTargetSlotChanged);
    slot_picker_layout->addWidget(target_slot_spin_);

    if (source_slot >= 0) {
        auto* btn_use_source = new QPushButton(QString("Use Source (%1)").arg(source_slot));
        btn_use_source->setToolTip(QString("Set destination slot to original source slot %1 (will overwrite original!)").arg(source_slot));
        btn_use_source->setStyleSheet("font-size: 10px; padding: 2px 6px;");
        connect(btn_use_source, &QPushButton::clicked, this, [this]() {
            target_slot_spin_->setValue(source_slot_);
        });
        slot_picker_layout->addWidget(btn_use_source);
    }
    save_layout->addLayout(slot_picker_layout, 0, 1);

    target_slot_status_label_ = new QLabel();
    target_slot_status_label_->setStyleSheet("font-size: 11px; font-weight: bold;");
    save_layout->addWidget(target_slot_status_label_, 1, 1);

    save_layout->addWidget(new QLabel("Slice Name:"), 2, 0);
    slice_name_edit_ = new QLineEdit();
    slice_name_edit_->setMaxLength(24);
    slice_name_edit_->setText(QString("%1_s1").arg(sample_name.left(18)));
    save_layout->addWidget(slice_name_edit_, 2, 1);

    auto* save_btn_layout = new QHBoxLayout();
    btn_save_slice_ = new QPushButton("Save Slice to Slot");
    btn_save_slice_->setStyleSheet("background-color: #00b4d8; color: #ffffff; font-weight: bold; padding: 5px 10px;");
    btn_save_slice_->setToolTip("Uploads the selected slice to the specified target slot (leaves dialog open to save more slices).");
    connect(btn_save_slice_, &QPushButton::clicked, this, [this]() { onSaveSelectedSlice(false); });
    save_btn_layout->addWidget(btn_save_slice_);

    btn_save_and_close_ = new QPushButton("Save & Close");
    btn_save_and_close_->setToolTip("Uploads this slice to target slot and closes chopper.");
    connect(btn_save_and_close_, &QPushButton::clicked, this, [this]() { onSaveSelectedSlice(true); });
    save_btn_layout->addWidget(btn_save_and_close_);
    save_layout->addLayout(save_btn_layout, 3, 0, 1, 2);

    save_feedback_label_ = new QLabel();
    save_feedback_label_->setStyleSheet("color: #00e5b0; font-size: 11px;");
    save_layout->addWidget(save_feedback_label_, 4, 0, 1, 2);

    slice_config_layout->addWidget(save_slice_box);
    slice_config_layout->addStretch();
    middle_box->addWidget(slice_config_box, 1);
    updateTargetSlotStatus();

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

    start_export_slot_spin_->setValue(first_empty >= 0 ? first_empty : 0);
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
    connect(slice_list_, &QListWidget::currentRowChanged, this, &SampleChopperDialog::selectSlice);
    connect(waveform_widget_, &WaveformWidget::selectionChanged, this, &SampleChopperDialog::onWaveformSelectionChanged);
    connect(waveform_widget_, &WaveformWidget::sliceDividerMoved, this, &SampleChopperDialog::onSliceDividerMoved);
    connect(waveform_widget_, &WaveformWidget::sliceClicked, this, [this](int s) {
        if (s >= 0 && static_cast<size_t>(s) < current_slice_points_.size()) {
            selectSlice(s);
            onPlaySelectedSlice();
        }
    });
    connect(&audio_player_, &AlsaAudioPlayer::positionChanged, this, [this](double norm_pos) {
        if (is_playing_slice_) {
            size_t s = static_cast<size_t>(std::clamp(std::round(start_crop_spin_->value() * volsa2::VOLCA_SAMPLERATE), 0.0, static_cast<double>(original_samples_.size())));
            size_t e = static_cast<size_t>(std::clamp(std::round(end_crop_spin_->value() * volsa2::VOLCA_SAMPLERATE), 0.0, static_cast<double>(original_samples_.size())));
            if (e > s && !original_samples_.empty()) {
                double full_norm = (static_cast<double>(s) + norm_pos * (e - s)) / original_samples_.size();
                waveform_widget_->setPlayheadPosition(full_norm);
            } else {
                waveform_widget_->setPlayheadPosition(norm_pos);
            }
        } else {
            waveform_widget_->setPlayheadPosition(norm_pos);
        }
    });
    connect(&audio_player_, &AlsaAudioPlayer::playbackFinished, this, [this]() {
        is_playing_slice_ = false;
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

    syncSliceSpansToWaveform();
    updateSliceList();

    if (!current_slice_points_.empty()) {
        selectSlice(0);
    } else {
        waveform_widget_->clearSelection();
        btn_play_slice_->setEnabled(false);
    }
}

void SampleChopperDialog::syncSliceSpansToWaveform() {
    std::vector<WaveformWidget::SliceSpan> spans;
    spans.reserve(current_slice_points_.size());
    for (const auto& sp : current_slice_points_) {
        spans.push_back({sp.start_sample, sp.end_sample});
    }
    waveform_widget_->setSliceSpans(spans);
}

void SampleChopperDialog::updateSliceList() {
    slice_list_->blockSignals(true);
    slice_list_->clear();
    for (size_t i = 0; i < current_slice_points_.size(); ++i) {
        const auto& sp = current_slice_points_[i];
        size_t len = sp.length();
        double dur = static_cast<double>(len) / volsa2::VOLCA_SAMPLERATE;

        QString item_text = QString("Slice %1: [%2 - %3] (%4 smpls, %5s)")
                                .arg(i + 1)
                                .arg(sp.start_sample)
                                .arg(sp.end_sample)
                                .arg(len)
                                .arg(QString::number(dur, 'f', 2));
        slice_list_->addItem(item_text);
    }
    slice_list_->blockSignals(false);
    btn_play_slice_->setEnabled(slice_list_->count() > 0 && slice_list_->currentRow() >= 0);
}

void SampleChopperDialog::updateSliceListItem(int row) {
    if (row < 0 || static_cast<size_t>(row) >= current_slice_points_.size()) return;
    const auto& sp = current_slice_points_[row];
    size_t len = sp.length();
    double dur = static_cast<double>(len) / volsa2::VOLCA_SAMPLERATE;
    QString item_text = QString("Slice %1: [%2 - %3] (%4 smpls, %5s)")
                            .arg(row + 1)
                            .arg(sp.start_sample)
                            .arg(sp.end_sample)
                            .arg(len)
                            .arg(QString::number(dur, 'f', 2));
    if (auto* item = slice_list_->item(row)) {
        item->setText(item_text);
    }
}

void SampleChopperDialog::selectSlice(int row) {
    if (row < 0 || static_cast<size_t>(row) >= current_slice_points_.size()) return;

    slice_list_->blockSignals(true);
    slice_list_->setCurrentRow(row);
    slice_list_->blockSignals(false);

    const auto& sp = current_slice_points_[row];
    double s_sec = static_cast<double>(sp.start_sample) / volsa2::VOLCA_SAMPLERATE;
    double e_sec = static_cast<double>(sp.end_sample) / volsa2::VOLCA_SAMPLERATE;

    start_crop_spin_->blockSignals(true);
    end_crop_spin_->blockSignals(true);
    start_crop_spin_->setValue(s_sec);
    end_crop_spin_->setValue(e_sec);
    start_crop_spin_->blockSignals(false);
    end_crop_spin_->blockSignals(false);

    waveform_widget_->blockSignals(true);
    waveform_widget_->setSelection(sp.start_sample, sp.end_sample);
    waveform_widget_->blockSignals(false);

    if (slice_info_label_) {
        size_t len = sp.length();
        double dur = static_cast<double>(len) / volsa2::VOLCA_SAMPLERATE;
        slice_info_label_->setText(QString("Slice %1: %2s (%3 smpls)")
                                       .arg(row + 1)
                                       .arg(QString::number(dur, 'f', 3))
                                       .arg(len));
    }

    if (slice_name_edit_) {
        slice_name_edit_->setText(QString("%1_s%2").arg(original_name_.left(18)).arg(row + 1));
    }
    if (save_feedback_label_) {
        save_feedback_label_->setText("");
    }

    btn_play_slice_->setEnabled(true);
}

void SampleChopperDialog::updateCurrentSliceBounds(size_t start, size_t end) {
    int row = slice_list_->currentRow();
    if (row < 0 || static_cast<size_t>(row) >= current_slice_points_.size()) return;

    size_t total = original_samples_.size();
    size_t s = std::clamp(start, size_t{0}, total);
    size_t e = std::clamp(end, size_t{0}, total);
    if (s > e) std::swap(s, e);

    bool link = link_slices_check_ && link_slices_check_->isChecked();

    if (link) {
        if (row > 0) {
            s = std::max(s, current_slice_points_[row - 1].start_sample);
            current_slice_points_[row - 1].end_sample = s;
            current_slice_points_[row - 1].end_seconds = static_cast<double>(s) / volsa2::VOLCA_SAMPLERATE;
            updateSliceListItem(row - 1);
        }
        if (static_cast<size_t>(row + 1) < current_slice_points_.size()) {
            e = std::min(e, current_slice_points_[row + 1].end_sample);
            current_slice_points_[row + 1].start_sample = e;
            current_slice_points_[row + 1].start_seconds = static_cast<double>(e) / volsa2::VOLCA_SAMPLERATE;
            updateSliceListItem(row + 1);
        }
    }

    current_slice_points_[row].start_sample = s;
    current_slice_points_[row].end_sample = e;
    current_slice_points_[row].start_seconds = static_cast<double>(s) / volsa2::VOLCA_SAMPLERATE;
    current_slice_points_[row].end_seconds = static_cast<double>(e) / volsa2::VOLCA_SAMPLERATE;
    updateSliceListItem(row);

    double s_sec = static_cast<double>(s) / volsa2::VOLCA_SAMPLERATE;
    double e_sec = static_cast<double>(e) / volsa2::VOLCA_SAMPLERATE;
    start_crop_spin_->blockSignals(true);
    end_crop_spin_->blockSignals(true);
    start_crop_spin_->setValue(s_sec);
    end_crop_spin_->setValue(e_sec);
    start_crop_spin_->blockSignals(false);
    end_crop_spin_->blockSignals(false);

    if (slice_info_label_) {
        size_t len = current_slice_points_[row].length();
        double dur = static_cast<double>(len) / volsa2::VOLCA_SAMPLERATE;
        slice_info_label_->setText(QString("Slice %1: %2s (%3 smpls)")
                                       .arg(row + 1)
                                       .arg(QString::number(dur, 'f', 3))
                                       .arg(len));
    }

    syncSliceSpansToWaveform();
}

void SampleChopperDialog::onSliceItemClicked(QListWidgetItem* item) {
    if (item) {
        selectSlice(slice_list_->row(item));
    }
}

void SampleChopperDialog::onPlaySelectedSlice() {
    int row = slice_list_->currentRow();
    if (row < 0 || static_cast<size_t>(row) >= current_slice_points_.size()) return;

    const auto& sp = current_slice_points_[row];
    auto slice_audio = volsa2::crop_audio(original_samples_, sp.start_sample, sp.end_sample, true);

    is_playing_slice_ = true;
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
        is_playing_slice_ = false;
        btn_play_full_->setText("Stop");
        audio_player_.play(original_samples_, volsa2::VOLCA_SAMPLERATE, 0.0);
    }
}

void SampleChopperDialog::onAutoTrimSilence() {
    if (original_samples_.empty()) return;
    int row = slice_list_->currentRow();
    if (row < 0 || static_cast<size_t>(row) >= current_slice_points_.size()) return;

    const auto& sp = current_slice_points_[row];
    if (sp.end_sample <= sp.start_sample) return;

    std::span<const int16_t> slice_span(original_samples_.data() + sp.start_sample, sp.end_sample - sp.start_sample);
    auto [rel_start, rel_end] = volsa2::find_silence_bounds(slice_span, -48.0);

    size_t abs_start = sp.start_sample + rel_start;
    size_t abs_end = sp.start_sample + rel_end;

    waveform_widget_->blockSignals(true);
    waveform_widget_->setSelection(abs_start, abs_end);
    waveform_widget_->blockSignals(false);

    updateCurrentSliceBounds(abs_start, abs_end);
}

void SampleChopperDialog::onWaveformSelectionChanged(size_t start, size_t end) {
    updateCurrentSliceBounds(start, end);
}

void SampleChopperDialog::onCropSpinChanged() {
    double s = start_crop_spin_->value();
    double e = end_crop_spin_->value();
    if (s > e) {
        start_crop_spin_->blockSignals(true);
        start_crop_spin_->setValue(e);
        start_crop_spin_->blockSignals(false);
        s = e;
    }
    size_t s_smpls = static_cast<size_t>(std::clamp(std::round(s * volsa2::VOLCA_SAMPLERATE), 0.0, static_cast<double>(original_samples_.size())));
    size_t e_smpls = static_cast<size_t>(std::clamp(std::round(e * volsa2::VOLCA_SAMPLERATE), 0.0, static_cast<double>(original_samples_.size())));

    waveform_widget_->blockSignals(true);
    waveform_widget_->setSelection(s_smpls, e_smpls);
    waveform_widget_->blockSignals(false);

    updateCurrentSliceBounds(s_smpls, e_smpls);
}

void SampleChopperDialog::onSliceDividerMoved(int divider_idx, size_t new_sample) {
    if (divider_idx <= 0 || static_cast<size_t>(divider_idx) >= current_slice_points_.size()) return;

    current_slice_points_[divider_idx - 1].end_sample = new_sample;
    current_slice_points_[divider_idx - 1].end_seconds = static_cast<double>(new_sample) / volsa2::VOLCA_SAMPLERATE;

    current_slice_points_[divider_idx].start_sample = new_sample;
    current_slice_points_[divider_idx].start_seconds = static_cast<double>(new_sample) / volsa2::VOLCA_SAMPLERATE;

    updateSliceListItem(divider_idx - 1);
    updateSliceListItem(divider_idx);

    int cur_row = slice_list_->currentRow();
    if (cur_row == divider_idx - 1) {
        end_crop_spin_->blockSignals(true);
        end_crop_spin_->setValue(current_slice_points_[cur_row].end_seconds);
        end_crop_spin_->blockSignals(false);
        waveform_widget_->blockSignals(true);
        waveform_widget_->setSelection(current_slice_points_[cur_row].start_sample, current_slice_points_[cur_row].end_sample);
        waveform_widget_->blockSignals(false);
    } else if (cur_row == divider_idx) {
        start_crop_spin_->blockSignals(true);
        start_crop_spin_->setValue(current_slice_points_[cur_row].start_seconds);
        start_crop_spin_->blockSignals(false);
        waveform_widget_->blockSignals(true);
        waveform_widget_->setSelection(current_slice_points_[cur_row].start_sample, current_slice_points_[cur_row].end_sample);
        waveform_widget_->blockSignals(false);
    }

    if (slice_info_label_ && cur_row >= 0 && static_cast<size_t>(cur_row) < current_slice_points_.size()) {
        size_t len = current_slice_points_[cur_row].length();
        double dur = static_cast<double>(len) / volsa2::VOLCA_SAMPLERATE;
        slice_info_label_->setText(QString("Slice %1: %2s (%3 smpls)")
                                       .arg(cur_row + 1)
                                       .arg(QString::number(dur, 'f', 3))
                                       .arg(len));
    }

    syncSliceSpansToWaveform();
}

void SampleChopperDialog::onSaveSelectedSlice(bool close_after) {
    int row = slice_list_->currentRow();
    if (row < 0 || static_cast<size_t>(row) >= current_slice_points_.size()) {
        QMessageBox::warning(this, "Save Slice", "Please select a slice from the list or waveform first.");
        return;
    }

    int target_slot = target_slot_spin_->value();
    if (target_slot < 0 || target_slot > 199) {
        QMessageBox::warning(this, "Save Slice", "Target slot must be between 0 and 199.");
        return;
    }

    const auto& sp = current_slice_points_[row];
    if (sp.length() == 0) {
        QMessageBox::warning(this, "Save Slice", "Selected slice has 0 length.");
        return;
    }

    auto cropped = volsa2::crop_audio(original_samples_, sp.start_sample, sp.end_sample, true);
    if (cropped.empty()) {
        QMessageBox::warning(this, "Save Slice", "Failed to extract audio for selected slice.");
        return;
    }

    QString name = slice_name_edit_->text().trimmed();
    if (name.isEmpty()) {
        name = QString("%1_s%2").arg(original_name_.left(18)).arg(row + 1);
    }
    if (name.size() > 24) {
        name = name.left(24);
    }

    // Check if target slot is occupied
    if (target_slot < static_cast<int>(current_slots_.size()) && !current_slots_[target_slot].is_empty()) {
        QString occupied_name = QString::fromStdString(current_slots_[target_slot].name);
        QString warn_msg;
        if (target_slot == source_slot_) {
            warn_msg = QString("Target slot %1 contains the ORIGINAL source sample (\"%2\").\n\nAre you sure you want to overwrite it with this individual slice?")
                           .arg(target_slot).arg(occupied_name);
        } else {
            warn_msg = QString("Target slot %1 is already occupied by \"%2\".\n\nAre you sure you want to overwrite it with this slice?")
                           .arg(target_slot).arg(occupied_name);
        }
        auto res = QMessageBox::question(this, "Confirm Overwrite", warn_msg, QMessageBox::Yes | QMessageBox::No);
        if (res != QMessageBox::Yes) {
            return;
        }
    }

    // Emit signal to upload slice
    emit cropAndSaveRequested(target_slot, name, cropped);

    // Update local cache of slots so UI status reflects the newly saved slice
    if (target_slot < static_cast<int>(current_slots_.size())) {
        current_slots_[target_slot].name = name.toStdString();
        current_slots_[target_slot].length = static_cast<uint32_t>(cropped.size());
        current_slots_[target_slot].level = volsa2::SampleHeader::DEFAULT_LEVEL;
        current_slots_[target_slot].speed = volsa2::SampleHeader::DEFAULT_SPEED;
    }

    save_feedback_label_->setText(QString("✓ Saved Slice %1 to Slot %2 (\"%3\")").arg(row + 1).arg(target_slot).arg(name));
    save_feedback_label_->setStyleSheet("color: #00e5b0; font-weight: bold; font-size: 11px;");

    if (close_after) {
        accept();
        return;
    }

    // Automatically advance target slot to the next empty slot for subsequent slices!
    int next_empty = findNextEmptySlot(target_slot + 1);
    if (next_empty >= 0) {
        target_slot_spin_->setValue(next_empty);
    } else {
        updateTargetSlotStatus();
    }
}

void SampleChopperDialog::onTargetSlotChanged(int slot) {
    (void)slot;
    updateTargetSlotStatus();
}

void SampleChopperDialog::updateTargetSlotStatus() {
    if (!target_slot_status_label_ || !target_slot_spin_) return;
    int slot = target_slot_spin_->value();
    if (slot < 0 || slot >= static_cast<int>(current_slots_.size())) {
        target_slot_status_label_->setText("");
        return;
    }

    const auto& h = current_slots_[slot];
    if (h.is_empty()) {
        target_slot_status_label_->setText("<span style='color: #00e5b0;'>[🟢 Empty Slot]</span>");
    } else if (slot == source_slot_) {
        target_slot_status_label_->setText(QString("<span style='color: #ff6666;'>[⚠️ Source Sample: \"%1\"]</span>")
                                               .arg(QString::fromStdString(h.name)));
    } else {
        target_slot_status_label_->setText(QString("<span style='color: #f07828;'>[Occupied: \"%1\"]</span>")
                                               .arg(QString::fromStdString(h.name)));
    }
}

int SampleChopperDialog::findNextEmptySlot(int start_from) const {
    for (int i = start_from; i < 200; ++i) {
        if (i < static_cast<int>(current_slots_.size()) && current_slots_[i].is_empty()) {
            return i;
        }
    }
    for (int i = 0; i < start_from; ++i) {
        if (i < static_cast<int>(current_slots_.size()) && current_slots_[i].is_empty()) {
            return i;
        }
    }
    return -1;
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

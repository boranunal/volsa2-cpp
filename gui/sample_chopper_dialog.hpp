/**
 * @file sample_chopper_dialog.hpp
 * @brief Modal dialog for beat slicing, interactive slice auditioning, manual sample trimming,
 *        and batch export of slices to consecutive Volca Sample 2 slots.
 * @date 2026
 */

#pragma once

#include "waveform_widget.hpp"
#include "audio_player.hpp"
#include "volsa2/proto.hpp"
#include "volsa2/sample_chopper.hpp"

#include <QDialog>
#include <QListWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <vector>
#include <cstdint>

/**
 * @class SampleChopperDialog
 * @brief Interactive beat slicer and sample chopper dialog.
 */
class SampleChopperDialog : public QDialog {
    Q_OBJECT

public:
    explicit SampleChopperDialog(const std::vector<volsa2::SampleHeader>& current_slots,
                                const std::vector<int16_t>& samples,
                                const QString& sample_name,
                                int source_slot = -1,
                                QWidget* parent = nullptr);

signals:
    /**
     * @brief Emitted when the user chooses to crop and overwrite the source slot.
     */
    void cropAndSaveRequested(int slot, const QString& name, const std::vector<int16_t>& samples);

    /**
     * @brief Emitted when the user exports all slices to consecutive device slots.
     */
    void batchExportRequested(int start_slot, const QString& base_name,
                              const std::vector<std::vector<int16_t>>& slices_data);

private slots:
    void onSliceModeChanged();
    void onRecomputeSlices();
    void onSliceItemClicked(QListWidgetItem* item);
    void selectSlice(int row);
    void onPlaySelectedSlice();
    void onPlayFull();
    void onAutoTrimSilence();
    void onWaveformSelectionChanged(size_t start, size_t end);
    void onCropSpinChanged();
    void onSliceDividerMoved(int divider_index, size_t new_sample);
    void onSaveSelectedSlice(bool close_after = false);
    void onTargetSlotChanged(int slot);
    void onExportSlices();

private:
    void updateSliceList();
    void updateSliceListItem(int row);
    void updateCurrentSliceBounds(size_t start_sample, size_t end_sample);
    void syncSliceSpansToWaveform();
    int findNextEmptySlot(int start_from = 0) const;
    void updateTargetSlotStatus();
    bool is_playing_slice_{false};

    std::vector<volsa2::SampleHeader> current_slots_;
    std::vector<int16_t> original_samples_;
    QString original_name_;
    int source_slot_{-1};

    WaveformWidget* waveform_widget_{nullptr};

    // Slice config
    QComboBox* slice_type_combo_{nullptr};    ///< Equal Grid vs Transient Onset
    QSpinBox* num_slices_spin_{nullptr};      ///< Desired slice count
    QListWidget* slice_list_{nullptr};        ///< List of generated slices

    // Crop inputs
    QDoubleSpinBox* start_crop_spin_{nullptr};///< Crop start in seconds
    QDoubleSpinBox* end_crop_spin_{nullptr};  ///< Crop end in seconds
    QCheckBox* link_slices_check_{nullptr};   ///< Link adjacent slice boundaries
    QLabel* slice_info_label_{nullptr};       ///< Readout of selected slice duration & samples
    QPushButton* btn_auto_trim_{nullptr};

    // Save selected slice to custom slot
    QSpinBox* target_slot_spin_{nullptr};         ///< Target Volca slot index for saving current slice
    QLabel* target_slot_status_label_{nullptr};   ///< Status indication for selected target slot
    QLineEdit* slice_name_edit_{nullptr};         ///< Name for the slice on the device
    QPushButton* btn_save_slice_{nullptr};        ///< Save slice to target slot
    QPushButton* btn_save_and_close_{nullptr};    ///< Save slice to target slot and close
    QLabel* save_feedback_label_{nullptr};        ///< Notification readout upon saving

    // Playback
    QPushButton* btn_play_slice_{nullptr};
    QPushButton* btn_play_full_{nullptr};

    // Batch Export
    QSpinBox* start_export_slot_spin_{nullptr};

    std::vector<volsa2::SlicePoint> current_slice_points_;
    AlsaAudioPlayer audio_player_{this};
};

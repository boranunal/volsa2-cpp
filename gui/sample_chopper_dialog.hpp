/**
 * @file sample_chopper_dialog.hpp
 * @brief Modal dialog for beat slicing, interactive slice auditioning, manual sample trimming,
 *        and batch export of slices to consecutive Volca Sample 2 slots.
 * @author Volsa2 Project Team
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
#include <QLabel>
#include <QPushButton>
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
    void onPlaySelectedSlice();
    void onPlayFull();
    void onAutoTrimSilence();
    void onWaveformSelectionChanged(size_t start, size_t end);
    void onCropSpinChanged();
    void onCropInPlace();
    void onExportSlices();

private:
    void updateSliceList();

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
    QPushButton* btn_auto_trim_{nullptr};

    // Playback
    QPushButton* btn_play_slice_{nullptr};
    QPushButton* btn_play_full_{nullptr};

    // Batch Export
    QSpinBox* start_export_slot_spin_{nullptr};

    std::vector<volsa2::SlicePoint> current_slice_points_;
    AlsaAudioPlayer audio_player_{this};
};

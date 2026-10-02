/**
 * @file upload_dialog.hpp
 * @brief Modal dialog for sample auditioning, channel selection, format conversion, and upload.
 * @details Provides file browsing, automated metadata extraction, waveform preview of the
 *          resampled 31.25 kHz signal, audition playback via QAudioSink, target slot selection
 *          with auto-empty detection, and overwrite/backup safeguards.
 * @author Volsa2 Project Team
 * @date 2026
 */

#pragma once

#include "waveform_widget.hpp"
#include "audio_player.hpp"
#include "volsa2/proto.hpp"

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <vector>
#include <cstdint>

/**
 * @class UploadDialog
 * @brief Interactive modal dialog guiding the user through sample conversion and upload.
 */
class UploadDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs an UploadDialog.
     * @param current_slots Snapshot of current device slot headers (used to warn on overwriting).
     * @param initial_slot Pre-selected slot (-1 to search for the first empty slot).
     * @param initial_file Pre-populated source audio path (e.g. from drag & drop).
     * @param parent Optional parent QWidget.
     */
    explicit UploadDialog(const std::vector<volsa2::SampleHeader>& current_slots,
                          int initial_slot = -1,
                          const QString& initial_file = QString(),
                          QWidget* parent = nullptr);

    /**
     * @brief Returns the chosen destination slot on the Volca Sample 2.
     * @return Integer slot index (0-199).
     */
    int targetSlot() const;

    /**
     * @brief Returns the sanitized sample name to be stored in the device header.
     * @return QString of up to 24 characters.
     */
    QString sampleName() const;

    /**
     * @brief Returns the converted 16-bit 31.25 kHz mono PCM audio samples.
     * @return Const reference to the sample vector.
     */
    const std::vector<int16_t>& audioData() const { return converted_samples_; }

    /**
     * @brief Indicates whether the user opted to back up the current slot's sample before overwriting.
     * @return True if backup was requested, false otherwise.
     */
    bool backupRequested() const;

private slots:
    void onBrowseFile();
    void onMonoModeChanged();
    void onSlotChanged(int slot);
    void onFindEmptySlot();
    void onPlayPreview();
    void updateFileInfoAndConversion();

private:
    QLineEdit* file_path_edit_{nullptr};     ///< Text input showing path to selected audio file.
    QSpinBox* slot_spin_{nullptr};           ///< Target slot selector (0-199).
    QPushButton* btn_find_empty_{nullptr};   ///< Button to automatically select the first empty slot.
    QLineEdit* name_edit_{nullptr};          ///< Sample name field (max 24 characters).
    QComboBox* mono_combo_{nullptr};         ///< Downmixing mode selector (Mid, Left, Right, Side).
    QLabel* info_label_{nullptr};            ///< Label displaying source vs target audio specifications.
    QLabel* overwrite_warning_{nullptr};     ///< Warning message shown when target slot is occupied.
    QCheckBox* backup_checkbox_{nullptr};    ///< Checkbox enabling backup prior to overwrite.
    WaveformWidget* waveform_widget_{nullptr};///< Interactive preview of converted audio waveform.
    QPushButton* btn_play_{nullptr};         ///< Button to audition the converted audio.
    QPushButton* btn_upload_{nullptr};       ///< Dialog accept / upload trigger button.

    std::vector<volsa2::SampleHeader> current_slots_; ///< Device slot cache for overwrite detection.
    std::vector<int16_t> converted_samples_;          ///< Processed 31.25 kHz PCM samples.
    AlsaAudioPlayer audio_player_{this};              ///< Native ALSA PCM audition player.
};

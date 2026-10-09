/**
 * @file mainwindow.hpp
 * @brief Primary application window for the Volsa 2 Qt GUI.
 * @details Integrates the ALSA device worker, 200-slot interactive sample table,
 *          dynamic memory bar, waveform visualizer, audio player, drag-and-drop handler,
 *          and sample upload/download actions.
 * @author Volsa2 Project Team
 * @date 2026
 */

#pragma once

#include "waveform_widget.hpp"
#include "device_worker.hpp"
#include "audio_player.hpp"
#include "volsa2/proto.hpp"

#include <QMainWindow>
#include <QTableWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QSlider>
#include <QThread>
#include <QTimer>
#include <vector>
#include <unordered_map>

/**
 * @class MainWindow
 * @brief Main GUI application window for Volsa 2 sample librarian.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief Constructs the MainWindow, sets up UI widgets, worker thread, and signal dispatch.
     * @param parent Optional parent QWidget.
     */
    explicit MainWindow(QWidget* parent = nullptr);

    /**
     * @brief Destructor. Halts audio playback and shuts down the background worker thread.
     */
    ~MainWindow() override;

protected:
    /**
     * @brief Drag-and-drop enter event: accepts audio file drop actions.
     * @param event QDragEnterEvent descriptor.
     */
    void dragEnterEvent(QDragEnterEvent* event) override;

    /**
     * @brief Drag-and-drop drop event: launches UploadDialog with the dropped file.
     * @param event QDropEvent descriptor.
     */
    void dropEvent(QDropEvent* event) override;

private slots:
    // Device connection slots
    void onConnectClicked();
    void onDisconnectClicked();
    void onRefreshClicked();

    // Table slots
    void onTableSelectionChanged();
    void onTableCustomContextMenu(const QPoint& pos);
    void onFilterChanged();

    // Sample operations
    void onUploadClicked();
    void onChopClicked();
    void onDownloadClicked();
    void onDeleteClicked();
    void onDownloadPackageClicked();
    void onUploadPackageClicked();

    // Pattern operations
    void onTabChanged(int index);
    void onRefreshPatternsClicked();
    void onRenamePatternClicked();
    void onPatternDoubleClicked(int row, int col);

    // Playback
    void onPlayClicked();
    void onStopClicked();
    void onVolumeChanged(int value);
    void onWaveformSeek(double pos);
    void onSlotSelectionDebounced();

    // Worker signals
    void onWorkerConnected(const QString& version, int channel);
    void onWorkerDisconnected();
    void onWorkerError(const QString& error);
    void onWorkerSpaceUpdated(double occupied, int used_sectors, int all_sectors);
    void onWorkerSlotLoaded(int slot, const volsa2::SampleHeader& header);
    void onWorkerAllSlotsLoaded(const std::vector<volsa2::SampleHeader>& headers);
    void onWorkerPatternLoaded(int slot, const volsa2::PatternData& pattern);
    void onWorkerAllPatternsLoaded(const std::vector<volsa2::PatternData>& patterns);
    void onWorkerPatternUploaded(int slot, const QString& name);
    void onWorkerProgress(int current, int total, const QString& statusText);
    void onWorkerSampleDataReady(int slot, const std::vector<int16_t>& samples);
    void onWorkerSampleUploaded(int slot, const QString& name);
    void onWorkerSampleDeleted(int slot);
    void onWorkerSampleDownloaded(int slot, const QString& filePath);
    void onWorkerBatchFinished(const QString& message);

private:
    void setupUi();
    void setupWorker();
    void updateTableItem(int slot, const volsa2::SampleHeader& header);
    void updatePatternTableItem(int slot, const volsa2::PatternData& pattern);
    int selectedSlot() const;
    std::vector<int> selectedSlots() const;
    int selectedPattern() const;

    // UI components
    QLabel* led_indicator_{nullptr};         ///< Circular LED indicator (green=connected, gray=disconnected).
    QLabel* dev_status_label_{nullptr};      ///< Text status (firmware version & global channel).
    QPushButton* btn_connect_{nullptr};      ///< Connect/Disconnect toggle button.
    QPushButton* btn_refresh_{nullptr};      ///< Refresh all slots button.
    QProgressBar* space_bar_{nullptr};       ///< Memory sector occupancy gauge.
    QProgressBar* operation_progress_{nullptr};///< Ongoing transfer progress bar.

    // Tab widget
    QTabWidget* main_tabs_{nullptr};         ///< Tab widget for Samples and Patterns views.

    // Samples tab
    QLineEdit* search_edit_{nullptr};        ///< Live slot/name filter edit box.
    QCheckBox* hide_empty_check_{nullptr};   ///< Toggle to filter out empty slots.
    QPushButton* btn_upload_{nullptr};       ///< Upload sample button.
    QPushButton* btn_chop_{nullptr};         ///< Chop/slice sample button.
    QPushButton* btn_download_{nullptr};     ///< Download sample(s) button (dynamic label for multiple selection).
    QPushButton* btn_delete_{nullptr};       ///< Erase slot(s) button (dynamic label for multiple selection).
    QPushButton* btn_download_pkg_{nullptr}; ///< Download package button.
    QPushButton* btn_upload_pkg_{nullptr};   ///< Upload package button.
    QTableWidget* table_{nullptr};           ///< Table displaying slots 0 through 199.

    // Bottom preview panel (Samples tab)
    WaveformWidget* waveform_widget_{nullptr};///< Interactive audio waveform viewer.
    QLabel* sample_detail_label_{nullptr};   ///< Selected sample metadata readout.
    QPushButton* btn_play_{nullptr};         ///< Audition playback start button.
    QPushButton* btn_stop_{nullptr};         ///< Audition playback stop button.
    QSlider* volume_slider_{nullptr};        ///< Audition output volume slider.

    // Patterns tab
    QTableWidget* pattern_table_{nullptr};    ///< Table displaying on-board patterns 1 through 16.
    QPushButton* btn_refresh_patterns_{nullptr};///< Button to refresh all patterns from device.
    QPushButton* btn_rename_pattern_{nullptr};///< Button to rename the selected pattern.

    // State & Cache
    std::vector<volsa2::SampleHeader> slots_; ///< Cached copy of all 200 slot headers.
    std::vector<volsa2::PatternData> patterns_;///< Cached copy of all 16 on-board patterns.
    bool patterns_loaded_{false};             ///< Flag indicating if patterns have been fetched from device.
    std::vector<int16_t> current_samples_;   ///< Downloaded PCM samples for active preview.
    std::unordered_map<int, std::vector<int16_t>> sample_cache_; ///< Memory cache for downloaded audio data.
    int active_preview_slot_{-1};            ///< Slot index currently displayed in the waveform viewer.

    // Worker & Thread
    QThread worker_thread_;                  ///< Secondary QThread executing DeviceWorker.
    DeviceWorker* worker_{nullptr};          ///< Worker instance for ALSA MIDI transactions.

    // Audio Playback & Debounce
    AlsaAudioPlayer* audio_player_{nullptr};  ///< Native ALSA PCM audition player.
    QTimer slot_selection_debounce_timer_;   ///< Debounce timer for slot selection during rapid scrolling.
    int pending_fetch_slot_{-1};              ///< Pending slot to download after debounce.
};

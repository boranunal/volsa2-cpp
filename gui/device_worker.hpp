/**
 * @file device_worker.hpp
 * @brief Background QObject worker executing ALSA MIDI SysEx transfers on a secondary thread.
 * @details Ensures the Qt GUI remains fluid and completely non-blocking during prolonged
 *          SysEx transactions (full memory queries, sample uploads, sample downloads).
 * @date 2026
 */

#pragma once

#include "volsa2/device.hpp"

#include <QObject>
#include <QString>
#include <vector>
#include <memory>

Q_DECLARE_METATYPE(volsa2::SampleHeader)
Q_DECLARE_METATYPE(std::vector<volsa2::SampleHeader>)
Q_DECLARE_METATYPE(std::vector<int16_t>)
Q_DECLARE_METATYPE(volsa2::PatternData)
Q_DECLARE_METATYPE(std::vector<volsa2::PatternData>)
Q_DECLARE_METATYPE(std::vector<int>)

/**
 * @class DeviceWorker
 * @brief Asynchronous worker handling all ALSA MIDI sequencer I/O on a background thread.
 * @details Receives commands via queued slots from the UI thread and emits progress,
 *          completion, or error signals back across the thread boundary.
 */
class DeviceWorker : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs a DeviceWorker and registers required Qt meta-types.
     * @param parent Optional parent QObject.
     */
    explicit DeviceWorker(QObject* parent = nullptr);

    /**
     * @brief Destructor. Ensures the ALSA sequencer is cleanly closed.
     */
    ~DeviceWorker() override;

public slots:
    /**
     * @brief Opens ALSA, finds Volca Sample 2, subscribes ports, and performs handshake.
     * @param cooldown_ms Inter-chunk transmission delay in milliseconds (default 10 ms).
     */
    void connectDevice(int cooldown_ms = 10);

    /**
     * @brief Disconnects from the ALSA Sequencer and resets connection state.
     */
    void disconnectDevice();

    /**
     * @brief Iterates through all 200 sample slots, fetching metadata headers sequentially.
     */
    void refreshAllSlots();

    /**
     * @brief Iterates through all 16 sequencer patterns, downloading pattern data from device.
     */
    void refreshAllPatterns();

    /**
     * @brief Downloads PCM audio sample data for a single slot.
     * @param slot Slot index (0-199).
     */
    void fetchSample(int slot);

    /**
     * @brief Uploads a sample header and raw PCM audio payload to the device.
     * @param slot Target slot index (0-199).
     * @param name Desired sample name (up to 24 characters).
     * @param data 16-bit signed mono PCM samples at 31,250 Hz.
     */
    void uploadSample(int slot, const QString& name, const std::vector<int16_t>& data);

    /**
     * @brief Uploads a single pattern to the device.
     * @param slot Pattern slot index (0-15).
     * @param pattern PatternData containing 7,936 bytes binary payload.
     */
    void uploadPattern(int slot, const volsa2::PatternData& pattern);

    /**
     * @brief Erases a sample slot on the device by transmitting an empty header.
     * @param slot Slot index (0-199).
     */
    void deleteSample(int slot);

    /**
     * @brief Downloads a sample from the device and writes it to disk as a RIFF WAV file.
     * @param slot Slot index (0-199).
     * @param filePath Target destination filepath on disk.
     */
    void downloadSample(int slot, const QString& filePath);

    /**
     * @brief Downloads multiple samples from the device into a destination directory.
     * @param slot_indices Collection of slot indices to download.
     * @param targetDir Destination directory path on disk.
     */
    void downloadSamples(const std::vector<int>& slot_indices, const QString& targetDir);

    /**
     * @brief Erases multiple sample slots sequentially on the device.
     * @param slot_indices Collection of slot indices to erase.
     */
    void deleteSamples(const std::vector<int>& slot_indices);

    /**
     * @brief Downloads all 16 patterns and 200 samples and archives them into an .ivlcsplpreset file.
     * @param filePath Target package file destination on disk.
     * @param presetName Custom preset collection name.
     * @param author Creator or sound designer name.
     */
    void downloadPackage(const QString& filePath, const QString& presetName, const QString& author);

    /**
     * @brief Uploads an .ivlcsplpreset package file to the device.
     * @param filePath Target package file on disk.
     * @param uploadSamples Whether to transmit sample headers and audio.
     * @param uploadPatterns Whether to transmit all 16 sequencer patterns.
     * @param eraseEmpty Whether to erase slots that are empty in the package.
     */
    void uploadPackage(const QString& filePath, bool uploadSamples = true, bool uploadPatterns = true, bool eraseEmpty = true);

signals:
    /** @brief Emitted when connection and inquiry handshake succeed. */
    void deviceConnected(const QString& version, int channel);

    /** @brief Emitted when device is disconnected. */
    void deviceDisconnected();

    /** @brief Emitted when an error or exception occurs during an ALSA operation. */
    void deviceError(const QString& error);

    /** @brief Emitted when memory sector capacity data is retrieved. */
    void spaceUpdated(double occupied, int used_sectors, int all_sectors);

    /** @brief Emitted when an individual slot header has been retrieved. */
    void slotLoaded(int slot, const volsa2::SampleHeader& header);

    /** @brief Emitted when a full scan of all 200 slots has completed. */
    void allSlotsLoaded(const std::vector<volsa2::SampleHeader>& headers);

    /** @brief Emitted when an individual pattern has been retrieved from the device. */
    void patternLoaded(int slot, const volsa2::PatternData& pattern);

    /** @brief Emitted when all 16 patterns have been retrieved from the device. */
    void allPatternsLoaded(const std::vector<volsa2::PatternData>& patterns);

    /** @brief Emitted after a pattern has been successfully uploaded to the device. */
    void patternUploaded(int slot, const QString& name);

    /** @brief Emitted during multi-step transfers to update progress indicators. */
    void progress(int current, int total, const QString& statusText);

    /** @brief Emitted when raw audio PCM data has been downloaded for waveform display. */
    void sampleDataReady(int slot, const std::vector<int16_t>& samples);

    /** @brief Emitted after a sample is successfully uploaded to the device. */
    void sampleUploaded(int slot, const QString& name);

    /** @brief Emitted after a slot has been successfully erased. */
    void sampleDeleted(int slot);

    /** @brief Emitted after a sample is successfully downloaded and saved to disk. */
    void sampleDownloaded(int slot, const QString& filePath);

    /** @brief Emitted when an entire .ivlcsplpreset library package has been written to disk. */
    void packageDownloaded(const QString& filePath);

    /** @brief Emitted when an entire .ivlcsplpreset library package has been uploaded to the device. */
    void packageUploaded(const QString& filePath);

    /** @brief Emitted after a batch operation (e.g. download all) completes. */
    void batchFinished(const QString& message);

private:
    std::unique_ptr<volsa2::Device> device_; ///< Owned Device instance executing on this worker's thread.
};

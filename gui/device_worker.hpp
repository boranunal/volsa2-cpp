/**
 * @file device_worker.hpp
 * @brief Background QObject worker executing ALSA MIDI SysEx transfers on a secondary thread.
 * @details Ensures the Qt GUI remains fluid and completely non-blocking during prolonged
 *          SysEx transactions (full memory queries, sample uploads, sample downloads).
 * @author Volsa2 Project Team
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
     * @brief Batch downloads all non-empty sample slots into a target folder.
     * @param destinationDir Directory path where WAV files should be saved.
     */
    void downloadAllSamples(const QString& destinationDir);

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

    /** @brief Emitted after a batch operation (e.g. download all) completes. */
    void batchFinished(const QString& message);

private:
    std::unique_ptr<volsa2::Device> device_; ///< Owned Device instance executing on this worker's thread.
};

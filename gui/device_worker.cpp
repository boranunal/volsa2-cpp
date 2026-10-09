/**
 * @file device_worker.cpp
 * @brief Implementation of asynchronous background ALSA worker slots and signal dispatch.
 * @author Volsa2 Project Team
 * @date 2026
 */

#include "device_worker.hpp"
#include "volsa2/audio.hpp"
#include "volsa2/package.hpp"

#include <filesystem>

namespace fs = std::filesystem;

DeviceWorker::DeviceWorker(QObject* parent)
    : QObject(parent) {
    qRegisterMetaType<volsa2::SampleHeader>("volsa2::SampleHeader");
    qRegisterMetaType<std::vector<volsa2::SampleHeader>>("std::vector<volsa2::SampleHeader>");
    qRegisterMetaType<std::vector<int16_t>>("std::vector<int16_t>");
}

DeviceWorker::~DeviceWorker() {
    disconnectDevice();
}

/**
 * @brief Connects to the device, queries channel/version and memory sectors.
 * @param cooldown_ms Inter-chunk cooldown.
 */
void DeviceWorker::connectDevice(int cooldown_ms) {
    try {
        device_ = std::make_unique<volsa2::Device>(std::chrono::milliseconds(cooldown_ms));
        device_->connect();

        emit deviceConnected(QString::fromStdString(device_->version().to_string()),
                             static_cast<int>(device_->channel().as_u8()));

        auto space = device_->get_sample_space();
        emit spaceUpdated(space.occupied(), space.used_sector_size, space.all_sector_size);

    } catch (const std::exception& e) {
        device_.reset();
        emit deviceError(QString("Connection failed: %1").arg(e.what()));
    }
}

/**
 * @brief Closes the connection and notifies listeners.
 */
void DeviceWorker::disconnectDevice() {
    if (device_) {
        device_->disconnect();
        device_.reset();
        emit deviceDisconnected();
    }
}

/**
 * @brief Queries metadata headers for slots 0 through 199.
 * @details Emits slotLoaded() as each individual slot is read, allowing the UI table
 *          to populate incrementally.
 */
void DeviceWorker::refreshAllSlots() {
    if (!device_ || !device_->is_connected()) {
        emit deviceError("Device is not connected");
        return;
    }

    try {
        std::vector<volsa2::SampleHeader> headers;
        headers.reserve(200);

        for (uint8_t i = 0; i < 200; ++i) {
            emit progress(i + 1, 200, QString("Querying slot %1 of 200...").arg(i + 1));
            auto h = device_->get_sample_header(i);
            emit slotLoaded(i, h);
            headers.push_back(std::move(h));
        }

        auto space = device_->get_sample_space();
        emit spaceUpdated(space.occupied(), space.used_sector_size, space.all_sector_size);

        emit allSlotsLoaded(headers);
        emit progress(200, 200, "Done reading all slots.");

    } catch (const std::exception& e) {
        emit deviceError(QString("Failed to read sample slots: %1").arg(e.what()));
    }
}

/**
 * @brief Downloads audio data for a single slot to feed waveform visualization and playback.
 * @param slot Slot index (0-199).
 */
void DeviceWorker::fetchSample(int slot) {
    if (!device_ || !device_->is_connected()) {
        emit deviceError("Device is not connected");
        return;
    }

    try {
        emit progress(0, 100, QString("Downloading audio from slot %1...").arg(slot));
        auto data = device_->get_sample(static_cast<uint8_t>(slot));
        emit sampleDataReady(slot, data.data);
        emit progress(100, 100, "Download completed.");
    } catch (const std::exception& e) {
        emit deviceError(QString("Failed to fetch sample %1: %2").arg(slot).arg(e.what()));
    }
}

/**
 * @brief Uploads sample header and audio payload, updates space metrics and slot cache.
 * @param slot Target slot.
 * @param name Sample label.
 * @param data PCM audio vector.
 */
void DeviceWorker::uploadSample(int slot, const QString& name, const std::vector<int16_t>& data) {
    if (!device_ || !device_->is_connected()) {
        emit deviceError("Device is not connected");
        return;
    }

    try {
        emit progress(0, 100, QString("Uploading \"%1\" to slot %2...").arg(name).arg(slot));
        auto [header, sample_data] = volsa2::SampleData::create(static_cast<uint8_t>(slot),
                                                                name.toStdString(),
                                                                data);
        device_->send_sample(header, sample_data, [this, slot, &name](size_t sent, size_t total) {
            int pct = total > 0 ? static_cast<int>((sent * 100) / total) : 0;
            emit progress(pct, 100, QString("Uploading \"%1\" to slot %2 (%3%)...").arg(name).arg(slot).arg(pct));
        });

        auto space = device_->get_sample_space();
        emit spaceUpdated(space.occupied(), space.used_sector_size, space.all_sector_size);

        emit slotLoaded(slot, header);
        emit sampleUploaded(slot, name);
        emit progress(100, 100, "Upload completed successfully.");

    } catch (const std::exception& e) {
        emit deviceError(QString("Failed to upload sample to slot %1: %2").arg(slot).arg(e.what()));
    }
}

/**
 * @brief Erases a sample slot and refreshes sector storage usage.
 * @param slot Target slot.
 */
void DeviceWorker::deleteSample(int slot) {
    if (!device_ || !device_->is_connected()) {
        emit deviceError("Device is not connected");
        return;
    }

    try {
        emit progress(0, 100, QString("Erasing slot %1...").arg(slot));
        device_->delete_sample(static_cast<uint8_t>(slot));

        auto space = device_->get_sample_space();
        emit spaceUpdated(space.occupied(), space.used_sector_size, space.all_sector_size);

        emit slotLoaded(slot, volsa2::SampleHeader::empty(static_cast<uint8_t>(slot)));
        emit sampleDeleted(slot);
        emit progress(100, 100, "Slot erased successfully.");

    } catch (const std::exception& e) {
        emit deviceError(QString("Failed to erase slot %1: %2").arg(slot).arg(e.what()));
    }
}

/**
 * @brief Downloads audio data for a slot and exports it to a standard WAV file.
 * @param slot Target slot.
 * @param filePath Output file path.
 */
void DeviceWorker::downloadSample(int slot, const QString& filePath) {
    if (!device_ || !device_->is_connected()) {
        emit deviceError("Device is not connected");
        return;
    }

    try {
        emit progress(0, 100, QString("Downloading slot %1 to file...").arg(slot));
        auto sample_data = device_->get_sample(static_cast<uint8_t>(slot));
        volsa2::write_wav_file(filePath.toStdString(), sample_data.data);

        emit sampleDownloaded(slot, filePath);
        emit progress(100, 100, QString("Saved to %1").arg(filePath));

    } catch (const std::exception& e) {
        emit deviceError(QString("Failed to download sample %1: %2").arg(slot).arg(e.what()));
    }
}

/**
 * @brief Batch iterates through all 200 slots, downloading every occupied sample to a directory.
 * @param destinationDir Destination folder.
 */
void DeviceWorker::downloadAllSamples(const QString& destinationDir) {
    if (!device_ || !device_->is_connected()) {
        emit deviceError("Device is not connected");
        return;
    }

    try {
        fs::path dest_dir = destinationDir.toStdString();
        fs::create_directories(dest_dir);

        int downloaded_count = 0;
        for (uint8_t i = 0; i < 200; ++i) {
            emit progress(i + 1, 200, QString("Checking slot %1 of 200...").arg(i + 1));
            auto header = device_->get_sample_header(i);
            if (!header.is_empty()) {
                auto data = device_->get_sample(i);
                std::string fname = (i < 10 ? "00" : (i < 100 ? "0" : "")) + std::to_string(i) + "_" +
                                    (header.name.empty() ? "sample" : header.name) + ".wav";
                fs::path target = dest_dir / fname;
                volsa2::write_wav_file(target, data.data);
                downloaded_count++;
            }
        }

        emit batchFinished(QString("Exported %1 samples to %2").arg(downloaded_count).arg(destinationDir));
        emit progress(200, 200, "Export completed.");

    } catch (const std::exception& e) {
        emit deviceError(QString("Batch export failed: %1").arg(e.what()));
    }
}

/**
 * @brief Downloads all 16 patterns and 200 samples and archives them into an .ivlcsplpreset file.
 */
void DeviceWorker::downloadPackage(const QString& filePath, const QString& presetName, const QString& author) {
    if (!device_ || !device_->is_connected()) {
        emit deviceError("Device is not connected");
        return;
    }

    try {
        fs::path out_path = filePath.toStdString();
        auto pkg = volsa2::PackageData::create_default(
            presetName.isEmpty() ? out_path.stem().string() : presetName.toStdString(),
            author.toStdString()
        );

        int total_steps = 16 + 200 + 1;
        int current_step = 0;

        // Phase 1: 16 Patterns
        for (uint8_t i = 0; i < volsa2::PatternData::MAX_PATTERNS; ++i) {
            emit progress(++current_step, total_steps, QString("Downloading pattern %1 of 16...").arg(i + 1));
            auto pat = device_->get_pattern(i);
            pkg.programs[i].pattern = std::move(pat);
        }

        // Phase 2: 200 Sample Headers
        std::vector<uint8_t> active_slots;
        for (uint8_t i = 0; i < 200; ++i) {
            if (i % 10 == 0 || i == 199) {
                emit progress(current_step, total_steps, QString("Scanning sample headers (%1/200)...").arg(i + 1));
            }
            auto h = device_->get_sample_header(i);
            pkg.samples[i].header = h;
            if (!h.is_empty()) {
                active_slots.push_back(i);
            }
            current_step++;
        }

        total_steps += static_cast<int>(active_slots.size());

        // Phase 3: Sample PCM Audio
        for (size_t idx = 0; idx < active_slots.size(); ++idx) {
            uint8_t slot = active_slots[idx];
            emit progress(++current_step, total_steps,
                          QString("Downloading sample audio %1/%2 (slot %3: %4)...")
                          .arg(idx + 1)
                          .arg(active_slots.size())
                          .arg(slot)
                          .arg(QString::fromStdString(pkg.samples[slot].header.name)));
            auto sdata = device_->get_sample(slot);
            pkg.samples[slot].data = std::move(sdata);
        }

        // Phase 4: Write ZIP package
        emit progress(total_steps - 1, total_steps, "Packing into .ivlcsplpreset archive...");
        volsa2::save_package(out_path, pkg);

        emit progress(total_steps, total_steps, "Package download complete.");
        emit packageDownloaded(filePath);
        emit batchFinished(QString("Package saved successfully to %1").arg(filePath));

    } catch (const std::exception& e) {
        emit deviceError(QString("Package download failed: %1").arg(e.what()));
    }
}

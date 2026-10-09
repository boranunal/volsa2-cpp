/**
 * @file device_worker.cpp
 * @brief Implementation of asynchronous background ALSA worker slots and signal dispatch.
 * @author Volsa2 Project Team
 * @date 2026
 */

#include "device_worker.hpp"
#include "volsa2/audio.hpp"
#include "volsa2/package.hpp"

#include <QFileInfo>
#include <filesystem>

namespace fs = std::filesystem;

DeviceWorker::DeviceWorker(QObject* parent)
    : QObject(parent) {
    qRegisterMetaType<volsa2::SampleHeader>("volsa2::SampleHeader");
    qRegisterMetaType<std::vector<volsa2::SampleHeader>>("std::vector<volsa2::SampleHeader>");
    qRegisterMetaType<std::vector<int16_t>>("std::vector<int16_t>");
    qRegisterMetaType<volsa2::PatternData>("volsa2::PatternData");
    qRegisterMetaType<std::vector<volsa2::PatternData>>("std::vector<volsa2::PatternData>");
    qRegisterMetaType<std::vector<int>>("std::vector<int>");
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
 * @brief Downloads multiple samples from the device into a destination directory.
 * @param slots Collection of slot indices to download.
 * @param targetDir Destination directory path on disk.
 */
void DeviceWorker::downloadSamples(const std::vector<int>& slot_indices, const QString& targetDir) {
    if (!device_ || !device_->is_connected()) {
        emit deviceError("Device is not connected");
        return;
    }

    try {
        fs::path dir = targetDir.toStdString();
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
        }

        int count = 0;
        int total = static_cast<int>(slot_indices.size());

        for (int slot : slot_indices) {
            count++;
            auto header = device_->get_sample_header(static_cast<uint8_t>(slot));
            if (header.is_empty()) {
                continue;
            }

            std::string name = header.name;
            if (name.empty()) {
                name = "sample_" + std::to_string(slot);
            }
            for (char& c : name) {
                if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                    c = '_';
                }
            }

            char fname[128];
            std::snprintf(fname, sizeof(fname), "%03d_%s.wav", slot, name.c_str());
            fs::path out_file = dir / fname;

            emit progress(count, total, QString("Downloading sample %1/%2 (slot %3: %4)...")
                          .arg(count).arg(total).arg(slot).arg(QString::fromStdString(header.name)));

            auto sdata = device_->get_sample(static_cast<uint8_t>(slot));
            volsa2::write_wav_file(out_file.string(), sdata.data);
        }

        emit progress(total, total, "Download complete.");
        emit batchFinished(QString("Successfully exported %1 samples to directory:\n%2").arg(count).arg(targetDir));

    } catch (const std::exception& e) {
        emit deviceError(QString("Failed to download samples: %1").arg(e.what()));
    }
}

/**
 * @brief Erases multiple sample slots sequentially on the device.
 * @param slot_indices Collection of slot indices to erase.
 */
void DeviceWorker::deleteSamples(const std::vector<int>& slot_indices) {
    if (!device_ || !device_->is_connected()) {
        emit deviceError("Device is not connected");
        return;
    }

    try {
        int count = 0;
        int total = static_cast<int>(slot_indices.size());

        for (int slot : slot_indices) {
            count++;
            emit progress(count, total, QString("Erasing slot %1 (%2/%3)...")
                          .arg(slot).arg(count).arg(total));

            device_->delete_sample(static_cast<uint8_t>(slot));
            emit slotLoaded(slot, volsa2::SampleHeader::empty(static_cast<uint8_t>(slot)));
            emit sampleDeleted(slot);
        }

        auto space = device_->get_sample_space();
        emit spaceUpdated(space.occupied(), space.used_sector_size, space.all_sector_size);

        emit progress(total, total, "Erasing complete.");
        emit batchFinished(QString("Successfully erased %1 sample slots.").arg(total));

    } catch (const std::exception& e) {
        emit deviceError(QString("Failed to erase samples: %1").arg(e.what()));
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

/**
 * @brief Iterates through all 16 patterns on the device, downloading pattern data sequentially.
 */
void DeviceWorker::refreshAllPatterns() {
    if (!device_ || !device_->is_connected()) {
        emit deviceError("Device is not connected");
        return;
    }

    try {
        std::vector<volsa2::PatternData> patterns;
        patterns.reserve(16);
        for (uint8_t i = 0; i < 16; ++i) {
            emit progress(i + 1, 16, QString("Loading pattern %1 of 16...").arg(i + 1));
            auto pat = device_->get_pattern(i);
            emit patternLoaded(i, pat);
            patterns.push_back(std::move(pat));
        }
        emit allPatternsLoaded(patterns);
        emit progress(16, 16, "Patterns loaded successfully.");
    } catch (const std::exception& e) {
        emit deviceError(QString("Failed to load patterns: %1").arg(e.what()));
    }
}

/**
 * @brief Transmits a single pattern payload to the device.
 */
void DeviceWorker::uploadPattern(int slot, const volsa2::PatternData& pattern) {
    if (!device_ || !device_->is_connected()) {
        emit deviceError("Device is not connected");
        return;
    }

    try {
        emit progress(0, 100, QString("Writing pattern %1 to Volca...").arg(slot + 1));
        device_->send_pattern(pattern);
        emit patternUploaded(slot, QString::fromStdString(pattern.name));
        emit progress(100, 100, QString("Pattern %1 updated.").arg(slot + 1));
    } catch (const std::exception& e) {
        emit deviceError(QString("Failed to write pattern %1: %2").arg(slot + 1).arg(e.what()));
    }
}

/**
 * @brief Restores all samples and/or patterns from an .ivlcsplpreset file to the device.
 */
void DeviceWorker::uploadPackage(const QString& filePath, bool uploadSamples, bool uploadPatterns, bool eraseEmpty) {
    if (!device_ || !device_->is_connected()) {
        emit deviceError("Device is not connected");
        return;
    }

    try {
        fs::path in_path = filePath.toStdString();
        emit progress(0, 100, QString("Reading package archive %1...").arg(filePath));
        auto pkg = volsa2::load_package(in_path);

        std::vector<uint8_t> active_slots;
        std::vector<bool> is_pkg_active(200, false);
        for (uint8_t i = 0; i < 200; ++i) {
            if (pkg.samples[i].data.has_value() && !pkg.samples[i].data->data.empty()) {
                active_slots.push_back(i);
                is_pkg_active[i] = true;
            }
        }

        // Identify existing device slots that are empty in the package to erase first
        std::vector<uint8_t> slots_to_erase;
        if (uploadSamples && eraseEmpty) {
            emit progress(0, 100, "Scanning device memory to identify unused slots to free...");
            for (uint8_t i = 0; i < 200; ++i) {
                if (!is_pkg_active[i]) {
                    auto h = device_->get_sample_header(i);
                    if (!h.is_empty()) {
                        slots_to_erase.push_back(i);
                    }
                }
            }
        }

        int total_steps = 0;
        if (uploadSamples) {
            total_steps += static_cast<int>(slots_to_erase.size()) + static_cast<int>(active_slots.size());
        }
        if (uploadPatterns) {
            total_steps += static_cast<int>(pkg.programs.size());
        }
        total_steps += 1; // For memory refresh

        if (total_steps <= 1) {
            emit batchFinished("No transfer options selected.");
            return;
        }

        int current_step = 0;

        // Phase 1: Free memory FIRST by erasing old occupied slots on the device that are empty in the package
        if (uploadSamples && !slots_to_erase.empty()) {
            for (size_t idx = 0; idx < slots_to_erase.size(); ++idx) {
                uint8_t slot = slots_to_erase[idx];
                emit progress(++current_step, total_steps,
                              QString("Freeing memory: erasing old slot %1 (%2/%3)...")
                              .arg(slot)
                              .arg(idx + 1)
                              .arg(slots_to_erase.size()));
                device_->delete_sample(slot);
            }
        }

        // Phase 2: Upload active samples from the package
        if (uploadSamples) {
            for (size_t idx = 0; idx < active_slots.size(); ++idx) {
                uint8_t slot = active_slots[idx];
                const auto& s = pkg.samples[slot];
                emit progress(++current_step, total_steps,
                              QString("Uploading sample %1/%2 (slot %3: %4)...")
                              .arg(idx + 1)
                              .arg(active_slots.size())
                              .arg(slot)
                              .arg(QString::fromStdString(s.header.name)));
                device_->send_sample(s.header, *s.data);
            }
        }

        // Phase 3: Upload Patterns
        if (uploadPatterns) {
            for (size_t i = 0; i < pkg.programs.size(); ++i) {
                const auto& pat = pkg.programs[i].pattern;
                emit progress(++current_step, total_steps,
                              QString("Uploading pattern %1/16 (slot %2: %3)...")
                              .arg(i + 1)
                              .arg(pat.pattern_no)
                              .arg(QString::fromStdString(pat.name)));
                device_->send_pattern(pat);
            }
        }

        // Phase 4: Refresh slot headers and memory gauge
        emit progress(total_steps, total_steps, "Updating slot table and device memory status...");
        refreshAllSlots();

        emit packageUploaded(filePath);
        emit batchFinished(QString("Package successfully restored to Volca Sample 2: %1").arg(QFileInfo(filePath).fileName()));

    } catch (const std::exception& e) {
        emit deviceError(QString("Package upload failed: %1").arg(e.what()));
    }
}


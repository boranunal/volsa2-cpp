/**
 * @file audio_player.hpp
 * @brief Direct ALSA PCM audio playback service for waveform auditioning and previews.
 * @details Replaces QtMultimedia QAudioSink to guarantee native playback on all Linux
 *          audio configurations (ALSA, PulseAudio, PipeWire) without plugin/device detection failures.
 * @author Volsa2 Project Team
 * @date 2026
 */

#pragma once

#include <QObject>
#include <vector>
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>

class AlsaAudioPlayer : public QObject {
    Q_OBJECT

public:
    explicit AlsaAudioPlayer(QObject* parent = nullptr);
    ~AlsaAudioPlayer() override;

    /**
     * @brief Starts playback of 16-bit PCM audio samples.
     * @param samples Audio sample buffer.
     * @param sample_rate Sampling rate in Hz (default: 31,250 Hz).
     * @param start_pos Fractional starting position [0.0, 1.0].
     */
    void play(const std::vector<int16_t>& samples, uint32_t sample_rate = 31250, double start_pos = 0.0);

    /**
     * @brief Immediately stops audio playback and drops buffered audio.
     */
    void stop();

    /**
     * @brief Checks if playback is actively running.
     */
    bool isPlaying() const noexcept { return is_playing_.load(); }

    /**
     * @brief Sets playback volume.
     * @param volume_pct Volume percentage (0 to 100).
     */
    void setVolume(int volume_pct) noexcept { volume_pct_.store(std::clamp(volume_pct, 0, 100)); }

signals:
    /**
     * @brief Emitted periodically during playback with normalized position [0.0, 1.0].
     */
    void positionChanged(double normalized_pos);

    /**
     * @brief Emitted when playback finishes or is stopped.
     */
    void playbackFinished();

private:
    void workerLoop(std::vector<int16_t> samples, uint32_t sample_rate, size_t start_frame);

    std::atomic<bool> is_playing_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<int> volume_pct_{80};
    std::thread worker_thread_;
    std::mutex thread_mutex_;
};

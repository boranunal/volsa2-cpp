/**
 * @file audio_player.cpp
 * @brief Implementation of AlsaAudioPlayer direct PCM playback engine.
 * @date 2026
 */

#include "audio_player.hpp"

#include <alsa/asoundlib.h>
#include <algorithm>
#include <iostream>

AlsaAudioPlayer::AlsaAudioPlayer(QObject* parent)
    : QObject(parent) {}

AlsaAudioPlayer::~AlsaAudioPlayer() {
    stop();
}

void AlsaAudioPlayer::stop() {
    stop_requested_.store(true);
    std::lock_guard<std::mutex> lock(thread_mutex_);
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    is_playing_.store(false);
}

void AlsaAudioPlayer::play(const std::vector<int16_t>& samples, uint32_t sample_rate, double start_pos) {
    stop();

    if (samples.empty()) {
        emit playbackFinished();
        return;
    }

    size_t start_frame = static_cast<size_t>(std::clamp(start_pos, 0.0, 1.0) * samples.size());
    if (start_frame >= samples.size()) {
        start_frame = 0;
    }

    stop_requested_.store(false);
    is_playing_.store(true);

    std::lock_guard<std::mutex> lock(thread_mutex_);
    worker_thread_ = std::thread(&AlsaAudioPlayer::workerLoop, this, samples, sample_rate, start_frame);
}

void AlsaAudioPlayer::workerLoop(std::vector<int16_t> samples, uint32_t sample_rate, size_t start_frame) {
    snd_pcm_t* pcm = nullptr;
    int err = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        std::cerr << "AlsaAudioPlayer: snd_pcm_open failed: " << snd_strerror(err) << "\n";
        is_playing_.store(false);
        emit playbackFinished();
        return;
    }

    bool is_stereo = false;
    // Attempt 1: Configure mono (1 channel) with soft-resampling
    err = snd_pcm_set_params(pcm,
                             SND_PCM_FORMAT_S16_LE,
                             SND_PCM_ACCESS_RW_INTERLEAVED,
                             1,
                             sample_rate,
                             1, // soft resample enabled
                             50000); // 50ms buffer latency

    if (err < 0) {
        // Attempt 2: Fallback to stereo (2 channels) for DACs that reject mono
        err = snd_pcm_set_params(pcm,
                                 SND_PCM_FORMAT_S16_LE,
                                 SND_PCM_ACCESS_RW_INTERLEAVED,
                                 2,
                                 sample_rate,
                                 1,
                                 50000);
        if (err == 0) {
            is_stereo = true;
        } else {
            std::cerr << "AlsaAudioPlayer: snd_pcm_set_params failed: " << snd_strerror(err) << "\n";
            snd_pcm_close(pcm);
            is_playing_.store(false);
            emit playbackFinished();
            return;
        }
    }

    const size_t total_frames = samples.size();
    size_t pos = start_frame;
    const size_t chunk_size = 512;
    std::vector<int16_t> chunk(is_stereo ? chunk_size * 2 : chunk_size);

    while (pos < total_frames && !stop_requested_.load()) {
        size_t frames_to_write = std::min(chunk_size, total_frames - pos);
        float vol = std::clamp(volume_pct_.load(), 0, 100) / 100.0f;

        if (is_stereo) {
            for (size_t i = 0; i < frames_to_write; ++i) {
                int16_t s = static_cast<int16_t>(samples[pos + i] * vol);
                chunk[i * 2]     = s;
                chunk[i * 2 + 1] = s;
            }
        } else {
            for (size_t i = 0; i < frames_to_write; ++i) {
                chunk[i] = static_cast<int16_t>(samples[pos + i] * vol);
            }
        }

        snd_pcm_sframes_t written = snd_pcm_writei(pcm, chunk.data(), frames_to_write);
        if (written < 0) {
            written = snd_pcm_recover(pcm, static_cast<int>(written), 0);
        }

        if (written > 0) {
            pos += static_cast<size_t>(written);
            double norm = static_cast<double>(pos) / static_cast<double>(total_frames);
            emit positionChanged(norm);
        } else if (written < 0) {
            // Unrecoverable write error
            break;
        }
    }

    if (stop_requested_.load()) {
        snd_pcm_drop(pcm);
        emit positionChanged(-1.0);
    } else {
        snd_pcm_drain(pcm);
        emit positionChanged(1.0);
    }

    snd_pcm_close(pcm);
    is_playing_.store(false);
    emit playbackFinished();
}

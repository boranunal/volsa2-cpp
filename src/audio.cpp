/**
 * @file audio.cpp
 * @brief Implementation of audio reading, resampling, downmixing, and WAV writing.
 * @details Leverages libsndfile for decoding arbitrary audio codecs, libsamplerate
 *          for high-fidelity sinc resampling, and custom binary formatting for RIFF WAV export.
 * @date 2026
 */

#include "volsa2/audio.hpp"

#include <sndfile.h>
#include <samplerate.h>

#include <cmath>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace volsa2 {

/**
 * @brief Extracts audio metadata headers from a file using libsndfile.
 * @param path Path to audio file.
 * @return Populated AudioInfo structure.
 */
AudioInfo inspect_audio_file(const std::filesystem::path& path) {
    SF_INFO sfinfo;
    std::memset(&sfinfo, 0, sizeof(sfinfo));

    SNDFILE* sndfile = sf_open(path.string().c_str(), SFM_READ, &sfinfo);
    if (!sndfile) {
        throw std::runtime_error("Failed to open audio file: " + path.string() + " (" + sf_strerror(nullptr) + ")");
    }

    AudioInfo info;
    info.sample_rate = static_cast<uint32_t>(sfinfo.samplerate);
    info.channels = static_cast<uint32_t>(sfinfo.channels);
    info.frames = static_cast<uint64_t>(sfinfo.frames);
    info.duration_seconds = (info.sample_rate > 0) ? (static_cast<double>(info.frames) / info.sample_rate) : 0.0;
    info.format = sfinfo.format;

    sf_close(sndfile);
    return info;
}

/**
 * @brief Complete DSP pipeline: decoding, channel mix, sinc resampling, and int16 quantization.
 * @param path Path to source audio.
 * @param mode Downmixing mode (Mid, Left, Right, Side).
 * @return Vector of 16-bit PCM samples ready for Volca Sample 2 transmission.
 */
std::vector<int16_t> load_and_convert_audio(const std::filesystem::path& path, MonoMode mode) {
    SF_INFO sfinfo;
    std::memset(&sfinfo, 0, sizeof(sfinfo));

    SNDFILE* sndfile = sf_open(path.string().c_str(), SFM_READ, &sfinfo);
    if (!sndfile) {
        throw std::runtime_error("Failed to open audio file: " + path.string() + " (" + sf_strerror(nullptr) + ")");
    }

    const sf_count_t num_frames = sfinfo.frames;
    const int num_channels = sfinfo.channels;
    if (num_frames <= 0 || num_channels <= 0) {
        sf_close(sndfile);
        return {};
    }

    // Step 1: Read all interleaved audio frames as double-precision floats
    std::vector<double> interleaved(static_cast<size_t>(num_frames * num_channels));
    sf_count_t frames_read = sf_readf_double(sndfile, interleaved.data(), num_frames);
    sf_close(sndfile);

    if (frames_read <= 0) {
        return {};
    }

    // Step 2: Downmix to single-channel (mono) float buffer
    std::vector<float> mono(static_cast<size_t>(frames_read));
    if (num_channels == 1) {
        for (sf_count_t i = 0; i < frames_read; ++i) {
            mono[static_cast<size_t>(i)] = static_cast<float>(interleaved[static_cast<size_t>(i)]);
        }
    } else {
        for (sf_count_t i = 0; i < frames_read; ++i) {
            double left = interleaved[static_cast<size_t>(i * num_channels + 0)];
            double right = interleaved[static_cast<size_t>(i * num_channels + 1)];
            double val = 0.0;

            switch (mode) {
                case MonoMode::Left:
                    val = left;
                    break;
                case MonoMode::Right:
                    val = right;
                    break;
                case MonoMode::Mid:
                    val = (left + right) * 0.5; // Phantom center sum
                    break;
                case MonoMode::Side:
                    val = (left - right) * 0.5; // Stereo difference
                    break;
            }
            mono[static_cast<size_t>(i)] = static_cast<float>(val);
        }
    }

    // Step 3: Resample to 31,250 Hz if source rate differs
    if (static_cast<uint32_t>(sfinfo.samplerate) != VOLCA_SAMPLERATE) {
        const double src_ratio = static_cast<double>(VOLCA_SAMPLERATE) / static_cast<double>(sfinfo.samplerate);
        const size_t max_out_frames = static_cast<size_t>(std::ceil(mono.size() * src_ratio)) + 256;
        std::vector<float> resampled(max_out_frames);

        SRC_DATA src_data;
        src_data.data_in = mono.data();
        src_data.input_frames = static_cast<long>(mono.size());
        src_data.data_out = resampled.data();
        src_data.output_frames = static_cast<long>(max_out_frames);
        src_data.src_ratio = src_ratio;

        // Best quality sinc interpolation (anti-aliased)
        int err = src_simple(&src_data, SRC_SINC_BEST_QUALITY, 1);
        if (err != 0) {
            throw std::runtime_error("Resampling error: " + std::string(src_strerror(err)));
        }

        resampled.resize(static_cast<size_t>(src_data.output_frames_gen));
        mono = std::move(resampled);
    }

    // Step 4: Quantize from float to 16-bit signed PCM with clamping
    std::vector<int16_t> pcm;
    pcm.reserve(mono.size());
    for (float sample : mono) {
        double scaled = std::round(static_cast<double>(sample) * 32767.0);
        int32_t clamped = std::clamp<int32_t>(static_cast<int32_t>(scaled), -32768, 32767);
        pcm.push_back(static_cast<int16_t>(clamped));
    }

    return pcm;
}

/**
 * @brief Writes a 16-bit mono PCM buffer to disk as a standard RIFF WAVE file.
 * @details Emits:
 *          - "RIFF" chunk header with total size.
 *          - "WAVE" format identifier.
 *          - "fmt " subchunk (PCM format, 1 channel, sample_rate, 16 bits).
 *          - "data" subchunk with raw sample bytes.
 * @param path Destination file path.
 * @param samples Audio sample buffer.
 * @param sample_rate Sample rate (default 31,250 Hz).
 */
void write_wav_file(const std::filesystem::path& path, std::span<const int16_t> samples, uint32_t sample_rate) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open file for writing: " + path.string());
    }

    const uint32_t num_channels = 1;
    const uint32_t bits_per_sample = 16;
    const uint32_t block_align = num_channels * (bits_per_sample / 8);
    const uint32_t byte_rate = sample_rate * block_align;
    const uint32_t data_bytes = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    const uint32_t riff_chunk_size = 36 + data_bytes;

    // 1. RIFF chunk descriptor
    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&riff_chunk_size), 4);
    out.write("WAVE", 4);

    // 2. "fmt " subchunk
    const uint32_t subchunk1_size = 16;
    const uint16_t audio_format = 1; // PCM
    const uint16_t channels_u16 = static_cast<uint16_t>(num_channels);
    const uint16_t bits_u16 = static_cast<uint16_t>(bits_per_sample);
    const uint16_t align_u16 = static_cast<uint16_t>(block_align);

    out.write("fmt ", 4);
    out.write(reinterpret_cast<const char*>(&subchunk1_size), 4);
    out.write(reinterpret_cast<const char*>(&audio_format), 2);
    out.write(reinterpret_cast<const char*>(&channels_u16), 2);
    out.write(reinterpret_cast<const char*>(&sample_rate), 4);
    out.write(reinterpret_cast<const char*>(&byte_rate), 4);
    out.write(reinterpret_cast<const char*>(&align_u16), 2);
    out.write(reinterpret_cast<const char*>(&bits_u16), 2);

    // 3. "data" subchunk
    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&data_bytes), 4);
    if (!samples.empty()) {
        out.write(reinterpret_cast<const char*>(samples.data()), data_bytes);
    }

    if (!out) {
        throw std::runtime_error("Failed to complete writing WAV file: " + path.string());
    }
}

} // namespace volsa2

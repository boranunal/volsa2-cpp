/**
 * @file audio.hpp
 * @brief Audio processing, format inspection, mono conversion, and resampling pipeline.
 * @details Implements audio decoding for arbitrary media formats (WAV, FLAC, AIFF, OGG)
 *          via libsndfile, channel downmixing modes (Mid, Left, Right, Side), high-quality
 *          sinc band-limited resampling to the Volca Sample 2 native sample rate (31.25 kHz)
 *          via libsamplerate, and standard 16-bit RIFF WAV serialization.
 * @author Volsa2 Project Team
 * @date 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <filesystem>
#include <string_view>

namespace volsa2 {

/**
 * @brief Native sample rate of the KORG Volca Sample 2 hardware (31,250 Hz).
 * @details The Volca Sample 2 digital sound engine processes all audio strictly at 31.25 kHz.
 */
constexpr uint32_t VOLCA_SAMPLERATE = 31250;

/**
 * @enum MonoMode
 * @brief Downmixing strategies for converting stereo or multi-channel audio to mono.
 */
enum class MonoMode {
    Left,  ///< Extract the left channel (channel index 0).
    Right, ///< Extract the right channel (channel index 1).
    Mid,   ///< Mono sum / phantom center: (Left + Right) * 0.5. Standard default.
    Side   ///< Stereo difference / stereo spread: (Left - Right) * 0.5.
};

/**
 * @brief Converts a MonoMode enum value to its string representation.
 * @param mode The downmixing mode.
 * @return String representation ("left", "right", "mid", "side").
 */
inline std::string_view to_string(MonoMode mode) {
    switch (mode) {
        case MonoMode::Left:  return "left";
        case MonoMode::Right: return "right";
        case MonoMode::Mid:   return "mid";
        case MonoMode::Side:  return "side";
    }
    return "mid";
}

/**
 * @brief Parses a string into a MonoMode enum value.
 * @param str String identifier ("left", "right", "side", or "mid").
 * @return Matching MonoMode (defaults to MonoMode::Mid if unknown).
 */
inline MonoMode mono_mode_from_string(std::string_view str) {
    if (str == "left")  return MonoMode::Left;
    if (str == "right") return MonoMode::Right;
    if (str == "side")  return MonoMode::Side;
    return MonoMode::Mid;
}

/**
 * @struct AudioInfo
 * @brief Metadata describing an audio file's encoding and dimensions.
 */
struct AudioInfo {
    uint32_t sample_rate{0};       ///< Sample rate in Hz.
    uint32_t channels{0};          ///< Number of interleaved audio channels.
    uint64_t frames{0};            ///< Total number of sample frames.
    double duration_seconds{0.0};  ///< Duration in seconds (frames / sample_rate).
    int format{0};                 ///< libsndfile bitwise format flags.
};

/**
 * @brief Inspects an audio file without loading its full sample payload into memory.
 * @param path Filesystem path to the audio file.
 * @return AudioInfo containing sample rate, channels, frame count, and duration.
 * @throws std::runtime_error if libsndfile cannot open or identify the file.
 */
AudioInfo inspect_audio_file(const std::filesystem::path& path);

/**
 * @brief Loads an audio file, converts it to mono, resamples to 31.25 kHz, and quantizes to 16-bit PCM.
 * @details Pipeline:
 *          1. Decodes all audio frames into double-precision floating point.
 *          2. Applies the requested MonoMode channel transformation.
 *          3. If the source sample rate differs from VOLCA_SAMPLERATE (31,250 Hz), performs
 *             band-limited sinc interpolation using libsamplerate (SRC_SINC_BEST_QUALITY).
 *          4. Scales floating point samples to 16-bit integer range [-32768, 32767] with rounding
 *             and anti-clipping clamping.
 * @param path Filesystem path to the source audio file.
 * @param mode Channel downmixing mode (defaults to MonoMode::Mid).
 * @return std::vector<int16_t> containing 16-bit mono PCM samples at 31,250 Hz.
 * @throws std::runtime_error on file I/O or resampling failure.
 */
std::vector<int16_t> load_and_convert_audio(const std::filesystem::path& path, MonoMode mode = MonoMode::Mid);

/**
 * @brief Serializes 16-bit mono PCM samples into a standard RIFF/WAVE file.
 * @details Emits a 44-byte standard RIFF WAVE header followed by little-endian 16-bit samples.
 *          Does not require external libraries for writing, guaranteeing standard compliance.
 * @param path Target filesystem path to create or overwrite.
 * @param samples Contiguous span of 16-bit signed PCM audio samples.
 * @param sample_rate Audio sample rate in Hz (defaults to VOLCA_SAMPLERATE).
 * @throws std::runtime_error if the file cannot be opened or written.
 */
void write_wav_file(const std::filesystem::path& path, std::span<const int16_t> samples, uint32_t sample_rate = VOLCA_SAMPLERATE);

} // namespace volsa2

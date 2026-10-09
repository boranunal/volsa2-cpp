/**
 * @file sample_chopper.hpp
 * @brief Core sample chopping, region cropping, silence trimming, and beat slicing utilities.
 * @details Provides fast, memory-safe algorithms for sub-span extraction with anti-click
 *          micro-fades, threshold-based silence detection, equal grid beat slicing,
 *          and energy-flux transient onset detection.
 * @author Volsa2 Project Team
 * @date 2026
 */

#pragma once

#include "volsa2/audio.hpp"

#include <cstdint>
#include <vector>
#include <span>
#include <utility>

namespace volsa2 {

/**
 * @struct SlicePoint
 * @brief Represents boundary coordinates for a single audio slice.
 */
struct SlicePoint {
    size_t start_sample{0};    ///< Starting sample index in source buffer (inclusive).
    size_t end_sample{0};      ///< Ending sample index in source buffer (exclusive).
    double start_seconds{0.0}; ///< Start time offset in seconds.
    double end_seconds{0.0};   ///< End time offset in seconds.

    /**
     * @brief Number of samples contained within this slice.
     */
    [[nodiscard]] size_t length() const {
        return (end_sample > start_sample) ? (end_sample - start_sample) : 0;
    }
};

/**
 * @brief Crops audio samples between start and end indices with optional anti-click micro-fade.
 * @param samples Source audio buffer.
 * @param start_idx Starting sample index (inclusive).
 * @param end_idx Ending sample index (exclusive).
 * @param apply_edge_fade When true, applies 32-sample half-cosine ramp at boundaries to eliminate clicks.
 * @return New contiguous buffer containing cropped PCM samples.
 */
std::vector<int16_t> crop_audio(std::span<const int16_t> samples,
                               size_t start_idx,
                               size_t end_idx,
                               bool apply_edge_fade = true);

/**
 * @brief Locates leading and trailing silence bounds using an RMS power threshold.
 * @param samples Source audio buffer.
 * @param threshold_db Silence threshold in decibels relative to full scale (default -48 dBFS).
 * @return Pair of {start_sample, end_sample} representing audible content boundaries.
 */
std::pair<size_t, size_t> find_silence_bounds(std::span<const int16_t> samples,
                                             double threshold_db = -48.0);

/**
 * @brief Automatically removes leading and trailing silence from an audio sample.
 * @param samples Source audio buffer.
 * @param threshold_db Silence threshold in decibels relative to full scale (default -48 dBFS).
 * @return Trimmed audio buffer.
 */
std::vector<int16_t> auto_trim_silence(std::span<const int16_t> samples,
                                      double threshold_db = -48.0);

/**
 * @brief Divides an audio sample into N equal-duration slices without rounding gaps.
 * @param total_samples Total number of samples in source audio buffer.
 * @param num_slices Number of slices to generate (must be >= 1).
 * @param sample_rate Audio sampling rate in Hz (defaults to 31,250 Hz).
 * @return Vector of SlicePoint structures defining contiguous partitions.
 */
std::vector<SlicePoint> divide_equal_slices(size_t total_samples,
                                           size_t num_slices,
                                           uint32_t sample_rate = VOLCA_SAMPLERATE);

/**
 * @brief Slices audio by detecting sharp transient attacks (drum hits, kicks, snares).
 * @param samples Source audio buffer.
 * @param sample_rate Audio sampling rate in Hz (defaults to 31,250 Hz).
 * @param max_slices Maximum number of slices to produce (default 16).
 * @param sensitivity Transient detection sensitivity from 0.0 (least sensitive) to 1.0 (most sensitive).
 * @return Vector of SlicePoint structures demarcating transient segments.
 */
std::vector<SlicePoint> detect_transient_slices(std::span<const int16_t> samples,
                                               uint32_t sample_rate = VOLCA_SAMPLERATE,
                                               size_t max_slices = 16,
                                               double sensitivity = 0.5);

} // namespace volsa2

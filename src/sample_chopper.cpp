/**
 * @file sample_chopper.cpp
 * @brief Implementation of sample chopping, silence trimming, and beat slicing utilities.
 * @date 2026
 */

#include "volsa2/sample_chopper.hpp"

#include <cmath>
#include <numbers>
#include <algorithm>

namespace volsa2 {

std::vector<int16_t> crop_audio(std::span<const int16_t> samples,
                               size_t start_idx,
                               size_t end_idx,
                               bool apply_edge_fade) {
    if (samples.empty() || start_idx >= samples.size()) {
        return {};
    }

    end_idx = std::min(end_idx, samples.size());
    if (start_idx >= end_idx) {
        return {};
    }

    size_t count = end_idx - start_idx;
    std::vector<int16_t> output(samples.begin() + start_idx, samples.begin() + end_idx);

    if (apply_edge_fade && count > 64) {
        const size_t fade_len = std::min<size_t>(32, count / 4);
        const double pi = std::numbers::pi;
        for (size_t i = 0; i < fade_len; ++i) {
            // Half-cosine fade-in: 0.5 * (1 - cos(pi * i / fade_len))
            double factor_in = 0.5 * (1.0 - std::cos(pi * i / fade_len));
            output[i] = static_cast<int16_t>(std::round(output[i] * factor_in));

            // Half-cosine fade-out
            size_t out_idx = count - 1 - i;
            double factor_out = 0.5 * (1.0 - std::cos(pi * i / fade_len));
            output[out_idx] = static_cast<int16_t>(std::round(output[out_idx] * factor_out));
        }
    }

    return output;
}

std::pair<size_t, size_t> find_silence_bounds(std::span<const int16_t> samples,
                                             double threshold_db) {
    if (samples.empty()) {
        return {0, 0};
    }

    const double thresh_linear = 32768.0 * std::pow(10.0, threshold_db / 20.0);
    const double thresh_pwr = thresh_linear * thresh_linear;

    constexpr size_t block_size = 64;
    const size_t num_blocks = samples.size() / block_size;

    if (num_blocks == 0) {
        return {0, samples.size()};
    }

    // 1. Scan forward from beginning to detect audio attack
    size_t start_bound = 0;
    bool found_start = false;

    for (size_t b = 0; b < num_blocks; ++b) {
        double sum_sq = 0.0;
        size_t offset = b * block_size;
        for (size_t i = 0; i < block_size; ++i) {
            double s = static_cast<double>(samples[offset + i]);
            sum_sq += s * s;
        }
        double rms_pwr = sum_sq / block_size;
        if (rms_pwr >= thresh_pwr) {
            start_bound = offset;
            found_start = true;
            break;
        }
    }

    if (!found_start) {
        // Entire buffer is below threshold
        return {0, samples.size()};
    }

    // 2. Scan backward from end to detect audio release
    size_t end_bound = samples.size();

    for (size_t b = num_blocks; b > 0; --b) {
        double sum_sq = 0.0;
        size_t offset = (b - 1) * block_size;
        for (size_t i = 0; i < block_size; ++i) {
            double s = static_cast<double>(samples[offset + i]);
            sum_sq += s * s;
        }
        double rms_pwr = sum_sq / block_size;
        if (rms_pwr >= thresh_pwr) {
            end_bound = std::min(samples.size(), offset + block_size);
            break;
        }
    }

    if (start_bound >= end_bound) {
        return {0, samples.size()};
    }

    return {start_bound, end_bound};
}

std::vector<int16_t> auto_trim_silence(std::span<const int16_t> samples,
                                      double threshold_db) {
    auto [start_bound, end_bound] = find_silence_bounds(samples, threshold_db);
    return crop_audio(samples, start_bound, end_bound, true);
}

std::vector<SlicePoint> divide_equal_slices(size_t total_samples,
                                           size_t num_slices,
                                           uint32_t sample_rate) {
    if (total_samples == 0 || num_slices == 0) {
        return {};
    }

    size_t n = std::min(num_slices, total_samples);
    std::vector<SlicePoint> slices;
    slices.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        SlicePoint sp;
        sp.start_sample = (i * total_samples) / n;
        sp.end_sample = ((i + 1) * total_samples) / n;
        sp.start_seconds = static_cast<double>(sp.start_sample) / sample_rate;
        sp.end_seconds = static_cast<double>(sp.end_sample) / sample_rate;
        slices.push_back(sp);
    }

    return slices;
}

std::vector<SlicePoint> detect_transient_slices(std::span<const int16_t> samples,
                                               uint32_t sample_rate,
                                               size_t max_slices,
                                               double sensitivity) {
    if (samples.empty()) {
        return {};
    }

    max_slices = std::clamp<size_t>(max_slices, 2, 64);
    constexpr size_t block_size = 128;
    const size_t num_blocks = samples.size() / block_size;

    if (num_blocks < 4) {
        return divide_equal_slices(samples.size(), std::min<size_t>(2, max_slices), sample_rate);
    }

    // Compute short-time energy curve
    std::vector<double> energy(num_blocks, 0.0);
    double max_energy = 0.0;
    for (size_t b = 0; b < num_blocks; ++b) {
        double sum = 0.0;
        size_t offset = b * block_size;
        for (size_t i = 0; i < block_size; ++i) {
            double s = samples[offset + i] / 32768.0;
            sum += s * s;
        }
        energy[b] = sum / block_size;
        max_energy = std::max(max_energy, energy[b]);
    }

    if (max_energy < 1e-5) {
        return divide_equal_slices(samples.size(), std::min<size_t>(4, max_slices), sample_rate);
    }

    // Minimum distance between transient attacks: ~50ms
    const size_t min_block_dist = std::max<size_t>(4, (sample_rate / 20) / block_size);
    const double sens = std::clamp(sensitivity, 0.05, 0.95);
    const double threshold = max_energy * (1.0 - sens) * 0.25;

    std::vector<size_t> boundaries;
    boundaries.push_back(0); // First slice starts at sample 0

    size_t last_boundary_block = 0;
    for (size_t b = 1; b < num_blocks; ++b) {
        double diff = energy[b] - energy[b - 1];
        if (diff > threshold && (b - last_boundary_block) >= min_block_dist) {
            boundaries.push_back(b * block_size);
            last_boundary_block = b;
            if (boundaries.size() >= max_slices) break;
        }
    }

    boundaries.push_back(samples.size());

    std::vector<SlicePoint> slices;
    slices.reserve(boundaries.size() - 1);

    for (size_t i = 0; i + 1 < boundaries.size(); ++i) {
        SlicePoint sp;
        sp.start_sample = boundaries[i];
        sp.end_sample = boundaries[i + 1];
        sp.start_seconds = static_cast<double>(sp.start_sample) / sample_rate;
        sp.end_seconds = static_cast<double>(sp.end_sample) / sample_rate;
        slices.push_back(sp);
    }

    return slices;
}

} // namespace volsa2

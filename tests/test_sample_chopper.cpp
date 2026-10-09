/**
 * @file test_sample_chopper.cpp
 * @brief Unit tests for sample chopping, silence trimming, and beat slicing algorithms.
 * @author Volsa2 Project Team
 * @date 2026
 */

#include "volsa2/sample_chopper.hpp"

#include <iostream>
#include <cassert>
#include <cmath>
#include <numbers>
#include <vector>

using namespace volsa2;

void test_crop_audio() {
    std::cout << "[Test] crop_audio()..." << std::endl;

    std::vector<int16_t> buffer(4000);
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = static_cast<int16_t>(i % 30000);
    }

    // Standard crop
    auto cropped = crop_audio(buffer, 500, 1500, true);
    assert(cropped.size() == 1000);

    // Boundary edge fades (anti-click micro-fade)
    assert(std::abs(cropped[0]) <= std::abs(buffer[500]));
    assert(std::abs(cropped.back()) <= std::abs(buffer[1499]));

    // Clamping & invalid ranges
    auto empty1 = crop_audio(buffer, 2000, 1000);
    assert(empty1.empty());

    auto empty2 = crop_audio(buffer, 5000, 6000);
    assert(empty2.empty());

    auto clamped = crop_audio(buffer, 3500, 6000);
    assert(clamped.size() == 500);

    std::cout << "  ✓ Passed crop_audio tests." << std::endl;
}

void test_silence_bounds_and_auto_trim() {
    std::cout << "[Test] find_silence_bounds() & auto_trim_silence()..." << std::endl;

    // Buffer: 1000 samples silence (0) + 2000 samples tone + 1000 samples silence (0)
    std::vector<int16_t> buffer(4000, 0);
    for (size_t i = 1000; i < 3000; ++i) {
        buffer[i] = static_cast<int16_t>((i % 2 == 0) ? 20000 : -20000);
    }

    auto [start_bound, end_bound] = find_silence_bounds(buffer, -40.0);
    std::cout << "  Detected silence bounds: [" << start_bound << ", " << end_bound << "]" << std::endl;
    assert(start_bound >= 850 && start_bound <= 1050);
    assert(end_bound >= 2950 && end_bound <= 3150);

    auto trimmed = auto_trim_silence(buffer, -40.0);
    assert(!trimmed.empty());
    assert(trimmed.size() < buffer.size());
    assert(trimmed.size() >= 1900 && trimmed.size() <= 2200);

    // Completely silent buffer
    std::vector<int16_t> all_silent(2000, 0);
    auto [s_all, e_all] = find_silence_bounds(all_silent, -40.0);
    assert(s_all == 0 && e_all == all_silent.size());

    std::cout << "  ✓ Passed silence bounds & auto-trim tests." << std::endl;
}

void test_divide_equal_slices() {
    std::cout << "[Test] divide_equal_slices()..." << std::endl;

    const size_t total_samples = 10000;
    const size_t num_slices = 4;
    auto slices = divide_equal_slices(total_samples, num_slices, 31250);

    assert(slices.size() == 4);
    assert(slices[0].start_sample == 0);
    assert(slices[0].end_sample == 2500);
    assert(slices[1].start_sample == 2500);
    assert(slices[1].end_sample == 5000);
    assert(slices[2].start_sample == 5000);
    assert(slices[2].end_sample == 7500);
    assert(slices[3].start_sample == 7500);
    assert(slices[3].end_sample == 10000);

    // Check duration calculations
    assert(std::abs(slices[0].start_seconds - 0.0) < 1e-5);
    assert(std::abs(slices[3].end_seconds - (10000.0 / 31250.0)) < 1e-5);

    // Check non-divisible total sample count (e.g. 10003 samples into 4 slices)
    auto slices_prime = divide_equal_slices(10003, 4, 31250);
    assert(slices_prime.size() == 4);
    assert(slices_prime[0].start_sample == 0);
    assert(slices_prime.back().end_sample == 10003);
    for (size_t i = 0; i + 1 < slices_prime.size(); ++i) {
        assert(slices_prime[i].end_sample == slices_prime[i + 1].start_sample);
    }

    std::cout << "  ✓ Passed divide_equal_slices tests." << std::endl;
}

void test_detect_transient_slices() {
    std::cout << "[Test] detect_transient_slices()..." << std::endl;

    const uint32_t sample_rate = 31250;
    const size_t total_samples = sample_rate * 2; // 2 seconds
    std::vector<int16_t> audio(total_samples, 0);

    // Place 4 distinct drum transients at 0.0s, 0.5s, 1.0s, 1.5s
    const size_t hit_offsets[] = {
        0,
        sample_rate / 2,
        sample_rate,
        (sample_rate * 3) / 2
    };

    for (size_t offset : hit_offsets) {
        for (size_t i = 0; i < 500 && (offset + i) < total_samples; ++i) {
            double env = std::exp(-static_cast<double>(i) / 100.0);
            double val = std::sin(2.0 * std::numbers::pi * 100.0 * i / sample_rate) * env;
            audio[offset + i] = static_cast<int16_t>(val * 28000.0);
        }
    }

    auto slices = detect_transient_slices(audio, sample_rate, 4, 0.5);
    std::cout << "  Detected " << slices.size() << " transient slices:" << std::endl;
    for (size_t i = 0; i < slices.size(); ++i) {
        std::cout << "    Slice " << i << ": [" << slices[i].start_sample << " - " << slices[i].end_sample << "] ("
                  << slices[i].start_seconds << "s - " << slices[i].end_seconds << "s)" << std::endl;
    }

    assert(!slices.empty());
    assert(slices[0].start_sample == 0);
    assert(slices.back().end_sample == total_samples);

    std::cout << "  ✓ Passed detect_transient_slices tests." << std::endl;
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "Running Volsa 2 Sample Chopper Test Suite " << std::endl;
    std::cout << "==========================================" << std::endl;

    test_crop_audio();
    test_silence_bounds_and_auto_trim();
    test_divide_equal_slices();
    test_detect_transient_slices();

    std::cout << "==========================================" << std::endl;
    std::cout << "All Sample Chopper tests passed!          " << std::endl;
    std::cout << "==========================================" << std::endl;
    return 0;
}
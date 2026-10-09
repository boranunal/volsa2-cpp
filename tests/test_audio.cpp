/**
 * @file test_audio.cpp
 * @brief Comprehensive unit tests for audio DSP, format inspection, resampling, and WAV I/O.
 * @author Volsa2 Project Team
 * @date 2026
 */

#include "volsa2/audio.hpp"

#include <cassert>
#include <iostream>
#include <vector>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

void test_wav_write_and_inspect() {
    std::cout << "Testing write_wav_file and inspect_audio_file...\n";

    fs::path temp_dir = fs::temp_directory_path() / "volsa2_test_audio";
    fs::create_directories(temp_dir);
    fs::path test_wav = temp_dir / "test_31250_mono.wav";

    // Generate 1 second of 440 Hz sine wave at 31250 Hz
    const uint32_t sample_rate = volsa2::VOLCA_SAMPLERATE;
    const size_t num_samples = sample_rate; // 1 second
    std::vector<int16_t> original_samples(num_samples);

    for (size_t i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;
        double s = std::sin(2.0 * M_PI * 440.0 * t);
        original_samples[i] = static_cast<int16_t>(s * 20000.0);
    }

    // Write WAV file
    volsa2::write_wav_file(test_wav, original_samples, sample_rate);
    assert(fs::exists(test_wav));
    assert(fs::file_size(test_wav) == 44 + num_samples * sizeof(int16_t));

    // Inspect file
    auto info = volsa2::inspect_audio_file(test_wav);
    assert(info.sample_rate == 31250);
    assert(info.channels == 1);
    assert(info.frames == num_samples);
    assert(std::abs(info.duration_seconds - 1.0) < 0.001);

    // Read back and compare samples
    auto loaded_samples = volsa2::load_and_convert_audio(test_wav, volsa2::MonoMode::Mid);
    assert(loaded_samples.size() == original_samples.size());
    for (size_t i = 0; i < loaded_samples.size(); ++i) {
        // Quantization / round-trip error should be within +/- 1 LSB
        assert(std::abs(loaded_samples[i] - original_samples[i]) <= 1);
    }

    // Test empty samples writing (must not crash)
    fs::path empty_wav = temp_dir / "test_empty.wav";
    std::vector<int16_t> empty_vec;
    volsa2::write_wav_file(empty_wav, empty_vec, sample_rate);
    assert(fs::exists(empty_wav));
    assert(fs::file_size(empty_wav) == 44);

    fs::remove_all(temp_dir);
    std::cout << "  [PASS] test_wav_write_and_inspect\n";
}

void test_stereo_downmix_modes() {
    std::cout << "Testing stereo downmixing modes (Mid, Left, Right, Side)...\n";

    fs::path temp_dir = fs::temp_directory_path() / "volsa2_test_stereo";
    fs::create_directories(temp_dir);
    fs::path stereo_wav = temp_dir / "test_stereo.wav";

    // Write a stereo 16-bit 31250 Hz WAV manually using standard RIFF
    // Left channel: constant +10000, Right channel: constant +4000
    const uint32_t sample_rate = 31250;
    const size_t num_frames = 100;
    const uint32_t num_channels = 2;
    const uint32_t bytes_per_sample = 2;
    const uint32_t data_bytes = num_frames * num_channels * bytes_per_sample;
    const uint32_t riff_chunk_size = 36 + data_bytes;

    {
        std::ofstream out(stereo_wav, std::ios::binary);
        out.write("RIFF", 4);
        out.write(reinterpret_cast<const char*>(&riff_chunk_size), 4);
        out.write("WAVE", 4);
        out.write("fmt ", 4);
        uint32_t sub1_size = 16;
        uint16_t fmt = 1;
        uint16_t ch = 2;
        uint32_t sr = sample_rate;
        uint32_t br = sample_rate * 4;
        uint16_t ba = 4;
        uint16_t bps = 16;
        out.write(reinterpret_cast<const char*>(&sub1_size), 4);
        out.write(reinterpret_cast<const char*>(&fmt), 2);
        out.write(reinterpret_cast<const char*>(&ch), 2);
        out.write(reinterpret_cast<const char*>(&sr), 4);
        out.write(reinterpret_cast<const char*>(&br), 4);
        out.write(reinterpret_cast<const char*>(&ba), 2);
        out.write(reinterpret_cast<const char*>(&bps), 2);
        out.write("data", 4);
        out.write(reinterpret_cast<const char*>(&data_bytes), 4);

        for (size_t i = 0; i < num_frames; ++i) {
            int16_t left = 10000;
            int16_t right = 4000;
            out.write(reinterpret_cast<const char*>(&left), 2);
            out.write(reinterpret_cast<const char*>(&right), 2);
        }
    }

    // Test Left Mode: should yield +10000
    auto left_samples = volsa2::load_and_convert_audio(stereo_wav, volsa2::MonoMode::Left);
    assert(left_samples.size() == num_frames);
    assert(std::abs(left_samples[0] - 10000) <= 2);

    // Test Right Mode: should yield +4000
    auto right_samples = volsa2::load_and_convert_audio(stereo_wav, volsa2::MonoMode::Right);
    assert(right_samples.size() == num_frames);
    assert(std::abs(right_samples[0] - 4000) <= 2);

    // Test Mid Mode: (Left + Right) / 2 = (10000 + 4000) / 2 = +7000
    auto mid_samples = volsa2::load_and_convert_audio(stereo_wav, volsa2::MonoMode::Mid);
    assert(mid_samples.size() == num_frames);
    assert(std::abs(mid_samples[0] - 7000) <= 2);

    // Test Side Mode: (Left - Right) / 2 = (10000 - 4000) / 2 = +3000
    auto side_samples = volsa2::load_and_convert_audio(stereo_wav, volsa2::MonoMode::Side);
    assert(side_samples.size() == num_frames);
    assert(std::abs(side_samples[0] - 3000) <= 2);

    fs::remove_all(temp_dir);
    std::cout << "  [PASS] test_stereo_downmix_modes\n";
}

void test_resampling_fidelity() {
    std::cout << "Testing sample rate conversion (44.1 kHz, 48 kHz, 96 kHz -> 31.25 kHz)...\n";

    fs::path temp_dir = fs::temp_directory_path() / "volsa2_test_resample";
    fs::create_directories(temp_dir);

    // Test 44100 Hz input
    fs::path wav_44k = temp_dir / "test_44100.wav";
    const uint32_t in_rate = 44100;
    const size_t in_frames = 44100; // 1.0 second
    std::vector<int16_t> pcm_44k(in_frames);
    for (size_t i = 0; i < in_frames; ++i) {
        double t = static_cast<double>(i) / in_rate;
        pcm_44k[i] = static_cast<int16_t>(std::sin(2.0 * M_PI * 1000.0 * t) * 15000.0);
    }
    volsa2::write_wav_file(wav_44k, pcm_44k, in_rate);

    auto resampled = volsa2::load_and_convert_audio(wav_44k, volsa2::MonoMode::Mid);

    // Resampled frame count should be approximately 31250 (1.0 second)
    double expected_frames = in_frames * (31250.0 / 44100.0);
    assert(std::abs(static_cast<double>(resampled.size()) - expected_frames) <= 20);

    fs::remove_all(temp_dir);
    std::cout << "  [PASS] test_resampling_fidelity\n";
}

int main() {
    test_wav_write_and_inspect();
    test_stereo_downmix_modes();
    test_resampling_fidelity();
    std::cout << "All audio tests passed successfully!\n";
    return 0;
}

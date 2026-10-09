#include "volsa2/proto.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <stdexcept>

std::vector<uint8_t> read_binary_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open file: " + path.string());
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
}

std::vector<int16_t> read_wav_samples(const std::filesystem::path& path) {
    auto bytes = read_binary_file(path);
    if (bytes.size() < 44) {
        throw std::runtime_error("Invalid WAV file (too small): " + path.string());
    }

    // Locate "data" chunk
    size_t offset = 12;
    size_t data_offset = 0;
    size_t data_size = 0;

    while (offset + 8 <= bytes.size()) {
        std::string chunk_id(reinterpret_cast<const char*>(&bytes[offset]), 4);
        uint32_t chunk_size = *reinterpret_cast<const uint32_t*>(&bytes[offset + 4]);
        if (chunk_id == "data") {
            data_offset = offset + 8;
            data_size = chunk_size;
            break;
        }
        offset += 8 + chunk_size;
    }

    if (data_offset == 0 || data_offset + data_size > bytes.size()) {
        throw std::runtime_error("Could not find data chunk in WAV: " + path.string());
    }

    std::vector<int16_t> samples(data_size / 2);
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<int16_t>(bytes[data_offset + i * 2] |
                                         (bytes[data_offset + i * 2 + 1] << 8));
    }
    return samples;
}

int main() {
    std::cout << "Running test_proto with test_data/ dumps 1..14...\n";

    for (int idx = 1; idx <= 14; ++idx) {
        std::string wav_path = "../../volsa2/test_data/sample" + std::to_string(idx) + ".wav.raw";
        std::string dump_path = "../../volsa2/test_data/sample_data_dump" + std::to_string(idx) + ".raw";

        if (!std::filesystem::exists(wav_path) || !std::filesystem::exists(dump_path)) {
            // Also try relative to project root
            wav_path = "volsa2/test_data/sample" + std::to_string(idx) + ".wav.raw";
            dump_path = "volsa2/test_data/sample_data_dump" + std::to_string(idx) + ".raw";
        }

        auto expected_samples = read_wav_samples(wav_path);
        auto dump_raw = read_binary_file(dump_path);

        auto sample_data = volsa2::SampleData::parse(dump_raw);
        assert(sample_data.data == expected_samples);

        // Verify round-trip encoding
        auto re_encoded = sample_data.encode(0); // dumps are channel 0
        assert(re_encoded == dump_raw);

        std::cout << "  [PASS] Sample " << idx << " matches expected WAV and round-trips exactly ("
                  << sample_data.data.size() << " samples)\n";
    }

    std::cout << "All proto tests passed!\n";
    return 0;
}

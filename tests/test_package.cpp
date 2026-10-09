#include "volsa2/package.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>

void test_md5() {
    std::string text = "hello";
    std::span<const uint8_t> span(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    std::string hash = volsa2::compute_md5_hex(span);
    assert(hash == "5d41402abc4b2a76b9719d911017c592");
    std::cout << "[PASS] test_md5" << std::endl;
}

void test_package_save_load() {
    std::filesystem::path tmp_path = std::filesystem::temp_directory_path() / "test_volsa_package.ivlcsplpreset";
    if (std::filesystem::exists(tmp_path)) {
        std::filesystem::remove(tmp_path);
    }

    auto pkg = volsa2::PackageData::create_default("Test Pack", "Sound Designer");
    assert(pkg.programs.size() == 16);
    assert(pkg.samples.size() == 200);

    pkg.programs[0].pattern.name = "Funky Beat";
    pkg.programs[0].comment = "First groove";
    pkg.programs[1].pattern.name = "Electro Bass";

    // Add audio to sample 0
    std::vector<int16_t> kick_pcm(1000);
    for (size_t i = 0; i < kick_pcm.size(); ++i) {
        kick_pcm[i] = static_cast<int16_t>(i * 30 - 15000);
    }
    pkg.samples[0].header.name = "Sub Kick";
    pkg.samples[0].header.length = static_cast<uint32_t>(kick_pcm.size());
    pkg.samples[0].header.level = 65535;
    pkg.samples[0].header.speed = 16384;
    pkg.samples[0].author = "Producer A";
    pkg.samples[0].comment = "Punchy low end";

    volsa2::SampleData sdata0;
    sdata0.sample_no = 0;
    sdata0.data = kick_pcm;
    pkg.samples[0].data = std::move(sdata0);

    // Save package
    volsa2::save_package(tmp_path, pkg);
    assert(std::filesystem::exists(tmp_path));
    assert(std::filesystem::file_size(tmp_path) > 0);

    // Load package back
    auto loaded = volsa2::load_package(tmp_path);
    assert(loaded.info.name == "Test Pack");
    assert(loaded.info.author == "Sound Designer");
    assert(loaded.programs.size() == 16);
    assert(loaded.samples.size() == 200);

    assert(loaded.programs[0].pattern.name == "Funky Beat");
    assert(loaded.programs[0].comment == "First groove");
    assert(loaded.programs[0].pattern.raw_data.size() == volsa2::PatternData::RAW_PATTERN_SIZE);
    assert(loaded.programs[1].pattern.name == "Electro Bass");

    assert(loaded.samples[0].header.name == "Sub Kick");
    assert(loaded.samples[0].header.length == 1000);
    assert(loaded.samples[0].author == "Producer A");
    assert(loaded.samples[0].comment == "Punchy low end");
    assert(loaded.samples[0].data.has_value());
    assert(loaded.samples[0].data->data.size() == 1000);
    assert(loaded.samples[0].data->data[10] == kick_pcm[10]);

    // Empty slot
    assert(!loaded.samples[2].data.has_value() || loaded.samples[2].data->data.empty());
    assert(loaded.samples[2].header.length == 0);

    std::filesystem::remove(tmp_path);
    std::cout << "[PASS] test_package_save_load" << std::endl;
}

void test_load_official_preset() {
    std::filesystem::path factory_path = "/home/boran/.wine/drive_c/users/boran/AppData/Roaming/KORG/KORG volca sample Sound Librarian/Preset Data/Factory Presets.ivlcsplpreset";
    if (!std::filesystem::exists(factory_path)) {
        std::cout << "[SKIP] Factory Presets.ivlcsplpreset not found, skipping official load test." << std::endl;
        return;
    }

    auto pkg = volsa2::load_package(factory_path);
    assert(pkg.info.name == "Factory");
    assert(pkg.programs.size() == 16);
    assert(pkg.samples.size() == 200);

    // Program 0 in factory presets is "Chill Chop"
    assert(pkg.programs[0].pattern.name == "Chill Chop");
    assert(pkg.programs[0].pattern.raw_data.size() == volsa2::PatternData::RAW_PATTERN_SIZE);

    // Sample 0 in factory presets is "Kick Thick" (length 7210)
    assert(pkg.samples[0].header.name == "Kick Thick");
    assert(pkg.samples[0].header.length == 7210);
    assert(pkg.samples[0].data.has_value());
    assert(pkg.samples[0].data->data.size() == 7210);

    std::cout << "[PASS] test_load_official_preset" << std::endl;
}

int main() {
    test_md5();
    test_package_save_load();
    test_load_official_preset();
    std::cout << "All package tests passed!" << std::endl;
    return 0;
}

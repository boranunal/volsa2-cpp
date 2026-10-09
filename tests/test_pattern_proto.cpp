#include "volsa2/proto.hpp"
#include <cassert>
#include <iostream>
#include <vector>

void test_pattern_dump_request() {
    volsa2::PatternDumpRequest req;
    req.channel = 3;
    req.pattern_no = 7;

    auto encoded = req.encode();
    assert(encoded.size() == 9);
    assert(encoded[0] == 0xF0);
    assert(encoded[1] == 0x42);
    assert(encoded[2] == (0x30 | 3));
    assert(encoded[3] == 0x00);
    assert(encoded[4] == 0x01);
    assert(encoded[5] == 0x2D);
    assert(encoded[6] == 0x1D);
    assert(encoded[7] == 7);
    assert(encoded[8] == 0xF7);

    std::cout << "[PASS] test_pattern_dump_request" << std::endl;
}

void test_pattern_data_roundtrip() {
    std::string test_name = "Synth Wave 80s";
    auto pattern = volsa2::PatternData::create(5, test_name);

    assert(pattern.pattern_no == 5);
    assert(pattern.name == test_name);
    assert(pattern.raw_data.size() == volsa2::PatternData::RAW_PATTERN_SIZE);
    assert(pattern.raw_data[0] == 'P');
    assert(pattern.raw_data[1] == 'T');
    assert(pattern.raw_data[2] == 'S');
    assert(pattern.raw_data[3] == 'T');

    // Fill some test pattern bytes across the 7936 byte payload
    for (size_t i = 100; i < 5000; ++i) {
        pattern.raw_data[i] = static_cast<uint8_t>((i * 37 + 13) & 0xFF);
    }

    uint8_t channel = 0;
    auto sysex = pattern.encode(channel);
    assert(sysex.size() == volsa2::PatternData::SYSEX_MESSAGE_SIZE);
    assert(sysex[0] == 0xF0);
    assert(sysex[1] == 0x42);
    assert(sysex[2] == 0x30);
    assert(sysex[3] == 0x00);
    assert(sysex[4] == 0x01);
    assert(sysex[5] == 0x2D);
    assert(sysex[6] == 0x4D);
    assert(sysex[7] == 5);
    assert(sysex.back() == 0xF7);

    auto decoded = volsa2::PatternData::parse(sysex);
    assert(decoded.pattern_no == 5);
    assert(decoded.name == test_name);
    assert(decoded.raw_data.size() == volsa2::PatternData::RAW_PATTERN_SIZE);
    assert(decoded.raw_data == pattern.raw_data);

    std::cout << "[PASS] test_pattern_data_roundtrip" << std::endl;
}

void test_pattern_data_errors() {
    std::vector<uint8_t> short_msg(100, 0);
    bool caught = false;
    try {
        volsa2::PatternData::parse(short_msg);
    } catch (const std::exception&) {
        caught = true;
    }
    assert(caught);

    auto pattern = volsa2::PatternData::create(2, "Test");
    auto sysex = pattern.encode(0);

    // Wrong function ID
    sysex[6] = 0x4E;
    caught = false;
    try {
        volsa2::PatternData::parse(sysex);
    } catch (const std::exception&) {
        caught = true;
    }
    assert(caught);

    std::cout << "[PASS] test_pattern_data_errors" << std::endl;
}

int main() {
    test_pattern_dump_request();
    test_pattern_data_roundtrip();
    test_pattern_data_errors();
    std::cout << "All pattern proto tests passed!" << std::endl;
    return 0;
}

/**
 * @file test_proto_comprehensive.cpp
 * @brief Exhaustive validation of protocol encoders, decoders, and error handling.
 * @author Volsa2 Project Team
 * @date 2026
 */

#include "volsa2/proto.hpp"

#include <cassert>
#include <iostream>
#include <vector>
#include <string>

void test_read_write_u8_exhaustive() {
    std::cout << "Testing read_u8 and write_u8 across full 8-bit domain (0-255)...\n";

    for (int i = 0; i < 256; ++i) {
        uint8_t original = static_cast<uint8_t>(i);
        std::vector<uint8_t> buf;
        volsa2::write_u8(buf, original);

        assert(buf.size() == 2);
        assert((buf[0] & 0x80) == 0); // LSB must be valid 7-bit
        assert((buf[1] & 0xFE) == 0); // MSB must be 0 or 1

        auto [recovered, bytes_read] = volsa2::read_u8(buf);
        assert(bytes_read == 2);
        assert(recovered == original);
    }
    std::cout << "  [PASS] test_read_write_u8_exhaustive\n";
}

void test_search_device() {
    std::cout << "Testing SearchDeviceRequest and SearchDeviceReply...\n";

    volsa2::SearchDeviceRequest req{volsa2::U7(64)};
    auto encoded = req.encode();
    assert(encoded.size() == 6);
    assert(encoded[0] == 0xF0 && encoded[1] == 0x42);
    assert(encoded[2] == 0x50 && encoded[3] == 0x00);
    assert(encoded[4] == 64);
    assert(encoded[5] == 0xF7);

    // Valid reply: channel=2, echo=64, model ID, firmware v1.02
    std::vector<uint8_t> valid_reply = {
        0xF0, 0x42, 0x50, 0x01,
        0x02, 0x40, // channel=2, echo=64
        0x2D, 0x01, 0x08, 0x00, // model ID
        0x02, 0x00, // minor = 2
        0x01, 0x00, // major = 1
        0xF7
    };
    auto parsed = volsa2::SearchDeviceReply::parse(valid_reply);
    assert(parsed.device_id.as_u8() == 2);
    assert(parsed.echo.as_u8() == 64);
    assert(parsed.version.major == 1 && parsed.version.minor == 2);
    assert(parsed.version.to_string() == "1.2");

    // Negative tests: truncated packet
    try {
        std::vector<uint8_t> short_reply(valid_reply.begin(), valid_reply.begin() + 10);
        volsa2::SearchDeviceReply::parse(short_reply);
        assert(false && "Should have thrown for short packet");
    } catch (const std::runtime_error&) {}

    // Negative tests: invalid model ID
    try {
        auto bad_model = valid_reply;
        bad_model[6] = 0xFF;
        volsa2::SearchDeviceReply::parse(bad_model);
        assert(false && "Should have thrown for wrong model ID");
    } catch (const std::runtime_error&) {}

    std::cout << "  [PASS] test_search_device\n";
}

void test_status_messages() {
    std::cout << "Testing StatusMessage (ACK and NAK variations)...\n";

    // Valid ACK on channel 3
    std::vector<uint8_t> ack_msg = {0xF0, 0x42, 0x33, 0x00, 0x01, 0x2D, 0x23, 0xF7};
    auto s_ack = volsa2::StatusMessage::parse(ack_msg);
    assert(s_ack.is_ack);

    // Busy
    std::vector<uint8_t> busy_msg = {0xF0, 0x42, 0x33, 0x00, 0x01, 0x2D, 0x24, 0xF7};
    auto s_busy = volsa2::StatusMessage::parse(busy_msg);
    assert(!s_busy.is_ack && s_busy.nak == volsa2::NakStatus::Busy);

    // SampleFull
    std::vector<uint8_t> full_msg = {0xF0, 0x42, 0x33, 0x00, 0x01, 0x2D, 0x25, 0xF7};
    auto s_full = volsa2::StatusMessage::parse(full_msg);
    assert(!s_full.is_ack && s_full.nak == volsa2::NakStatus::SampleFull);

    // DataFormat
    std::vector<uint8_t> data_msg = {0xF0, 0x42, 0x33, 0x00, 0x01, 0x2D, 0x26, 0xF7};
    auto s_data = volsa2::StatusMessage::parse(data_msg);
    assert(!s_data.is_ack && s_data.nak == volsa2::NakStatus::DataFormat);

    // Unknown status code
    try {
        std::vector<uint8_t> bad_status = {0xF0, 0x42, 0x33, 0x00, 0x01, 0x2D, 0x99, 0xF7};
        volsa2::StatusMessage::parse(bad_status);
        assert(false && "Should have thrown for unknown status code");
    } catch (const std::runtime_error&) {}

    // Invalid length
    try {
        std::vector<uint8_t> short_status = {0xF0, 0x42, 0x33, 0x23, 0xF7};
        volsa2::StatusMessage::parse(short_status);
        assert(false && "Should have thrown for invalid status length");
    } catch (const std::runtime_error&) {}

    std::cout << "  [PASS] test_status_messages\n";
}

void test_sample_space_dump() {
    std::cout << "Testing SampleSpaceDumpRequest and SampleSpaceDump...\n";

    volsa2::SampleSpaceDumpRequest req{5}; // channel 5
    auto encoded = req.encode();
    assert(encoded.size() == 8);
    assert(encoded[2] == (0x30 | 5));
    assert(encoded[6] == 0x1B);
    assert(encoded[7] == 0xF7);

    // Simulate response: used = 300 sectors, all = 1000 sectors
    // 300 = 0x012C -> lsb = 0x2C (44), msb = 0x02
    // 1000 = 0x03E8 -> lsb = 0x68 (104), msb = 0x07
    uint8_t used_lsb = 300 & 0x7F;
    uint8_t used_msb = (300 >> 7) & 0x7F;
    uint8_t all_lsb = 1000 & 0x7F;
    uint8_t all_msb = (1000 >> 7) & 0x7F;

    std::vector<uint8_t> dump_msg = {
        0xF0, 0x42, 0x35, 0x00, 0x01, 0x2D, 0x4B,
        used_lsb, used_msb, all_lsb, all_msb,
        0xF7
    };
    auto dump = volsa2::SampleSpaceDump::parse(dump_msg);
    assert(dump.used_sector_size == 300);
    assert(dump.all_sector_size == 1000);
    assert(std::abs(dump.occupied() - 0.3) < 0.0001);

    std::cout << "  [PASS] test_sample_space_dump\n";
}

void test_sample_header_round_trip() {
    std::cout << "Testing SampleHeader encode and parse round-trip...\n";

    // 1. Normal header
    volsa2::SampleHeader h1;
    h1.sample_no = 42;
    h1.name = "MY_KICK_DRUM";
    h1.length = 12500;
    h1.level = 60000;
    h1.speed = 16384;

    auto encoded1 = h1.encode(0);
    assert(encoded1.size() == 47);
    auto parsed1 = volsa2::SampleHeader::parse(encoded1);

    assert(parsed1.sample_no == 42);
    assert(parsed1.name == "MY_KICK_DRUM");
    assert(parsed1.length == 12500);
    assert(parsed1.level == 60000);
    assert(parsed1.speed == 16384);
    assert(!parsed1.is_empty());

    // 2. Empty header
    auto empty_h = volsa2::SampleHeader::empty(199);
    assert(empty_h.is_empty());
    auto encoded_empty = empty_h.encode(1);
    assert(encoded_empty.size() == 47);
    auto parsed_empty = volsa2::SampleHeader::parse(encoded_empty);
    assert(parsed_empty.sample_no == 199);
    assert(parsed_empty.is_empty());

    // 3. Exact 24-character name
    volsa2::SampleHeader h_max_name;
    h_max_name.sample_no = 0;
    h_max_name.name = "123456789012345678901234"; // 24 chars
    h_max_name.length = 500;
    auto enc_max = h_max_name.encode(0);
    auto dec_max = volsa2::SampleHeader::parse(enc_max);
    assert(dec_max.name == "123456789012345678901234");

    std::cout << "  [PASS] test_sample_header_round_trip\n";
}

void test_sample_data_round_trip() {
    std::cout << "Testing SampleData encode and parse round-trip with edge-case sample values...\n";

    volsa2::SampleData sd;
    sd.sample_no = 15;
    sd.data = {
        0, 1, -1, 32767, -32768, 16384, -16384,
        100, -100, 255, -255, 32766, -32767
    };

    auto encoded = sd.encode(0);
    auto parsed = volsa2::SampleData::parse(encoded);

    assert(parsed.sample_no == 15);
    assert(parsed.data.size() == sd.data.size());
    assert(parsed.data == sd.data);

    // Test empty audio data
    volsa2::SampleData empty_sd;
    empty_sd.sample_no = 0;
    auto enc_empty = empty_sd.encode(0);
    auto dec_empty = volsa2::SampleData::parse(enc_empty);
    assert(dec_empty.sample_no == 0);
    assert(dec_empty.data.empty());

    std::cout << "  [PASS] test_sample_data_round_trip\n";
}

int main() {
    test_read_write_u8_exhaustive();
    test_search_device();
    test_status_messages();
    test_sample_space_dump();
    test_sample_header_round_trip();
    test_sample_data_round_trip();
    std::cout << "All comprehensive protocol tests passed successfully!\n";
    return 0;
}

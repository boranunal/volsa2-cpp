/**
 * @file proto.cpp
 * @brief Implementation of KORG SysEx protocol serialization and parsing.
 * @details Implements binary decoding and encoding for SearchDeviceReply, StatusMessage,
 *          SampleSpaceDump, SampleHeader, and SampleData.
 * @author Volsa2 Project Team
 * @date 2026
 */

#include "volsa2/proto.hpp"
#include <algorithm>
#include <stdexcept>
#include <sstream>

namespace volsa2 {

/**
 * @brief Parses an incoming SearchDeviceReply message from raw bytes.
 * @details Validates:
 *          1. Exact length of 15 bytes.
 *          2. Initial KorgSysEx header signature [0xF0, 0x42].
 *          3. Function ID [0x50, 0x01].
 *          4. Termination by EOX (0xF7) at byte 14.
 *          5. Hardware Model ID matching VOLCA_SAMPLE_2_ID [0x2D, 0x01, 0x08, 0x00].
 *          Extracts global channel, echo verification byte, and firmware version.
 * @param slice Span of received SysEx bytes.
 * @return Parsed SearchDeviceReply structure.
 * @throws std::runtime_error if validation fails.
 */
SearchDeviceReply SearchDeviceReply::parse(std::span<const uint8_t> slice) {
    if (slice.size() != 15) {
        throw std::runtime_error("SearchDeviceReply: invalid message length (expected 15, got " + std::to_string(slice.size()) + ")");
    }
    if (!KorgSysEx::matches(slice.subspan(0, KorgSysEx::LEN))) {
        throw std::runtime_error("SearchDeviceReply: invalid KorgSysEx header");
    }
    if (slice[2] != 0x50 || slice[3] != 0x01) {
        throw std::runtime_error("SearchDeviceReply: invalid function ID");
    }
    if (slice[14] != EOX) {
        throw std::runtime_error("SearchDeviceReply: missing EOX");
    }

    if (std::memcmp(&slice[6], VOLCA_SAMPLE_2_ID.data(), 4) != 0) {
        throw std::runtime_error("SearchDeviceReply: model ID does not match Volca Sample 2");
    }

    auto dev_id = U7::checked(slice[4]);
    auto echo_val = U7::checked(slice[5]);
    if (!dev_id || !echo_val) {
        throw std::runtime_error("SearchDeviceReply: invalid 7-bit field");
    }

    uint16_t minor = static_cast<uint16_t>(slice[10] | (static_cast<uint16_t>(slice[11]) << 8));
    uint16_t major = static_cast<uint16_t>(slice[12] | (static_cast<uint16_t>(slice[13]) << 8));

    SearchDeviceReply reply;
    reply.device_id = *dev_id;
    reply.echo = *echo_val;
    reply.version = Version{major, minor};
    return reply;
}

/**
 * @brief Parses an incoming StatusMessage packet.
 * @details Validates:
 *          1. Exact length of 8 bytes.
 *          2. ExtendedKorgSysEx header.
 *          3. EOX terminator at index 7.
 *          4. Status code at index 6: ACK (0x23), Busy (0x24), SampleFull (0x25), DataFormat (0x26).
 * @param slice Span of received SysEx bytes.
 * @return Parsed StatusMessage.
 * @throws std::runtime_error if the header is invalid or the status code is unrecognized.
 */
StatusMessage StatusMessage::parse(std::span<const uint8_t> slice) {
    if (slice.size() != 8) {
        throw std::runtime_error("StatusMessage: invalid message length (expected 8, got " + std::to_string(slice.size()) + ")");
    }
    auto header = ExtendedKorgSysEx::parse(slice.subspan(0, ExtendedKorgSysEx::LEN));
    if (!header) {
        throw std::runtime_error("StatusMessage: invalid ExtendedKorgSysEx header");
    }
    if (slice[7] != EOX) {
        throw std::runtime_error("StatusMessage: missing EOX");
    }

    uint8_t status_byte = slice[6];
    StatusMessage status;
    if (status_byte == ACK_STATUS) {
        status.is_ack = true;
    } else if (status_byte == static_cast<uint8_t>(NakStatus::Busy)) {
        status.is_ack = false;
        status.nak = NakStatus::Busy;
    } else if (status_byte == static_cast<uint8_t>(NakStatus::SampleFull)) {
        status.is_ack = false;
        status.nak = NakStatus::SampleFull;
    } else if (status_byte == static_cast<uint8_t>(NakStatus::DataFormat)) {
        status.is_ack = false;
        status.nak = NakStatus::DataFormat;
    } else {
        throw std::runtime_error("StatusMessage: unknown status code 0x" + std::to_string(status_byte));
    }
    return status;
}

/**
 * @brief Parses a SampleSpaceDump message.
 * @details Format consists of ExtendedKorgSysEx header, Function ID 0x4B, 4 bytes of sector
 *          capacity data (used_lsb, used_msb, all_lsb, all_msb), and EOX.
 *          Sector integers are assembled via: `val = lsb | (msb << 7)`.
 * @param slice Span of received SysEx bytes.
 * @return Parsed SampleSpaceDump.
 * @throws std::runtime_error on malformed packet.
 */
SampleSpaceDump SampleSpaceDump::parse(std::span<const uint8_t> slice) {
    if (slice.size() != 12) {
        throw std::runtime_error("SampleSpaceDump: invalid message length (expected 12, got " + std::to_string(slice.size()) + ")");
    }
    auto header = ExtendedKorgSysEx::parse(slice.subspan(0, ExtendedKorgSysEx::LEN));
    if (!header) {
        throw std::runtime_error("SampleSpaceDump: invalid ExtendedKorgSysEx header");
    }
    if (slice[6] != 0x4B) {
        throw std::runtime_error("SampleSpaceDump: invalid function ID");
    }
    if (slice[11] != EOX) {
        throw std::runtime_error("SampleSpaceDump: missing EOX");
    }

    uint8_t used_lsb = slice[7];
    uint8_t used_msb = slice[8];
    uint8_t all_lsb = slice[9];
    uint8_t all_msb = slice[10];

    SampleSpaceDump dump;
    dump.used_sector_size = static_cast<uint16_t>(used_lsb | (static_cast<uint16_t>(used_msb) << 7));
    dump.all_sector_size = static_cast<uint16_t>(all_lsb | (static_cast<uint16_t>(all_msb) << 7));
    return dump;
}

/**
 * @brief Encodes a SampleHeader into a complete SysEx message.
 * @details Assembles:
 *          1. 6-byte ExtendedKorgSysEx header with target channel.
 *          2. 1-byte Function ID (0x4E).
 *          3. 2-byte slot index via write_u8(sample_no).
 *          4. 32-byte binary payload:
 *             - bytes 0-23: name (padded with 0x00)
 *             - bytes 24-27: length (uint32 LE)
 *             - bytes 28-29: level (uint16 LE)
 *             - bytes 30-31: speed (uint16 LE)
 *          5. Encodes the 32 bytes into 37 7-bit bytes via U8ToU7::convert.
 *          6. Appends EOX (0xF7).
 *          Total packet length: 6 + 1 + 2 + 37 + 1 = 47 bytes.
 * @param channel Global MIDI channel.
 * @return Serialized 47-byte SysEx message.
 */
std::vector<uint8_t> SampleHeader::encode(uint8_t channel) const {
    ExtendedKorgSysEx header{channel};
    std::vector<uint8_t> msg = header.encode();
    msg.push_back(0x4E);
    write_u8(msg, sample_no);

    std::vector<uint8_t> raw_data(32, 0);
    size_t copy_name_len = std::min<size_t>(name.size(), NAME_LEN);
    std::memcpy(raw_data.data(), name.data(), copy_name_len);

    raw_data[24] = static_cast<uint8_t>(length & 0xFF);
    raw_data[25] = static_cast<uint8_t>((length >> 8) & 0xFF);
    raw_data[26] = static_cast<uint8_t>((length >> 16) & 0xFF);
    raw_data[27] = static_cast<uint8_t>((length >> 24) & 0xFF);

    raw_data[28] = static_cast<uint8_t>(level & 0xFF);
    raw_data[29] = static_cast<uint8_t>((level >> 8) & 0xFF);

    raw_data[30] = static_cast<uint8_t>(speed & 0xFF);
    raw_data[31] = static_cast<uint8_t>((speed >> 8) & 0xFF);

    auto u7_data = U8ToU7::convert(raw_data);
    msg.insert(msg.end(), u7_data.begin(), u7_data.end());
    msg.push_back(EOX);

    return msg;
}

/**
 * @brief Parses an incoming SampleHeader SysEx message.
 * @details Decodes the 2-byte slot number, converts the remaining 7-bit payload to 8-bit bytes
 *          via U7ToU8::convert, extracts the 24-byte name (trimming trailing zeros), 32-bit length,
 *          16-bit level, and 16-bit speed.
 * @param slice Span of received SysEx bytes.
 * @return Parsed SampleHeader.
 * @throws std::runtime_error if format validation or decoding fails.
 */
SampleHeader SampleHeader::parse(std::span<const uint8_t> slice) {
    if (slice.size() < 47) {
        throw std::runtime_error("SampleHeader: message too short (expected at least 47, got " + std::to_string(slice.size()) + ")");
    }
    auto header = ExtendedKorgSysEx::parse(slice.subspan(0, ExtendedKorgSysEx::LEN));
    if (!header) {
        throw std::runtime_error("SampleHeader: invalid ExtendedKorgSysEx header");
    }
    if (slice[6] != 0x4E) {
        throw std::runtime_error("SampleHeader: invalid function ID");
    }
    if (slice.back() != EOX) {
        throw std::runtime_error("SampleHeader: missing EOX");
    }

    auto payload = slice.subspan(7, slice.size() - 8);
    auto [sample_no, read_bytes] = read_u8(payload);
    auto u7_data = payload.subspan(read_bytes);

    auto decoded = U7ToU8::convert(u7_data);
    if (decoded.size() < 32) {
        throw std::runtime_error("SampleHeader: decoded data too short (" + std::to_string(decoded.size()) + " < 32)");
    }

    SampleHeader sh;
    sh.sample_no = sample_no;

    size_t name_len = NAME_LEN;
    while (name_len > 0 && decoded[name_len - 1] == 0) {
        name_len--;
    }
    sh.name = std::string(reinterpret_cast<const char*>(decoded.data()), name_len);

    sh.length = static_cast<uint32_t>(decoded[24]) |
                (static_cast<uint32_t>(decoded[25]) << 8) |
                (static_cast<uint32_t>(decoded[26]) << 16) |
                (static_cast<uint32_t>(decoded[27]) << 24);

    sh.level = static_cast<uint16_t>(decoded[28] | (static_cast<uint16_t>(decoded[29]) << 8));
    sh.speed = static_cast<uint16_t>(decoded[30] | (static_cast<uint16_t>(decoded[31]) << 8));

    return sh;
}

/**
 * @brief Encodes SampleData audio samples into a complete SysEx packet.
 * @details Converts 16-bit signed PCM samples into little-endian bytes, packs them into
 *          7-bit format via U8ToU7::convert, and frames them with the ExtendedKorgSysEx header,
 *          Function ID (0x4F), 2-byte slot number, and EOX.
 * @param channel Global MIDI channel.
 * @return Complete serialized SysEx audio packet.
 */
std::vector<uint8_t> SampleData::encode(uint8_t channel) const {
    ExtendedKorgSysEx header{channel};
    std::vector<uint8_t> msg = header.encode();
    msg.push_back(0x4F);
    write_u8(msg, sample_no);

    std::vector<uint8_t> pcm_bytes;
    pcm_bytes.reserve(data.size() * 2);
    for (int16_t sample : data) {
        uint16_t u = static_cast<uint16_t>(sample);
        pcm_bytes.push_back(static_cast<uint8_t>(u & 0xFF));
        pcm_bytes.push_back(static_cast<uint8_t>(u >> 8));
    }

    auto u7_data = U8ToU7::convert(pcm_bytes);
    msg.insert(msg.end(), u7_data.begin(), u7_data.end());
    msg.push_back(EOX);

    return msg;
}

/**
 * @brief Parses an incoming SampleData audio dump SysEx message.
 * @details Reads the 2-byte slot number, passes the remaining 7-bit payload through U7ToU8::convert,
 *          and reconstructs 16-bit signed integers in little-endian order: `s = b[0] | (b[1] << 8)`.
 * @param slice Span of received SysEx bytes.
 * @return Parsed SampleData containing the audio PCM buffer.
 * @throws std::runtime_error on message corruption.
 */
SampleData SampleData::parse(std::span<const uint8_t> slice) {
    if (slice.size() < ExtendedKorgSysEx::LEN + 1 + 2 + 1) {
        throw std::runtime_error("SampleData: message too short");
    }
    auto header = ExtendedKorgSysEx::parse(slice.subspan(0, ExtendedKorgSysEx::LEN));
    if (!header) {
        throw std::runtime_error("SampleData: invalid ExtendedKorgSysEx header");
    }
    if (slice[6] != 0x4F) {
        throw std::runtime_error("SampleData: invalid function ID");
    }
    if (slice.back() != EOX) {
        throw std::runtime_error("SampleData: missing EOX");
    }

    auto payload = slice.subspan(7, slice.size() - 8);
    auto [sample_no, read_bytes] = read_u8(payload);
    auto u7_data = payload.subspan(read_bytes);

    auto decoded = U7ToU8::convert(u7_data);

    SampleData sd;
    sd.sample_no = sample_no;
    sd.data.reserve(decoded.size() / 2);

    for (size_t i = 0; i + 1 < decoded.size(); i += 2) {
        uint16_t raw_u16 = static_cast<uint16_t>(decoded[i]) |
                           (static_cast<uint16_t>(decoded[i + 1]) << 8);
        sd.data.push_back(static_cast<int16_t>(raw_u16));
    }

    return sd;
}

/**
 * @brief Factory creating a PatternData object, initializing name and PTST magic if needed.
 */
PatternData PatternData::create(uint8_t pattern_no, const std::string& name, std::vector<uint8_t> raw) {
    PatternData pd;
    pd.pattern_no = pattern_no & 0x0F;
    if (raw.size() == RAW_PATTERN_SIZE) {
        pd.raw_data = std::move(raw);
    } else {
        pd.raw_data.assign(RAW_PATTERN_SIZE, 0);
        pd.raw_data[0] = 'P';
        pd.raw_data[1] = 'T';
        pd.raw_data[2] = 'S';
        pd.raw_data[3] = 'T';
    }

    if (!name.empty()) {
        pd.name = name.substr(0, std::min<size_t>(name.size(), NAME_MAX_LEN));
        std::memset(pd.raw_data.data() + NAME_OFFSET, 0, NAME_MAX_LEN);
        std::memcpy(pd.raw_data.data() + NAME_OFFSET, pd.name.data(), pd.name.size());
    } else {
        size_t nlen = 0;
        while (nlen < NAME_MAX_LEN && pd.raw_data[NAME_OFFSET + nlen] != '\0') {
            nlen++;
        }
        pd.name = std::string(reinterpret_cast<const char*>(pd.raw_data.data() + NAME_OFFSET), nlen);
    }

    return pd;
}

/**
 * @brief Serializes the pattern payload into a 9,079-byte SysEx message.
 */
std::vector<uint8_t> PatternData::encode(uint8_t channel) const {
    if (raw_data.size() != RAW_PATTERN_SIZE) {
        throw std::runtime_error("PatternData::encode: invalid raw_data size (expected " +
                                 std::to_string(RAW_PATTERN_SIZE) + ", got " + std::to_string(raw_data.size()) + ")");
    }
    ExtendedKorgSysEx header{channel};
    std::vector<uint8_t> msg = header.encode();
    msg.push_back(0x4D);
    msg.push_back(pattern_no & 0x0F);

    auto u7 = U8ToU7::convert(raw_data);
    msg.insert(msg.end(), u7.begin(), u7.end());
    msg.push_back(EOX);

    return msg;
}

/**
 * @brief Parses an incoming 9,079-byte PatternData SysEx message (0x4D).
 */
PatternData PatternData::parse(std::span<const uint8_t> slice) {
    if (slice.size() < SYSEX_MESSAGE_SIZE) {
        throw std::runtime_error("PatternData::parse: message too short (expected " +
                                 std::to_string(SYSEX_MESSAGE_SIZE) + ", got " + std::to_string(slice.size()) + ")");
    }
    auto header = ExtendedKorgSysEx::parse(slice.subspan(0, ExtendedKorgSysEx::LEN));
    if (!header) {
        throw std::runtime_error("PatternData::parse: invalid ExtendedKorgSysEx header");
    }
    if (slice[6] != 0x4D) {
        throw std::runtime_error("PatternData::parse: invalid function ID (expected 0x4D, got 0x" +
                                 std::to_string(slice[6]) + ")");
    }
    if (slice.back() != EOX) {
        throw std::runtime_error("PatternData::parse: missing EOX terminator");
    }

    uint8_t pattern_no = slice[7] & 0x0F;
    auto u7_payload = slice.subspan(8, slice.size() - 9);
    auto raw = U7ToU8::convert(u7_payload);
    if (raw.size() < RAW_PATTERN_SIZE) {
        throw std::runtime_error("PatternData::parse: decoded data too short (got " +
                                 std::to_string(raw.size()) + ", expected " + std::to_string(RAW_PATTERN_SIZE) + ")");
    }
    if (raw.size() > RAW_PATTERN_SIZE) {
        raw.resize(RAW_PATTERN_SIZE);
    }

    return create(pattern_no, "", std::move(raw));
}

} // namespace volsa2

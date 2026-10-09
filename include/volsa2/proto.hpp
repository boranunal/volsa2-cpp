/**
 * @file proto.hpp
 * @brief KORG Volca Sample 2 MIDI System Exclusive (SysEx) protocol definitions.
 * @details Implements message serialization, parsing, error validation, and header definitions
 *          for communicating with the KORG Volca Sample 2 over standard MIDI SysEx protocols.
 *          Covers device inquiry, device discovery, memory status, sample metadata headers,
 *          and raw PCM audio dumps.
 * @date 2026
 */

#pragma once

#include "volsa2/seven_bit.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <optional>
#include <stdexcept>
#include <array>
#include <cstring>

namespace volsa2 {

/** @brief MIDI 1.0 System Exclusive Start Status byte (0xF0). */
constexpr uint8_t EST = 0xF0;

/** @brief MIDI 1.0 End of Exclusive Status byte (0xF7). */
constexpr uint8_t EOX = 0xF7;

/** @brief KORG Manufacturer System Exclusive Identification Code (0x42). */
constexpr uint8_t KORG_ID = 0x42;

/**
 * @brief Volca Sample 2 Model ID byte sequence (4 bytes: 0x2D, 0x01, 0x08, 0x00).
 * @details Received during the SearchDeviceReply handshake to verify hardware identity.
 */
constexpr std::array<uint8_t, 4> VOLCA_SAMPLE_2_ID = {0x2D, 0x01, 0x08, 0x00};

/** @brief Acknowledge (ACK) status code returned by the device on success (0x23). */
constexpr uint8_t ACK_STATUS = 0x23;

/**
 * @enum NakStatus
 * @brief Negative Acknowledge (NAK) error codes transmitted by Volca Sample 2.
 */
enum class NakStatus : uint8_t {
    Busy       = 0x24, ///< The device is busy executing another internal routine (0x24).
    SampleFull = 0x25, ///< Sample storage memory has no free sectors remaining (0x25).
    DataFormat = 0x26  ///< The transmitted data packet has an invalid length or malformed format (0x26).
};

/**
 * @brief Converts a NakStatus error code into a human-readable diagnostic message.
 * @param status The NAK status code.
 * @return Constant string description of the error.
 */
inline const char* to_string(NakStatus status) {
    switch (status) {
        case NakStatus::Busy:       return "device is busy";
        case NakStatus::SampleFull: return "sample memory is full";
        case NakStatus::DataFormat: return "invalid data format";
        default:                    return "unknown error";
    }
}

/**
 * @struct Version
 * @brief Represents device firmware version numbers in Major.Minor format.
 */
struct Version {
    uint16_t major{0}; ///< Major revision number.
    uint16_t minor{0}; ///< Minor revision number.

    /**
     * @brief Formats the version numbers as a "Major.Minor" string.
     * @return Formatted version string (e.g. "1.2").
     */
    std::string to_string() const {
        return std::to_string(major) + "." + std::to_string(minor);
    }
};

/**
 * @struct KorgSysEx
 * @brief Standard 2-byte KORG System Exclusive header: [0xF0, 0x42].
 * @details Used primarily for initial system-level requests such as SearchDeviceRequest.
 */
struct KorgSysEx {
    static constexpr size_t LEN = 2;                        ///< Fixed header length in bytes.
    static constexpr std::array<uint8_t, 2> HEADER = {EST, KORG_ID}; ///< Header signature [0xF0, 0x42].

    /**
     * @brief Checks if the given byte slice begins with the KorgSysEx signature.
     * @param slice Raw byte buffer to inspect.
     * @return True if the first 2 bytes equal [0xF0, 0x42].
     */
    static bool matches(std::span<const uint8_t> slice) noexcept {
        return slice.size() >= LEN && slice[0] == HEADER[0] && slice[1] == HEADER[1];
    }

    /**
     * @brief Serializes the KorgSysEx header.
     * @return Byte vector containing [0xF0, 0x42].
     */
    static std::vector<uint8_t> encode() {
        return {HEADER[0], HEADER[1]};
    }
};

/**
 * @struct ExtendedKorgSysEx
 * @brief Extended 6-byte KORG Exclusive Header: [0xF0, 0x42, 0x3g, 0x00, 0x01, 0x2D].
 * @details Used for all Volca Sample 2 sample operations, parameters, and memory dumps.
 *          The 3rd byte combines a fixed nibble (0x30) with the device global MIDI channel `g` (0x0..0xF).
 */
struct ExtendedKorgSysEx {
    static constexpr size_t LEN = 6;                          ///< Fixed header length in bytes.
    static constexpr uint8_t CHANNEL_PREFIX = 3 << 4;         ///< Channel prefix nibble (0x30).
    static constexpr std::array<uint8_t, 3> SUFFIX = {0x00, 0x01, 0x2D}; ///< Suffix signature [0x00, 0x01, 0x2D].

    uint8_t global_channel{0}; ///< Global MIDI channel (0-15).

    /**
     * @brief Parses an ExtendedKorgSysEx header from the beginning of a buffer.
     * @param slice Byte span to parse.
     * @return std::optional<ExtendedKorgSysEx> if valid; std::nullopt otherwise.
     */
    static std::optional<ExtendedKorgSysEx> parse(std::span<const uint8_t> slice) noexcept {
        if (slice.size() < LEN) return std::nullopt;
        if (!KorgSysEx::matches(slice.subspan(0, KorgSysEx::LEN))) return std::nullopt;

        uint8_t ch_byte = slice[2];
        if ((ch_byte & 0xF0) != CHANNEL_PREFIX) return std::nullopt;
        if (slice[3] != SUFFIX[0] || slice[4] != SUFFIX[1] || slice[5] != SUFFIX[2]) return std::nullopt;

        ExtendedKorgSysEx header;
        header.global_channel = ch_byte & 0x0F;
        return header;
    }

    /**
     * @brief Serializes the 6-byte extended header incorporating the assigned global channel.
     * @return Byte vector containing the 6-byte sequence.
     */
    std::vector<uint8_t> encode() const {
        return {EST, KORG_ID, static_cast<uint8_t>(CHANNEL_PREFIX | (global_channel & 0x0F)),
                SUFFIX[0], SUFFIX[1], SUFFIX[2]};
    }
};

/**
 * @brief Encodes an 8-bit unsigned integer into 2 7-bit bytes [LSB, MSB].
 * @details In KORG parameters (such as slot numbers), single bytes are often split into
 *          2 bytes: `lsb = val & 0x7F` and `msb = (val >> 7) & 1`.
 * @param dest Destination buffer to append bytes to.
 * @param value The 8-bit value to encode.
 */
inline void write_u8(std::vector<uint8_t>& dest, uint8_t value) {
    auto [msb, lsb] = U7::split_u8(value);
    dest.push_back(lsb.as_u8());
    dest.push_back(msb);
}

/**
 * @brief Decodes a 2-byte [LSB, MSB] sequence into a single 8-bit unsigned integer.
 * @param slice Buffer starting at the 2-byte sequence.
 * @return std::pair<uint8_t, size_t> containing the reconstructed value and bytes consumed (2).
 * @throws std::runtime_error if slice has fewer than 2 bytes.
 */
inline std::pair<uint8_t, size_t> read_u8(std::span<const uint8_t> slice) {
    if (slice.size() < 2) {
        throw std::runtime_error("read_u8: slice too small");
    }
    uint8_t lsb = slice[0] & 0x7F;
    bool msb = (slice[1] & 0x01) != 0;
    return {U7(lsb).merge(msb), 2};
}

// ---------------- Protocol Messages ----------------

/**
 * @struct SearchDeviceRequest
 * @brief Outgoing device discovery inquiry message.
 * @details Format: [0xF0, 0x42, 0x50, 0x00, echo, 0xF7].
 */
struct SearchDeviceRequest {
    U7 echo{42}; ///< Arbitrary echo byte to verify response matching.

    /**
     * @brief Serializes the inquiry message.
     * @return Complete 6-byte SysEx message.
     */
    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> msg = KorgSysEx::encode();
        msg.push_back(0x50);
        msg.push_back(0x00);
        msg.push_back(echo.as_u8());
        msg.push_back(EOX);
        return msg;
    }
};

/**
 * @struct SearchDeviceReply
 * @brief Incoming device discovery response message.
 * @details Format: [0xF0, 0x42, 0x50, 0x01, channel, echo, 0x2D, 0x01, 0x08, 0x00, minor_lo, minor_hi, major_lo, major_hi, 0xF7].
 */
struct SearchDeviceReply {
    U7 device_id{0};  ///< Global MIDI channel assigned to device.
    U7 echo{0};       ///< Echo byte matching the request.
    Version version;  ///< Hardware firmware revision.

    /**
     * @brief Parses and validates a 15-byte SearchDeviceReply message.
     * @param slice Raw SysEx buffer.
     * @return Parsed SearchDeviceReply.
     * @throws std::runtime_error on format or signature mismatch.
     */
    static SearchDeviceReply parse(std::span<const uint8_t> slice);
};

/**
 * @struct StatusMessage
 * @brief Incoming execution acknowledgment or error message.
 * @details Format: [0xF0, 0x42, 0x3g, 0x00, 0x01, 0x2D, status_code, 0xF7].
 */
struct StatusMessage {
    bool is_ack{false};               ///< True if ACK (0x23), false if NAK.
    NakStatus nak{NakStatus::Busy};   ///< Specific NAK status if is_ack is false.

    /**
     * @brief Parses a status response packet.
     * @param slice Raw SysEx buffer.
     * @return Parsed StatusMessage.
     * @throws std::runtime_error on invalid format or unknown status code.
     */
    static StatusMessage parse(std::span<const uint8_t> slice);
};

/**
 * @struct SampleSpaceDumpRequest
 * @brief Outgoing request to inspect device memory sector usage.
 * @details Format: [0xF0, 0x42, 0x3g, 0x00, 0x01, 0x2D, 0x1B, 0xF7].
 */
struct SampleSpaceDumpRequest {
    uint8_t channel{0}; ///< Target device global MIDI channel.

    /**
     * @brief Serializes the memory query request.
     * @return 8-byte SysEx message.
     */
    std::vector<uint8_t> encode() const {
        ExtendedKorgSysEx header{channel};
        std::vector<uint8_t> msg = header.encode();
        msg.push_back(0x1B);
        msg.push_back(EOX);
        return msg;
    }
};

/**
 * @struct SampleSpaceDump
 * @brief Incoming device memory capacity and usage information.
 * @details Format: [0xF0, 0x42, 0x3g, 0x00, 0x01, 0x2D, 0x4B, used_lsb, used_msb, all_lsb, all_msb, 0xF7].
 */
struct SampleSpaceDump {
    uint16_t all_sector_size{0};  ///< Total number of memory sectors available on the device.
    uint16_t used_sector_size{0}; ///< Number of sectors currently occupied by loaded samples.

    /**
     * @brief Computes the fraction of memory occupied (range [0.0, 1.0]).
     * @return Ratio of used sectors to total sectors.
     */
    double occupied() const noexcept {
        if (all_sector_size == 0) return 0.0;
        return static_cast<double>(used_sector_size) / static_cast<double>(all_sector_size);
    }

    /**
     * @brief Parses a 12-byte SampleSpaceDump message.
     * @param slice Raw SysEx buffer.
     * @return Parsed SampleSpaceDump.
     * @throws std::runtime_error on format error.
     */
    static SampleSpaceDump parse(std::span<const uint8_t> slice);
};

/**
 * @struct SampleHeaderDumpRequest
 * @brief Outgoing request to query metadata for a specific sample slot.
 * @details Format: [0xF0, 0x42, 0x3g, 0x00, 0x01, 0x2D, 0x1E, slot_lsb, slot_msb, 0xF7].
 */
struct SampleHeaderDumpRequest {
    uint8_t channel{0};   ///< Target device global MIDI channel.
    uint8_t sample_no{0}; ///< Sample slot number (0-199).

    /**
     * @brief Serializes the slot query request.
     * @return 10-byte SysEx message.
     */
    std::vector<uint8_t> encode() const {
        ExtendedKorgSysEx header{channel};
        std::vector<uint8_t> msg = header.encode();
        msg.push_back(0x1E);
        write_u8(msg, sample_no);
        msg.push_back(EOX);
        return msg;
    }
};

/**
 * @struct SampleHeader
 * @brief Sample metadata descriptor containing name, length, playback speed, and volume level.
 * @details Serialized as 32 8-bit bytes encoded into 37 7-bit bytes via U8ToU7.
 *          The 32-byte payload contains:
 *          - Bytes 0-23: Sample name (ASCII/UTF-8 padded with 0x00).
 *          - Bytes 24-27: Length in samples (32-bit unsigned little-endian).
 *          - Bytes 28-29: Volume level (16-bit unsigned little-endian, default 65535).
 *          - Bytes 30-31: Playback speed/pitch (16-bit unsigned little-endian, default 16384).
 */
struct SampleHeader {
    static constexpr size_t DATA_SIZE_7BIT = 37;    ///< Size of the 7-bit encoded metadata payload.
    static constexpr size_t NAME_LEN = 24;          ///< Maximum length of the sample name string.
    static constexpr uint16_t DEFAULT_SPEED = 16384;///< Default playback speed (100% pitch).
    static constexpr uint16_t DEFAULT_LEVEL = 65535;///< Default playback volume level (100%).

    uint8_t sample_no{0};             ///< Slot index (0-199).
    std::string name;                 ///< Name of the sample (up to 24 characters).
    uint32_t length{0};               ///< Audio length in samples (at 31.25 kHz).
    uint16_t level{DEFAULT_LEVEL};    ///< Playback level.
    uint16_t speed{DEFAULT_SPEED};    ///< Playback speed.

    /**
     * @brief Determines whether this slot is empty (all fields unassigned).
     * @return True if name is empty and length, level, speed are 0.
     */
    bool is_empty() const noexcept {
        return name.empty() && length == 0 && level == 0 && speed == 0;
    }

    /**
     * @brief Factory creating an empty sample header (used to erase a slot on the device).
     * @param sample_no Target slot index (0-199).
     * @return SampleHeader with zeroed parameters.
     */
    static SampleHeader empty(uint8_t sample_no) noexcept {
        SampleHeader h;
        h.sample_no = sample_no;
        h.name = "";
        h.length = 0;
        h.level = 0;
        h.speed = 0;
        return h;
    }

    /**
     * @brief Encodes the sample header into a complete SysEx message.
     * @param channel Target device global MIDI channel.
     * @return 47-byte SysEx message.
     */
    std::vector<uint8_t> encode(uint8_t channel) const;

    /**
     * @brief Parses a SampleHeader message received from the device.
     * @param slice Raw SysEx buffer.
     * @return Parsed SampleHeader.
     * @throws std::runtime_error on format or checksum failure.
     */
    static SampleHeader parse(std::span<const uint8_t> slice);
};

/**
 * @struct SampleDataDumpRequest
 * @brief Outgoing request to download audio PCM sample data for a slot.
 * @details Format: [0xF0, 0x42, 0x3g, 0x00, 0x01, 0x2D, 0x1F, slot_lsb, slot_msb, 0xF7].
 */
struct SampleDataDumpRequest {
    uint8_t channel{0};   ///< Target device global MIDI channel.
    uint8_t sample_no{0}; ///< Sample slot number (0-199).

    /**
     * @brief Serializes the data dump request.
     * @return 10-byte SysEx message.
     */
    std::vector<uint8_t> encode() const {
        ExtendedKorgSysEx header{channel};
        std::vector<uint8_t> msg = header.encode();
        msg.push_back(0x1F);
        write_u8(msg, sample_no);
        msg.push_back(EOX);
        return msg;
    }
};

/**
 * @struct SampleData
 * @brief Sample audio payload containing raw 16-bit signed PCM samples.
 * @details Audio samples are serialized as 16-bit little-endian integers, packed via U8ToU7.
 *          During parsing, U7ToU8 reconstructs the byte stream which is then grouped into int16_t values.
 */
struct SampleData {
    uint8_t sample_no{0};          ///< Associated slot index.
    std::vector<int16_t> data;     ///< Contiguous array of 16-bit signed PCM audio samples.

    /**
     * @brief Helper factory to construct a matching pair of SampleHeader and SampleData.
     * @param sample_no Target slot index.
     * @param name Desired sample name.
     * @param data 16-bit signed mono PCM audio buffer.
     * @return std::pair<SampleHeader, SampleData> ready for sequential transmission.
     */
    static std::pair<SampleHeader, SampleData> create(uint8_t sample_no, const std::string& name, std::vector<int16_t> data) {
        std::string n = name.substr(0, std::min<size_t>(name.size(), SampleHeader::NAME_LEN));
        SampleHeader header;
        header.sample_no = sample_no;
        header.name = n;
        header.length = static_cast<uint32_t>(data.size());
        header.level = SampleHeader::DEFAULT_LEVEL;
        header.speed = SampleHeader::DEFAULT_SPEED;

        SampleData sd;
        sd.sample_no = sample_no;
        sd.data = std::move(data);
        return {header, sd};
    }

    /**
     * @brief Encodes the audio data into a complete SysEx packet.
     * @param channel Target device global MIDI channel.
     * @return Complete SysEx message containing the 7-bit packed audio samples.
     */
    std::vector<uint8_t> encode(uint8_t channel) const;

    /**
     * @brief Parses an incoming audio data SysEx message.
     * @param slice Raw SysEx buffer.
     * @return Parsed SampleData containing reconstructed int16_t PCM audio samples.
     * @throws std::runtime_error on format corruption.
     */
    static SampleData parse(std::span<const uint8_t> slice);
};

/**
 * @struct PatternDumpRequest
 * @brief Outgoing request to download sequencer pattern data for a specific pattern slot.
 * @details Format: [0xF0, 0x42, 0x3g, 0x00, 0x01, 0x2D, 0x1D, pattern_no, 0xF7] (9 bytes).
 */
struct PatternDumpRequest {
    uint8_t channel{0};    ///< Target device global MIDI channel.
    uint8_t pattern_no{0}; ///< Pattern slot number (0-15).

    /**
     * @brief Serializes the pattern dump request.
     * @return 9-byte SysEx message.
     */
    std::vector<uint8_t> encode() const {
        ExtendedKorgSysEx header{channel};
        std::vector<uint8_t> msg = header.encode();
        msg.push_back(0x1D);
        msg.push_back(pattern_no & 0x7F);
        msg.push_back(EOX);
        return msg;
    }
};

/**
 * @struct PatternData
 * @brief Volca Sample 2 sequencer pattern payload containing sequence steps, part parameters, and motion sequences.
 * @details Format in SysEx:
 *          - Header: [0xF0, 0x42, 0x3g, 0x00, 0x01, 0x2D, 0x4D, pattern_no] (8 bytes)
 *          - Payload: 9,070 7-bit bytes (encoding 7,936 bytes 8-bit unshielded data)
 *          - Terminator: [0xF7]
 *          Total SysEx message length: 9,079 bytes (0x2377).
 *          Decoded binary data is exactly 7,936 bytes (0x1F00), starting with 'PTST' magic bytes
 *          and containing the pattern name at offset 16 (up to 48 bytes null-padded).
 */
struct PatternData {
    static constexpr size_t RAW_PATTERN_SIZE = 7936;   ///< Exact unshielded binary size (0x1F00).
    static constexpr size_t ENCODED_7BIT_SIZE = 9070;  ///< Number of 7-bit bytes in SysEx payload.
    static constexpr size_t SYSEX_MESSAGE_SIZE = 9079; ///< Total size of complete SysEx frame (0x2377).
    static constexpr size_t MAX_PATTERNS = 16;         ///< Total patterns on device (0-15).
    static constexpr size_t NAME_OFFSET = 16;          ///< Byte offset of pattern name in raw binary.
    static constexpr size_t NAME_MAX_LEN = 48;         ///< Maximum length of pattern name string.

    uint8_t pattern_no{0};          ///< Pattern slot index (0-15).
    std::string name;               ///< Pattern name string (from offset 16).
    std::vector<uint8_t> raw_data;  ///< Exactly 7,936 bytes of raw pattern binary data.

    /**
     * @brief Creates a PatternData instance, optionally formatting the raw binary.
     * @param pattern_no Pattern slot index (0-15).
     * @param name Desired pattern name.
     * @param raw Raw 7,936-byte buffer (if empty, an initialized buffer with 'PTST' magic is created).
     * @return Initialized PatternData.
     */
    static PatternData create(uint8_t pattern_no, const std::string& name = "", std::vector<uint8_t> raw = {});

    /**
     * @brief Serializes the pattern into a complete SysEx message (0x4D).
     * @param channel Target device global MIDI channel.
     * @return 9,079-byte SysEx message.
     */
    std::vector<uint8_t> encode(uint8_t channel) const;

    /**
     * @brief Parses an incoming PatternData SysEx message (0x4D).
     * @param slice Raw SysEx buffer (must be at least 9,079 bytes).
     * @return Parsed PatternData.
     * @throws std::runtime_error on message size, header, or decoding error.
     */
    static PatternData parse(std::span<const uint8_t> slice);
};

} // namespace volsa2

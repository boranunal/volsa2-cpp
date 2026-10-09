/**
 * @file seven_bit.hpp
 * @brief 7-bit MIDI SysEx encoding and decoding utilities for KORG Volca Sample 2.
 * @details MIDI 1.0 System Exclusive messages restrict data bytes to 7 bits (0x00 - 0x7F).
 *          To transmit arbitrary 8-bit binary data (such as 16-bit PCM audio samples or
 *          structured sample metadata), KORG employs a bit-packing format where groups of up
 *          to 7 8-bit bytes are transformed into groups of up to 8 7-bit bytes.
 *          The first byte of each 8-byte octet aggregates the Most Significant Bits (MSBs) of
 *          the remaining 7 bytes. This file implements the U7 value type and bidirectional
 *          streaming conversion between 8-bit and 7-bit representations.
 * @date 2026
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <optional>
#include <utility>
#include <span>

namespace volsa2 {

/**
 * @class U7
 * @brief Strongly-typed representation of an unsigned 7-bit integer (range [0, 127]).
 * @details In MIDI SysEx protocols, any byte with the high bit (bit 7) set is interpreted
 *          as a status byte (e.g., 0xF0 for Exclusive Start, 0xF7 for End of Exclusive).
 *          Payload bytes must therefore strictly reside within [0, 127]. U7 encapsulates this
 *          invariant and provides bit-level operations for extracting, splitting, and merging
 *          MSBs during KORG 7-bit/8-bit transformations.
 */
class U7 {
public:
    /** @brief The maximum permissible value for a 7-bit integer (127 or 0x7F). */
    static constexpr uint8_t MAX_VAL = 127;

    /** @brief The minimum permissible value for a 7-bit integer (0 or 0x00). */
    static constexpr uint8_t MIN_VAL = 0;

    /**
     * @brief Constructs a U7 instance from a raw 8-bit integer, masking off the 8th bit.
     * @param raw The raw byte value. Only the lower 7 bits are preserved.
     */
    constexpr explicit U7(uint8_t raw = 0) noexcept : value_(raw & 0x7F) {}

    /**
     * @brief Returns the minimum U7 value (0).
     * @return U7 instance with value 0.
     */
    static constexpr U7 min() noexcept { return U7(MIN_VAL); }

    /**
     * @brief Returns the maximum U7 value (127).
     * @return U7 instance with value 127.
     */
    static constexpr U7 max() noexcept { return U7(MAX_VAL); }

    /**
     * @brief Validates whether a raw byte fits within 7 bits without truncation.
     * @param byte The byte to check.
     * @return std::optional<U7> containing the value if byte <= 127, otherwise std::nullopt.
     */
    static std::optional<U7> checked(uint8_t byte) noexcept {
        if (byte <= MAX_VAL) {
            return U7(byte);
        }
        return std::nullopt;
    }

    /**
     * @brief Returns the underlying 7-bit value as an unsigned 8-bit integer.
     * @return uint8_t value guaranteed to be <= 127.
     */
    constexpr uint8_t as_u8() const noexcept { return value_; }

    /**
     * @brief Explicit conversion operator to uint8_t.
     * @return uint8_t value guaranteed to be <= 127.
     */
    constexpr explicit operator uint8_t() const noexcept { return value_; }

    /** @brief Default equality operator. */
    constexpr bool operator==(const U7& other) const noexcept = default;

    /** @brief Default three-way comparison (spaceship) operator. */
    constexpr auto operator<=>(const U7& other) const noexcept = default;

    /**
     * @brief Splits an arbitrary 8-bit unsigned integer into its MSB and its lower 7-bit portion.
     * @details Bit 7 is extracted as a 1-bit flag (0 or 1), while bits 0-6 are encapsulated in a U7.
     * @param num The 8-bit number to decompose.
     * @return A std::pair where:
     *         - first: uint8_t equal to 1 if bit 7 was set, or 0 if unset.
     *         - second: U7 containing bits 0..6.
     */
    static constexpr std::pair<uint8_t, U7> split_u8(uint8_t num) noexcept {
        uint8_t msb = (num >> 7) & 0x01;
        return {msb, U7(num & 0x7F)};
    }

    /**
     * @brief Merges a 7-bit value with an external MSB flag to reconstruct an 8-bit byte.
     * @param msb If true, bit 7 of the result is set (value |= 0x80); otherwise, bit 7 remains 0.
     * @return The reconstructed 8-bit unsigned integer.
     */
    constexpr uint8_t merge(bool msb) const noexcept {
        return value_ | (msb ? 0x80 : 0x00);
    }

    /**
     * @brief Extracts the n-th bit of this U7 byte and aligns it to bit 7 (0x80).
     * @details When decoding a KORG MSB header byte, bit 0 corresponds to the MSB of data byte 0,
     *          bit 1 to data byte 1, ..., up to bit 6 for data byte 6. This method tests if bit
     *          @p n is set and returns 0x80 if true, or 0x00 if false.
     * @param n Zero-based bit index to test (0 <= n <= 6).
     * @return 0x80 if bit n is 1; 0x00 if bit n is 0.
     */
    constexpr uint8_t take_nth_msb(size_t n) const noexcept {
        return (value_ & (1U << n)) ? 0x80 : 0x00;
    }

private:
    uint8_t value_{0}; ///< Internal 7-bit storage ([0, 127]).
};

/**
 * @struct U8ToU7
 * @brief Encodes raw 8-bit bytes into the 7-bit KORG SysEx transmission format.
 * @details Every block of up to 7 bytes is converted into a block of up to 8 7-bit bytes.
 *          Byte 0 of the output block is an MSB header byte where bit `j` stores the MSB of
 *          input byte `j`. Output bytes 1..j+1 store the lower 7 bits of the input bytes.
 */
struct U8ToU7 {
    /**
     * @brief Computes the exact encoded size in bytes for a given input length.
     * @details For every 7 input bytes, an additional MSB header byte is required.
     *          Formula: len + ceil(len / 7).
     * @param len Number of raw 8-bit input bytes.
     * @return Total number of 7-bit output bytes.
     */
    static constexpr size_t convert_len(size_t len) noexcept {
        size_t msbs = len / 7;
        if (len % 7 != 0) {
            msbs += 1;
        }
        return len + msbs;
    }

    /**
     * @brief Converts a contiguous span of 8-bit bytes into 7-bit KORG SysEx format.
     * @param input Span of raw 8-bit input data.
     * @return std::vector<uint8_t> containing the 7-bit packed bytes.
     */
    static std::vector<uint8_t> convert(std::span<const uint8_t> input);
};

/**
 * @struct U7ToU8
 * @brief Decodes 7-bit KORG SysEx transmission data back into raw 8-bit bytes.
 * @details Parses 8-byte octets (or a final partial block of length > 1). The first byte
 *          is treated as the MSB header, and its bits are distributed across the subsequent
 *          data bytes to restore their 8th bit.
 */
struct U7ToU8 {
    /**
     * @brief Computes the decoded size in bytes for a given 7-bit SysEx payload length.
     * @details Subtracts one MSB header byte for every group of up to 8 bytes.
     *          Formula: len - ceil(len / 8) for len > 0; 0 for len == 0.
     * @param len Number of 7-bit input bytes.
     * @return Total number of decoded 8-bit output bytes.
     */
    static constexpr size_t convert_len(size_t len) noexcept {
        if (len == 0) return 0;
        size_t msbs = len / 8;
        if (len % 8 != 0) {
            msbs += 1;
        }
        return len - msbs;
    }

    /**
     * @brief Converts a contiguous span of 7-bit KORG SysEx data into raw 8-bit bytes.
     * @param input Span of 7-bit packed data.
     * @return std::vector<uint8_t> containing the restored 8-bit data bytes.
     */
    static std::vector<uint8_t> convert(std::span<const uint8_t> input);
};

} // namespace volsa2

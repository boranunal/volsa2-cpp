/**
 * @file seven_bit.cpp
 * @brief Implementation of 7-bit / 8-bit SysEx conversion routines.
 * @details Implements the chunking and bit-packing logic for U8ToU7 and U7ToU8 converters.
 * @date 2026
 */

#include "volsa2/seven_bit.hpp"
#include <algorithm>

namespace volsa2 {

/**
 * @brief Encodes 8-bit data into KORG 7-bit SysEx octets.
 * @details The encoding process operates on successive chunks of up to 7 input bytes:
 *          1. Compute chunk_len = min(7, remaining_bytes).
 *          2. Initialize an empty MSB accumulator byte (msb_byte = 0).
 *          3. For each byte j in the chunk:
 *             a. Extract bit 7 (MSB) as 0 or 1.
 *             b. Shift the MSB into bit position j of msb_byte (msb << j).
 *             c. Store the lower 7 bits (byte & 0x7F) in a temporary array.
 *          4. Append the assembled msb_byte to the output stream.
 *          5. Append the 7-bit data bytes (1..chunk_len) to the output stream.
 * @param input Contiguous view of raw 8-bit bytes to encode.
 * @return std::vector<uint8_t> containing the 7-bit encoded stream.
 */
std::vector<uint8_t> U8ToU7::convert(std::span<const uint8_t> input) {
    std::vector<uint8_t> output;
    const size_t in_len = input.size();
    output.reserve(convert_len(in_len));

    for (size_t i = 0; i < in_len; i += 7) {
        const size_t chunk_len = std::min<size_t>(7, in_len - i);
        uint8_t msb_byte = 0;
        uint8_t lower_bytes[7] = {0};

        for (size_t j = 0; j < chunk_len; ++j) {
            uint8_t byte = input[i + j];
            auto [msb, byte7] = U7::split_u8(byte);

            // Place the MSB of the j-th data byte into bit j of the MSB header
            msb_byte |= static_cast<uint8_t>(msb << j);
            lower_bytes[j] = byte7.as_u8();
        }

        // Write the MSB header byte first
        output.push_back(msb_byte);

        // Write the subsequent 7-bit payload bytes
        for (size_t j = 0; j < chunk_len; ++j) {
            output.push_back(lower_bytes[j]);
        }
    }

    return output;
}

/**
 * @brief Decodes KORG 7-bit SysEx octets back into 8-bit binary data.
 * @details The decoding process processes input in blocks of up to 8 bytes:
 *          1. Compute chunk_len = min(8, remaining_bytes).
 *          2. If chunk_len <= 1, only an orphaned MSB byte exists with no data bytes; terminate.
 *          3. Interpret the first byte as the MSB header byte (input[i]).
 *          4. The number of data bytes in this block is num_data_bytes = chunk_len - 1.
 *          5. For each data byte j from 0 to num_data_bytes - 1:
 *             a. Extract the lower 7 bits from input[i + 1 + j].
 *             b. Query the MSB header for bit j using take_nth_msb(j), which returns 0x80 or 0x00.
 *             c. Combine the lower 7 bits with the 8th bit: restored = lower7 | msb_flag.
 *             d. Append restored to the output stream.
 * @param input Contiguous view of 7-bit packed bytes.
 * @return std::vector<uint8_t> containing the restored 8-bit binary data.
 */
std::vector<uint8_t> U7ToU8::convert(std::span<const uint8_t> input) {
    std::vector<uint8_t> output;
    const size_t in_len = input.size();
    if (in_len == 0) {
        return output;
    }
    output.reserve(convert_len(in_len));

    for (size_t i = 0; i < in_len; i += 8) {
        const size_t chunk_len = std::min<size_t>(8, in_len - i);
        if (chunk_len <= 1) {
            // An isolated MSB byte without trailing payload bytes is invalid or padding
            break;
        }

        U7 msb_header(input[i]);
        const size_t num_data_bytes = chunk_len - 1;

        for (size_t j = 0; j < num_data_bytes; ++j) {
            uint8_t byte7 = input[i + 1 + j] & 0x7F;
            uint8_t restored = byte7 | msb_header.take_nth_msb(j);
            output.push_back(restored);
        }
    }

    return output;
}

} // namespace volsa2

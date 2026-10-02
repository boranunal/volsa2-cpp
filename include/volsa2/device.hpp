/**
 * @file device.hpp
 * @brief High-level ALSA MIDI Sequencer interface for KORG Volca Sample 2.
 * @details Encapsulates client registration, automatic port discovery, duplex subscription,
 *          chunked SysEx transmission with flow-control cooldown, and high-level sample
 *          librarian operations (list, upload, download, delete).
 * @author Volsa2 Project Team
 * @date 2026
 */

#pragma once

#include "volsa2/proto.hpp"
#include <chrono>
#include <vector>
#include <span>
#include <functional>
#include <alsa/asoundlib.h>

namespace volsa2 {

/**
 * @class Device
 * @brief Manages a live connection to a KORG Volca Sample 2 synthesizer over ALSA MIDI.
 * @details Implements the full communication state machine:
 *          - Opens an ALSA Sequencer instance and registers a duplex port named "VolSa2".
 *          - Scans for an ALSA client named "volca sample" and connects bi-directionally.
 *          - Transmits SearchDeviceRequest to obtain the device's global MIDI channel and firmware version.
 *          - Enforces chunked SysEx delivery (256-byte blocks with inter-chunk sleep) to prevent hardware input FIFO drops.
 *          - Exposes synchronous methods for querying memory, reading sample headers, uploading audio, and erasing slots.
 */
class Device {
public:
    /**
     * @brief Constructs a Device instance with a configurable chunk transmission cooldown.
     * @param chunk_cooldown Sleep duration between successive 256-byte SysEx chunks (default 10 ms).
     */
    explicit Device(std::chrono::milliseconds chunk_cooldown = std::chrono::milliseconds(10));

    /**
     * @brief Destructor. Closes sequencer ports and disconnects from ALSA.
     */
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    /** @brief Move constructor. Transfers sequencer handle and port ownership. */
    Device(Device&& other) noexcept;

    /** @brief Move assignment operator. Disconnects existing handle before transferring. */
    Device& operator=(Device&& other) noexcept;

    /**
     * @brief Opens the ALSA sequencer, discovers Volca Sample 2, subscribes ports, and performs handshake.
     * @throws std::runtime_error if ALSA fails to open or if the Volca Sample device cannot be found.
     */
    void connect();

    /**
     * @brief Unsubscribes ports and closes the ALSA sequencer instance.
     */
    void disconnect();

    /**
     * @brief Checks if the ALSA sequencer handle is currently open and connected.
     * @return True if connected, false otherwise.
     */
    bool is_connected() const noexcept { return seq_ != nullptr; }

    /**
     * @brief Retrieves the active global MIDI channel of the connected Volca device.
     * @return U7 instance containing the channel (0-15).
     */
    U7 channel() const noexcept { return channel_; }

    /**
     * @brief Retrieves the firmware version reported by the device during handshake.
     * @return Version structure containing major and minor numbers.
     */
    Version version() const noexcept { return version_; }

    /**
     * @brief Transmits a raw SysEx message in 256-byte chunks with flow-control cooldown.
     * @param data Byte view of the complete SysEx packet (must start with 0xF0 and end with 0xF7).
     * @param progress_cb Optional callback invoked after each transmitted chunk with (sent_bytes, total_bytes).
     * @throws std::runtime_error on ALSA transmit error.
     */
    void send_raw(std::span<const uint8_t> data, std::function<void(size_t sent, size_t total)> progress_cb = nullptr);

    /**
     * @brief Blocks until a complete SysEx packet terminated with EOX (0xF7) is received or timeout expires.
     * @param timeout Maximum duration to wait before timing out (default 10s).
     * @return std::vector<uint8_t> containing the received SysEx packet.
     * @throws std::runtime_error on ALSA input queue error or timeout.
     */
    std::vector<uint8_t> receive_raw(std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));

    /**
     * @brief Flushes pending incoming events from both kernel and user-space ALSA input queues.
     */
    void clear_input();

    /**
     * @brief Drains pending non-SysEx events (Active Sensing 0xFE, MIDI Clock 0xF8) and stages incoming SysEx.
     */
    void drain_pending_events();

    /**
     * @brief Queries the device's storage sector usage.
     * @return SampleSpaceDump containing used and total sector counts.
     * @throws std::runtime_error on communication failure.
     */
    SampleSpaceDump get_sample_space();

    /**
     * @brief Queries metadata for a specific sample slot.
     * @param sample_no Slot index (0-199).
     * @return SampleHeader describing the slot contents.
     * @throws std::runtime_error if sample_no > 199 or communication fails.
     */
    SampleHeader get_sample_header(uint8_t sample_no);

    /**
     * @brief Queries metadata for all 200 sample slots on the device sequentially.
     * @param on_slot_read Optional progress callback invoked as each slot header is retrieved.
     * @return std::vector<SampleHeader> containing 200 headers.
     */
    std::vector<SampleHeader> get_all_sample_headers(std::function<void(uint8_t slot, const SampleHeader& header)> on_slot_read = nullptr);

    /**
     * @brief Downloads raw 16-bit PCM audio samples from a specific device slot.
     * @param sample_no Slot index (0-199).
     * @return SampleData containing the audio buffer.
     * @throws std::runtime_error if slot is out of range or communication fails.
     */
    SampleData get_sample(uint8_t sample_no);

    /**
     * @brief Erases the sample in the specified slot by transmitting an empty header.
     * @param sample_no Slot index (0-199).
     * @throws std::runtime_error if the device rejects the erase command.
     */
    void delete_sample(uint8_t sample_no);

    /**
     * @brief Uploads a new sample header and PCM audio data to the device.
     * @details Sends the SampleHeader, waits for device ACK, sends the SampleData, and waits for final ACK.
     * @param header Metadata header describing slot, name, length, level, and speed.
     * @param data Audio payload containing 16-bit mono PCM samples at 31.25 kHz.
     * @param progress_cb Optional callback invoked during audio chunk transmission (sent_bytes, total_bytes).
     * @throws std::runtime_error if the device returns a NAK status code.
     */
    void send_sample(const SampleHeader& header, const SampleData& data,
                     std::function<void(size_t sent, size_t total)> progress_cb = nullptr);

private:
    snd_seq_t* seq_{nullptr};             ///< ALSA sequencer handle.
    int my_client_{-1};                    ///< Local ALSA client ID.
    int my_port_{-1};                      ///< Local ALSA port ID.
    int volca_client_{-1};                 ///< Remote Volca Sample ALSA client ID.
    int volca_port_{-1};                   ///< Remote Volca Sample ALSA port ID.
    U7 channel_{0};                        ///< Global MIDI channel discovered during handshake.
    Version version_;                      ///< Device firmware version.
    std::chrono::milliseconds chunk_cooldown_{10}; ///< Inter-chunk delay to prevent hardware FIFO overflows.
    std::vector<uint8_t> rx_buffer_;       ///< Incoming SysEx byte buffer staged across events.
};

} // namespace volsa2

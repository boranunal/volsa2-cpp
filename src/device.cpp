/**
 * @file device.cpp
 * @brief Implementation of ALSA MIDI Sequencer communication for Volca Sample 2.
 * @details Implements low-level ALSA calls (snd_seq_open, snd_seq_query_next_client,
 *          snd_seq_subscribe_port, snd_seq_event_output_direct, snd_seq_event_input).
 * @author Volsa2 Project Team
 * @date 2026
 */

#include "volsa2/device.hpp"

#include <thread>
#include <stdexcept>
#include <string_view>
#include <algorithm>
#include <poll.h>
#include <cerrno>
#include <cstring>

namespace volsa2 {

namespace {
constexpr const char* SELF_CLIENT_NAME = "VolSa2";
constexpr const char* VOLCA_CLIENT_NAME = "volca sample";
} // namespace

Device::Device(std::chrono::milliseconds chunk_cooldown)
    : chunk_cooldown_(chunk_cooldown) {}

Device::~Device() {
    disconnect();
}

Device::Device(Device&& other) noexcept
    : seq_(other.seq_),
      my_client_(other.my_client_),
      my_port_(other.my_port_),
      volca_client_(other.volca_client_),
      volca_port_(other.volca_port_),
      channel_(other.channel_),
      version_(other.version_),
      chunk_cooldown_(other.chunk_cooldown_) {
    other.seq_ = nullptr;
    other.my_client_ = -1;
    other.my_port_ = -1;
    other.volca_client_ = -1;
    other.volca_port_ = -1;
}

Device& Device::operator=(Device&& other) noexcept {
    if (this != &other) {
        disconnect();
        seq_ = other.seq_;
        my_client_ = other.my_client_;
        my_port_ = other.my_port_;
        volca_client_ = other.volca_client_;
        volca_port_ = other.volca_port_;
        channel_ = other.channel_;
        version_ = other.version_;
        chunk_cooldown_ = other.chunk_cooldown_;

        other.seq_ = nullptr;
        other.my_client_ = -1;
        other.my_port_ = -1;
        other.volca_client_ = -1;
        other.volca_port_ = -1;
    }
    return *this;
}

/**
 * @brief Establishes an ALSA Sequencer duplex session with the Volca Sample 2.
 * @details Steps:
 *          1. Open ALSA Sequencer (`snd_seq_open`) with duplex permissions.
 *          2. Set client identity to "VolSa2".
 *          3. Create local application port with read, write, and duplex capabilities.
 *          4. Enumerate existing ALSA clients to locate one named "volca sample".
 *          5. Query the first port exposed by the Volca client.
 *          6. Create duplex port subscriptions (Volca -> Local, Local -> Volca).
 *          7. Transmit SearchDeviceRequest and receive SearchDeviceReply to determine
 *             the hardware's assigned global channel and firmware version.
 */
void Device::connect() {
    if (seq_) {
        return;
    }

    int err = snd_seq_open(&seq_, "default", SND_SEQ_OPEN_DUPLEX, 0);
    if (err < 0) {
        throw std::runtime_error("snd_seq_open failed: " + std::string(snd_strerror(err)));
    }

    snd_seq_set_client_name(seq_, SELF_CLIENT_NAME);

    my_port_ = snd_seq_create_simple_port(
        seq_,
        SELF_CLIENT_NAME,
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE |
        SND_SEQ_PORT_CAP_READ  | SND_SEQ_PORT_CAP_SUBS_READ  |
        SND_SEQ_PORT_CAP_DUPLEX,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION | SND_SEQ_PORT_TYPE_PORT
    );
    if (my_port_ < 0) {
        disconnect();
        throw std::runtime_error("snd_seq_create_simple_port failed: " + std::string(snd_strerror(my_port_)));
    }

    my_client_ = snd_seq_client_id(seq_);

    // Configure generous client input/output pools in kernel space (32,768 cells)
    // and large user-space buffers (128 KB) to prevent -ENOSPC FIFO overruns during large transfers.
    snd_seq_set_client_pool_input(seq_, 32768);
    snd_seq_set_client_pool_output(seq_, 32768);
    snd_seq_set_input_buffer_size(seq_, 131072);
    snd_seq_set_output_buffer_size(seq_, 131072);
    snd_seq_nonblock(seq_, 1);

    // Iterate through connected ALSA clients to find "volca sample"
    snd_seq_client_info_t* cinfo = nullptr;
    snd_seq_client_info_alloca(&cinfo);
    snd_seq_client_info_set_client(cinfo, -1);

    bool found = false;
    while (snd_seq_query_next_client(seq_, cinfo) >= 0) {
        int client_id = snd_seq_client_info_get_client(cinfo);
        const char* name = snd_seq_client_info_get_name(cinfo);
        if (name && std::string_view(name) == VOLCA_CLIENT_NAME) {
            snd_seq_port_info_t* pinfo = nullptr;
            snd_seq_port_info_alloca(&pinfo);
            snd_seq_port_info_set_client(pinfo, client_id);
            snd_seq_port_info_set_port(pinfo, -1);
            if (snd_seq_query_next_port(seq_, pinfo) >= 0) {
                volca_client_ = client_id;
                volca_port_ = snd_seq_port_info_get_port(pinfo);
                found = true;
                break;
            }
        }
    }

    if (!found) {
        disconnect();
        throw std::runtime_error("Could not find 'volca sample' device on ALSA MIDI Sequencer.");
    }

    // Subscribe bi-directional communication ports
    snd_seq_port_subscribe_t* sub = nullptr;
    snd_seq_port_subscribe_alloca(&sub);
    snd_seq_addr_t sender, dest;

    // Volca -> Local
    sender.client = static_cast<unsigned char>(volca_client_);
    sender.port = static_cast<unsigned char>(volca_port_);
    dest.client = static_cast<unsigned char>(my_client_);
    dest.port = static_cast<unsigned char>(my_port_);
    snd_seq_port_subscribe_set_sender(sub, &sender);
    snd_seq_port_subscribe_set_dest(sub, &dest);
    err = snd_seq_subscribe_port(seq_, sub);
    if (err < 0) {
        disconnect();
        throw std::runtime_error("Failed to subscribe Volca -> Local: " + std::string(snd_strerror(err)));
    }

    // Local -> Volca
    sender.client = static_cast<unsigned char>(my_client_);
    sender.port = static_cast<unsigned char>(my_port_);
    dest.client = static_cast<unsigned char>(volca_client_);
    dest.port = static_cast<unsigned char>(volca_port_);
    snd_seq_port_subscribe_set_sender(sub, &sender);
    snd_seq_port_subscribe_set_dest(sub, &dest);
    err = snd_seq_subscribe_port(seq_, sub);
    if (err < 0) {
        disconnect();
        throw std::runtime_error("Failed to subscribe Local -> Volca: " + std::string(snd_strerror(err)));
    }

    // Perform inquiry handshake
    clear_input();
    SearchDeviceRequest req{U7(42)};
    send_raw(req.encode());

    auto reply_raw = receive_raw(std::chrono::milliseconds(5000));
    auto reply = SearchDeviceReply::parse(reply_raw);
    channel_ = reply.device_id;
    version_ = reply.version;
}

/**
 * @brief Closes the ALSA Sequencer session and resets all identifiers.
 */
void Device::disconnect() {
    clear_input();
    if (seq_) {
        snd_seq_close(seq_);
        seq_ = nullptr;
    }
    my_client_ = -1;
    my_port_ = -1;
    volca_client_ = -1;
    volca_port_ = -1;
}

/**
 * @brief Flushes pending incoming events from both kernel and user-space ALSA input queues.
 */
void Device::clear_input() {
    if (!seq_) return;
    snd_seq_drop_input(seq_);
    snd_seq_drop_input_buffer(seq_);
    rx_buffer_.clear();
}

/**
 * @brief Drains pending non-SysEx events (Active Sensing 0xFE, MIDI Clock 0xF8) and stages incoming SysEx.
 */
void Device::drain_pending_events() {
    if (!seq_) return;

    while (true) {
        snd_seq_event_t* ev = nullptr;
        int err = snd_seq_event_input(seq_, &ev);
        if (err == -EAGAIN) {
            break;
        }
        if (err == -ENOSPC) {
            // Buffer overrun recovered by ALSA; continue draining
            continue;
        }
        if (err < 0 || !ev) {
            break;
        }

        if (ev->type == SND_SEQ_EVENT_SYSEX &&
            ev->source.client == volca_client_ &&
            ev->source.port == volca_port_ &&
            ev->dest.port == my_port_) {
            const auto* ptr = static_cast<const uint8_t*>(ev->data.ext.ptr);
            size_t len = ev->data.ext.len;
            rx_buffer_.insert(rx_buffer_.end(), ptr, ptr + len);
        }
        // Non-SysEx events (Active Sensing 0xFE, MIDI Clock 0xF8) are silently discarded
    }
}

/**
 * @brief Sends a SysEx packet in 256-byte chunks with optional delay between chunks.
 * @details The KORG Volca Sample 2 hardware contains a limited USB MIDI input FIFO buffer.
 *          Chunking into 256-byte blocks followed by a brief sleep (default 10 ms) resolves this.
 *          Drains pending incoming MIDI events after each chunk to avoid ALSA FIFO overruns.
 * @param data Byte view of the packet to send.
 * @param progress_cb Optional callback invoked after each chunk.
 */
void Device::send_raw(std::span<const uint8_t> data, std::function<void(size_t sent, size_t total)> progress_cb) {
    if (!seq_) {
        throw std::runtime_error("Device not connected");
    }

    const size_t total_len = data.size();
    for (size_t offset = 0; offset < total_len; offset += 256) {
        size_t chunk_len = std::min<size_t>(256, total_len - offset);
        const uint8_t* chunk_ptr = data.data() + offset;

        snd_seq_event_t ev;
        snd_seq_ev_clear(&ev);
        snd_seq_ev_set_sysex(&ev, static_cast<unsigned int>(chunk_len), const_cast<uint8_t*>(chunk_ptr));
        ev.source.port = static_cast<unsigned char>(my_port_);
        ev.dest.client = static_cast<unsigned char>(volca_client_);
        ev.dest.port = static_cast<unsigned char>(volca_port_);
        snd_seq_ev_set_direct(&ev);
        snd_seq_ev_set_priority(&ev, 1);

        int err = snd_seq_event_output_direct(seq_, &ev);
        if (err < 0) {
            throw std::runtime_error("snd_seq_event_output_direct failed: " + std::string(snd_strerror(err)));
        }

        // Apply flow control cooldown if not the final chunk
        bool is_end = (chunk_ptr[chunk_len - 1] == EOX);
        if (!is_end && chunk_cooldown_.count() > 0) {
            std::this_thread::sleep_for(chunk_cooldown_);
        }

        // Keep ALSA input buffer clean during multi-second chunk streams
        drain_pending_events();

        if (progress_cb) {
            progress_cb(offset + chunk_len, total_len);
        }
    }

    snd_seq_sync_output_queue(seq_);
    snd_seq_drain_output(seq_);
}

/**
 * @brief Reads incoming ALSA Sequencer events until an entire SysEx message ending with 0xF7 is gathered.
 * @param timeout Maximum duration to wait before timing out.
 * @return Reassembled SysEx byte buffer.
 */
std::vector<uint8_t> Device::receive_raw(std::chrono::milliseconds timeout) {
    if (!seq_) {
        throw std::runtime_error("Device not connected");
    }

    auto start_time = std::chrono::steady_clock::now();

    // Check if a complete message was already staged into rx_buffer_
    if (!rx_buffer_.empty() && rx_buffer_.back() == EOX) {
        std::vector<uint8_t> result = std::move(rx_buffer_);
        rx_buffer_.clear();
        return result;
    }

    int pcount = snd_seq_poll_descriptors_count(seq_, POLLIN);
    std::vector<struct pollfd> pfds(pcount > 0 ? pcount : 1);
    if (pcount > 0) {
        snd_seq_poll_descriptors(seq_, pfds.data(), pcount, POLLIN);
    }

    while (true) {
        snd_seq_event_t* ev = nullptr;
        int err = snd_seq_event_input(seq_, &ev);
        if (err == -ENOSPC) {
            // ALSA input buffer overrun flag; ALSA automatically clears the FIFO.
            // Continue reading without throwing a fatal exception!
            continue;
        }
        if (err == -EAGAIN) {
            // No more events in user-space buffer, poll ALSA file descriptor
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
            if (elapsed >= timeout) {
                rx_buffer_.clear();
                throw std::runtime_error("Timeout waiting for response from Volca Sample 2 (" +
                                         std::to_string(timeout.count()) + " ms)");
            }

            int remaining_ms = static_cast<int>((timeout - elapsed).count());
            int wait_ms = std::clamp(remaining_ms, 1, 100);
            int ret = poll(pfds.data(), pcount, wait_ms);
            if (ret < 0 && errno != EINTR) {
                throw std::runtime_error("poll failed: " + std::string(strerror(errno)));
            }
            continue;
        }
        if (err < 0) {
            throw std::runtime_error("snd_seq_event_input failed: " + std::string(snd_strerror(err)));
        }
        if (!ev) {
            continue;
        }

        if (ev->type == SND_SEQ_EVENT_SYSEX &&
            ev->source.client == volca_client_ &&
            ev->source.port == volca_port_ &&
            ev->dest.port == my_port_) {
            const auto* ptr = static_cast<const uint8_t*>(ev->data.ext.ptr);
            size_t len = ev->data.ext.len;
            rx_buffer_.insert(rx_buffer_.end(), ptr, ptr + len);

            if (!rx_buffer_.empty() && rx_buffer_.back() == EOX) {
                std::vector<uint8_t> result = std::move(rx_buffer_);
                rx_buffer_.clear();
                return result;
            }
        }
    }
}

SampleSpaceDump Device::get_sample_space() {
    clear_input();
    SampleSpaceDumpRequest req{channel_.as_u8()};
    send_raw(req.encode());
    auto raw = receive_raw(std::chrono::milliseconds(5000));
    return SampleSpaceDump::parse(raw);
}

SampleHeader Device::get_sample_header(uint8_t sample_no) {
    if (sample_no > 199) {
        throw std::runtime_error("sample_no must be less than 200");
    }
    clear_input();
    SampleHeaderDumpRequest req{channel_.as_u8(), sample_no};
    send_raw(req.encode());
    auto raw = receive_raw(std::chrono::milliseconds(5000));
    return SampleHeader::parse(raw);
}

std::vector<SampleHeader> Device::get_all_sample_headers(std::function<void(uint8_t slot, const SampleHeader& header)> on_slot_read) {
    std::vector<SampleHeader> headers;
    headers.reserve(200);

    for (uint8_t i = 0; i < 200; ++i) {
        SampleHeader h = get_sample_header(i);
        if (on_slot_read) {
            on_slot_read(i, h);
        }
        headers.push_back(std::move(h));
    }
    return headers;
}

SampleData Device::get_sample(uint8_t sample_no) {
    if (sample_no > 199) {
        throw std::runtime_error("sample_no must be less than 200");
    }
    clear_input();
    SampleDataDumpRequest req{channel_.as_u8(), sample_no};
    send_raw(req.encode());
    auto raw = receive_raw(std::chrono::milliseconds(30000));
    return SampleData::parse(raw);
}

void Device::delete_sample(uint8_t sample_no) {
    if (sample_no > 199) {
        throw std::runtime_error("sample_no must be less than 200");
    }
    clear_input();
    SampleHeader empty_h = SampleHeader::empty(sample_no);
    send_raw(empty_h.encode(channel_.as_u8()));

    auto status_raw = receive_raw(std::chrono::milliseconds(5000));
    auto status = StatusMessage::parse(status_raw);
    if (!status.is_ack) {
        throw std::runtime_error(std::string("Device returned error: ") + to_string(status.nak));
    }
}

void Device::send_sample(const SampleHeader& header, const SampleData& data,
                         std::function<void(size_t sent, size_t total)> progress_cb) {
    clear_input();

    // 1. Transmit sample metadata header and verify device ACK
    send_raw(header.encode(channel_.as_u8()));
    auto s1 = StatusMessage::parse(receive_raw(std::chrono::milliseconds(5000)));
    if (!s1.is_ack) {
        if (s1.nak == NakStatus::SampleFull) {
            throw std::runtime_error("Device sample memory is full. Please erase unused samples to free space.");
        }
        throw std::runtime_error(std::string("Device rejected header: ") + to_string(s1.nak));
    }

    clear_input();

    // 2. Transmit raw audio sample data dump and verify device ACK
    send_raw(data.encode(channel_.as_u8()), progress_cb);
    auto s2 = StatusMessage::parse(receive_raw(std::chrono::milliseconds(20000)));
    if (!s2.is_ack) {
        if (s2.nak == NakStatus::SampleFull) {
            throw std::runtime_error("Device sample memory is full. Please erase unused samples to free space.");
        }
        throw std::runtime_error(std::string("Device rejected sample data: ") + to_string(s2.nak));
    }
}

} // namespace volsa2

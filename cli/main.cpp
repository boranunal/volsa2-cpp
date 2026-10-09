/**
 * @file main.cpp
 * @brief Command-line interface (CLI) for Volsa 2 sample manager.
 * @details Implements the commands: `list` (ls), `download` (dl), `upload` (up),
 *          and `remove` (rm) matching the behavior and flags of the original volsa2 CLI.
 * @author Volsa2 Project Team
 * @date 2026
 */

#include "volsa2/device.hpp"
#include "volsa2/audio.hpp"
#include "volsa2/package.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <iomanip>
#include <chrono>

namespace fs = std::filesystem;

/**
 * @brief Prints CLI usage help and supported command flags.
 * @param prog Executable binary name.
 */
void print_help(const char* prog) {
    std::cout << "volsa2-cli (C++ edition) - KORG Volca Sample 2 sample manager over ALSA\n\n"
              << "Usage: " << prog << " [options] <command> [args...]\n\n"
              << "Commands:\n"
              << "  list, ls [-a]              List samples loaded into device (-a to show empty slots)\n"
              << "  download, dl <slot> [-o path] Download sample from device slot (default: ./\n"
              << "  upload, up <file> [slot] [-m mode] [-o path] [--dry-run]\n"
              << "                             Upload audio file to device slot (mode: mid, left, right, side)\n"
              << "  remove, rm <slot> [-p]     Remove sample at slot (-p to print name)\n"
              << "  download-package, pkg-dl <file.ivlcsplpreset> [--name <name>] [--author <author>]\n"
              << "                             Download complete package (all 16 patterns and 200 samples)\n"
              << "  upload-package, pkg-up <file.ivlcsplpreset> [-y] [--samples-only] [--patterns-only] [--clear-empty] [--dry-run]\n"
              << "                             Upload complete package to device\n\n"
              << "Options:\n"
              << "  -c, --cooldown <ms>        Chunk cooldown in ms (default: 10)\n"
              << "  -h, --help                 Print help information\n";
}

/**
 * @brief Interactively prompts the user with a Yes/No question on stdout.
 * @param question Text prompt to display.
 * @return True if user enters 'Y' or 'y', false if 'N' or 'n'.
 */
bool ask_yn(const std::string& question) {
    while (true) {
        std::cout << question << " [Y/N]: " << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) {
            return false;
        }
        if (line == "Y" || line == "y") return true;
        if (line == "N" || line == "n") return false;
    }
}

/**
 * @brief Resolves target output filepath, appending filename if the path points to a directory.
 * @param path User-specified output path.
 * @param filename Default filename to use if path is a directory.
 * @return Fully normalized destination path with .wav extension.
 */
fs::path normalize_output_path(const fs::path& path, const std::string& filename) {
    fs::path p = path;
    if (fs::is_directory(p)) {
        p /= filename;
        p.replace_extension(".wav");
    }
    return p;
}

/**
 * @brief Application entry point for volsa2-cli.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    std::chrono::milliseconds cooldown(10);
    int arg_idx = 1;

    // Parse global flags
    while (arg_idx < argc && argv[arg_idx][0] == '-') {
        std::string arg = argv[arg_idx];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "-c" || arg == "--cooldown") {
            if (arg_idx + 1 >= argc) {
                std::cerr << "Error: --cooldown requires a value in ms\n";
                return 1;
            }
            cooldown = std::chrono::milliseconds(std::stoi(argv[++arg_idx]));
        } else {
            break;
        }
        arg_idx++;
    }

    if (arg_idx >= argc) {
        print_help(argv[0]);
        return 1;
    }

    std::string cmd = argv[arg_idx++];

    try {
        volsa2::Device device(cooldown);

        if (cmd == "list" || cmd == "ls") {
            bool show_empty = false;
            for (; arg_idx < argc; ++arg_idx) {
                std::string a = argv[arg_idx];
                if (a == "-a" || a == "--show-empty") {
                    show_empty = true;
                }
            }

            std::cout << "Connecting to Volca Sample 2...\n";
            device.connect();
            std::cout << "Connected: Channel " << static_cast<int>(device.channel().as_u8())
                      << ", Firmware v" << device.version().to_string() << "\n";

            auto space = device.get_sample_space();
            std::cout << "Occupied space: " << std::fixed << std::setprecision(1)
                      << (space.occupied() * 100.0) << "%\n";

            int last_printed = -1;
            for (uint8_t i = 0; i < 200; ++i) {
                auto header = device.get_sample_header(i);
                if (header.is_empty()) {
                    if (show_empty) {
                        std::cout << std::setw(3) << static_cast<int>(i) << ": <EMPTY>\n";
                    }
                    continue;
                }

                if (show_empty) {
                    for (int empty_idx = last_printed + 1; empty_idx < i; ++empty_idx) {
                        std::cout << std::setw(3) << empty_idx << ": <EMPTY>\n";
                    }
                }
                last_printed = i;

                std::cout << std::setw(3) << static_cast<int>(header.sample_no) << ": "
                          << std::left << std::setw(24) << header.name << std::right
                          << " - length: " << std::setw(8) << header.length
                          << ", speed: " << std::setw(5) << header.speed
                          << ", level: " << std::setw(5) << header.level << "\n";
            }
        }
        else if (cmd == "download" || cmd == "dl") {
            if (arg_idx >= argc) {
                std::cerr << "Error: download requires <sample-no>\n";
                return 1;
            }
            int sample_no = std::stoi(argv[arg_idx++]);
            fs::path out_path = "./";

            for (; arg_idx < argc; ++arg_idx) {
                std::string a = argv[arg_idx];
                if ((a == "-o" || a == "--output") && arg_idx + 1 < argc) {
                    out_path = argv[++arg_idx];
                }
            }

            device.connect();
            auto header = device.get_sample_header(static_cast<uint8_t>(sample_no));
            std::cout << "Downloading sample \"" << header.name << "\" from Volca slot " << sample_no << "...\n";

            auto data = device.get_sample(static_cast<uint8_t>(sample_no));
            fs::path target_file = normalize_output_path(out_path, header.name.empty() ? ("sample_" + std::to_string(sample_no)) : header.name);
            volsa2::write_wav_file(target_file, data.data);
            std::cout << "Wrote sample to " << target_file << "\n";
        }
        else if (cmd == "upload" || cmd == "up") {
            if (arg_idx >= argc) {
                std::cerr << "Error: upload requires <path-to-sample>\n";
                return 1;
            }
            fs::path input_file = argv[arg_idx++];
            std::optional<uint8_t> target_slot;
            volsa2::MonoMode mono_mode = volsa2::MonoMode::Mid;
            std::optional<fs::path> out_converted;
            bool dry_run = false;

            while (arg_idx < argc) {
                std::string a = argv[arg_idx];
                if ((a == "-m" || a == "--mono-mode") && arg_idx + 1 < argc) {
                    mono_mode = volsa2::mono_mode_from_string(argv[++arg_idx]);
                } else if ((a == "-o" || a == "--output") && arg_idx + 1 < argc) {
                    out_converted = argv[++arg_idx];
                } else if (a == "--dry-run") {
                    dry_run = true;
                } else if (a[0] != '-' && !target_slot) {
                    target_slot = static_cast<uint8_t>(std::stoi(a));
                }
                arg_idx++;
            }

            std::string sample_name = input_file.stem().string();
            std::cout << "Loading and converting audio: " << input_file << " (mono mode: " << volsa2::to_string(mono_mode) << ")...\n";
            auto pcm_samples = volsa2::load_and_convert_audio(input_file, mono_mode);
            std::cout << "Converted to " << pcm_samples.size() << " samples @ 31.25 kHz\n";

            if (out_converted) {
                fs::path p = normalize_output_path(*out_converted, sample_name);
                volsa2::write_wav_file(p, pcm_samples);
                std::cout << "Wrote converted audio to " << p << "\n";
            }

            if (dry_run) {
                std::cout << "Dry run complete. Sample was not uploaded.\n";
                return 0;
            }

            device.connect();

            if (!target_slot) {
                std::cout << "Finding first empty slot...\n";
                for (uint8_t i = 0; i < 200; ++i) {
                    auto h = device.get_sample_header(i);
                    if (h.is_empty()) {
                        target_slot = i;
                        break;
                    }
                }
                if (!target_slot) {
                    std::cerr << "Error: no empty slots found on device\n";
                    return 1;
                }
            }

            auto current_header = device.get_sample_header(*target_slot);
            if (!current_header.is_empty()) {
                std::string q = "Sample slot " + std::to_string(*target_slot) +
                                " is not empty (current - \"" + current_header.name + "\"). Do you want to overwrite?";
                if (!ask_yn(q)) {
                    std::cout << "Upload cancelled.\n";
                    return 0;
                }

                if (ask_yn("Do you want to backup the loaded sample (\"" + current_header.name + "\")?")) {
                    auto existing_data = device.get_sample(*target_slot);
                    fs::path backup_file = normalize_output_path("./", current_header.name + "_backup");
                    volsa2::write_wav_file(backup_file, existing_data.data);
                    std::cout << "Wrote backup to " << backup_file << "\n";
                }
            }

            auto [header, data] = volsa2::SampleData::create(*target_slot, sample_name, std::move(pcm_samples));
            device.send_sample(header, data, [](size_t sent, size_t total) {
                if (total > 0) {
                    int pct = static_cast<int>((sent * 100) / total);
                    std::cout << "\rUploading: " << pct << "% [" << sent << "/" << total << " bytes]" << std::flush;
                }
            });
            std::cout << "\nLoaded sample \"" << header.name << "\" in slot " << static_cast<int>(*target_slot) << "\n";
        }
        else if (cmd == "remove" || cmd == "rm") {
            if (arg_idx >= argc) {
                std::cerr << "Error: remove requires <sample-no>\n";
                return 1;
            }
            uint8_t slot = static_cast<uint8_t>(std::stoi(argv[arg_idx++]));
            bool print_name = false;

            for (; arg_idx < argc; ++arg_idx) {
                std::string a = argv[arg_idx];
                if (a == "-p" || a == "--print-name") {
                    print_name = true;
                }
            }

            device.connect();
            std::string name_str;
            if (print_name) {
                auto h = device.get_sample_header(slot);
                if (h.is_empty()) {
                    std::cout << "Sample slot " << static_cast<int>(slot) << " is already empty\n";
                    return 0;
                }
                name_str = "\"" + h.name + "\" ";
            }

            device.delete_sample(slot);
            std::cout << "Removed sample " << name_str << "at slot " << static_cast<int>(slot) << "\n";
        }
        else if (cmd == "download-package" || cmd == "pkg-dl") {
            if (arg_idx >= argc) {
                std::cerr << "Error: download-package requires <output_file.ivlcsplpreset>\n";
                return 1;
            }
            fs::path out_file = argv[arg_idx++];
            if (out_file.extension() != ".ivlcsplpreset") {
                out_file.replace_extension(".ivlcsplpreset");
            }

            std::string preset_name = out_file.stem().string();
            std::string author_name = "";

            for (; arg_idx < argc; ++arg_idx) {
                std::string a = argv[arg_idx];
                if ((a == "-n" || a == "--name") && arg_idx + 1 < argc) {
                    preset_name = argv[++arg_idx];
                } else if ((a == "-a" || a == "--author") && arg_idx + 1 < argc) {
                    author_name = argv[++arg_idx];
                }
            }

            device.connect();
            std::cout << "Connected to Volca Sample 2 (Channel " << static_cast<int>(device.channel().as_u8())
                      << ", Firmware " << device.version().to_string() << ")\n";

            auto pkg = volsa2::PackageData::create_default(preset_name, author_name);

            // Step 1: Download 16 patterns
            std::cout << "[1/4] Downloading 16 patterns from device...\n";
            for (uint8_t i = 0; i < volsa2::PatternData::MAX_PATTERNS; ++i) {
                std::cout << "\r  Pattern " << static_cast<int>(i + 1) << "/16..." << std::flush;
                auto pat = device.get_pattern(i);
                pkg.programs[i].pattern = std::move(pat);
            }
            std::cout << "\r  Downloaded all 16 patterns successfully.    \n";

            // Step 2: Download 200 sample headers
            std::cout << "[2/4] Scanning 200 sample headers...\n";
            std::vector<uint8_t> non_empty_slots;
            for (uint8_t i = 0; i < 200; ++i) {
                if (i % 20 == 0 || i == 199) {
                    std::cout << "\r  Scanning slots " << static_cast<int>(i + 1) << "/200..." << std::flush;
                }
                auto h = device.get_sample_header(i);
                pkg.samples[i].header = h;
                if (!h.is_empty()) {
                    non_empty_slots.push_back(i);
                }
            }
            std::cout << "\r  Found " << non_empty_slots.size() << " active samples across 200 slots.      \n";

            // Step 3: Download audio data for non-empty samples
            std::cout << "[3/4] Downloading audio PCM data for " << non_empty_slots.size() << " samples...\n";
            for (size_t idx = 0; idx < non_empty_slots.size(); ++idx) {
                uint8_t slot = non_empty_slots[idx];
                std::cout << "\r  Downloading sample " << (idx + 1) << "/" << non_empty_slots.size()
                          << " (slot " << static_cast<int>(slot) << ": \"" << pkg.samples[slot].header.name << "\")..." << std::flush;
                auto sdata = device.get_sample(slot);
                pkg.samples[slot].data = std::move(sdata);
            }
            std::cout << "\r  Finished downloading audio for all active samples.                    \n";

            // Step 4: Write .ivlcsplpreset package
            std::cout << "[4/4] Packing into " << out_file << "...\n";
            volsa2::save_package(out_file, pkg);
            std::cout << "Done! Package saved successfully (" << fs::file_size(out_file) << " bytes).\n";
        }
        else if (cmd == "upload-package" || cmd == "pkg-up") {
            if (arg_idx >= argc) {
                std::cerr << "Error: upload-package requires <file.ivlcsplpreset>\n";
                return 1;
            }
            fs::path in_file = argv[arg_idx++];
            if (!fs::exists(in_file)) {
                std::cerr << "Error: Package file not found: " << in_file << "\n";
                return 1;
            }

            bool samples_only = false;
            bool patterns_only = false;
            bool clear_empty = true; // Clean restore by default to prevent filling device memory
            bool dry_run = false;
            bool auto_yes = false;

            for (; arg_idx < argc; ++arg_idx) {
                std::string a = argv[arg_idx];
                if (a == "-y" || a == "--yes") {
                    auto_yes = true;
                } else if (a == "--samples-only") {
                    samples_only = true;
                } else if (a == "--patterns-only") {
                    patterns_only = true;
                } else if (a == "--clear-empty" || a == "--erase-empty") {
                    clear_empty = true;
                } else if (a == "--keep-existing") {
                    clear_empty = false;
                } else if (a == "--dry-run") {
                    dry_run = true;
                } else {
                    std::cerr << "Unknown option for upload-package: " << a << "\n";
                    return 1;
                }
            }

            if (samples_only && patterns_only) {
                std::cerr << "Error: Cannot specify both --samples-only and --patterns-only\n";
                return 1;
            }

            std::cout << "Loading package: " << in_file << "...\n";
            auto pkg = volsa2::load_package(in_file);

            std::vector<uint8_t> active_slots;
            std::vector<bool> is_pkg_active(200, false);
            for (uint8_t i = 0; i < 200; ++i) {
                if (pkg.samples[i].data.has_value() && !pkg.samples[i].data->data.empty()) {
                    active_slots.push_back(i);
                    is_pkg_active[i] = true;
                }
            }

            std::cout << "Package Information:\n"
                      << "  Name:           " << (pkg.info.name.empty() ? "(none)" : pkg.info.name) << "\n"
                      << "  Author:         " << (pkg.info.author.empty() ? "(none)" : pkg.info.author) << "\n"
                      << "  Date:           " << (pkg.info.date.empty() ? "(none)" : pkg.info.date) << "\n"
                      << "  Patterns:       " << pkg.programs.size() << " sequencer patterns\n"
                      << "  Active Samples: " << active_slots.size() << " of 200 slots\n\n";

            if (dry_run) {
                std::cout << "--- Dry Run Summary ---\n";
                if (!samples_only) {
                    std::cout << "Patterns to upload (" << pkg.programs.size() << "):\n";
                    for (size_t i = 0; i < pkg.programs.size(); ++i) {
                        std::cout << "  Pattern " << std::setw(2) << (i + 1)
                                  << " (slot " << std::setw(2) << i << "): \""
                                  << pkg.programs[i].pattern.name << "\"\n";
                    }
                }
                if (!patterns_only) {
                    std::cout << "Samples to upload (" << active_slots.size() << "):\n";
                    for (uint8_t slot : active_slots) {
                        const auto& h = pkg.samples[slot].header;
                        std::cout << "  Slot " << std::setw(3) << static_cast<int>(slot) << ": "
                                  << std::left << std::setw(24) << (h.name.empty() ? "<unnamed>" : h.name) << std::right
                                  << " - " << std::setw(8) << h.length << " samples ("
                                  << std::fixed << std::setprecision(2) << (h.length / 31250.0) << "s), "
                                  << "speed: " << h.speed << ", level: " << h.level << "\n";
                    }
                    if (clear_empty) {
                        std::cout << "Clean restore: will scan and erase old unused slots before uploading.\n";
                    }
                }
                std::cout << "Dry run completed. No data sent to device.\n";
                return 0;
            }

            if (!auto_yes) {
                std::string warning = "WARNING: Uploading this package will overwrite ";
                if (samples_only) warning += "samples";
                else if (patterns_only) warning += "patterns";
                else warning += "samples and patterns";
                warning += " on your Volca Sample 2. Continue?";
                if (!ask_yn(warning)) {
                    std::cout << "Operation aborted.\n";
                    return 0;
                }
            }

            device.connect();
            std::cout << "Connected to Volca Sample 2 (Channel " << static_cast<int>(device.channel().as_u8())
                      << ", Firmware " << device.version().to_string() << ")\n";

            // Upload Samples
            if (!patterns_only) {
                // Free memory FIRST: erase occupied slots on device that are empty in the package
                if (clear_empty) {
                    std::cout << "\nScanning device memory to identify unused slots to free...\n";
                    std::vector<uint8_t> slots_to_erase;
                    for (uint8_t i = 0; i < 200; ++i) {
                        if (!is_pkg_active[i]) {
                            auto h = device.get_sample_header(i);
                            if (!h.is_empty()) {
                                slots_to_erase.push_back(i);
                            }
                        }
                    }

                    if (!slots_to_erase.empty()) {
                        std::cout << "Freeing memory: erasing " << slots_to_erase.size() << " old occupied slots...\n";
                        for (size_t idx = 0; idx < slots_to_erase.size(); ++idx) {
                            uint8_t slot = slots_to_erase[idx];
                            std::cout << "\r  Erasing old slot " << static_cast<int>(slot)
                                      << " (" << (idx + 1) << "/" << slots_to_erase.size() << ")..." << std::flush;
                            device.delete_sample(slot);
                        }
                        std::cout << "\r  Memory freed: erased " << slots_to_erase.size() << " old slots successfully.          \n";
                    }
                }

                std::cout << "\n[1/2] Uploading " << active_slots.size() << " active samples to device...\n";
                for (size_t idx = 0; idx < active_slots.size(); ++idx) {
                    uint8_t slot = active_slots[idx];
                    const auto& s = pkg.samples[slot];
                    std::cout << "\r  [" << (idx + 1) << "/" << active_slots.size() << "] Slot "
                              << static_cast<int>(slot) << ": \"" << s.header.name << "\" ("
                              << s.header.length << " samples)..." << std::flush;
                    device.send_sample(s.header, *s.data);
                }
                std::cout << "\r  Finished uploading all " << active_slots.size() << " samples.               \n";
            }

            // Upload Patterns
            if (!samples_only) {
                int total_pats = static_cast<int>(pkg.programs.size());
                std::cout << "\n[2/2] Uploading " << total_pats << " sequencer patterns to device...\n";
                for (size_t i = 0; i < pkg.programs.size(); ++i) {
                    const auto& pat = pkg.programs[i].pattern;
                    std::cout << "\r  [" << (i + 1) << "/" << total_pats << "] Pattern "
                              << static_cast<int>(i + 1) << " (slot " << static_cast<int>(pat.pattern_no)
                              << "): \"" << pat.name << "\"..." << std::flush;
                    device.send_pattern(pat);
                }
                std::cout << "\r  Finished uploading all " << total_pats << " patterns.                      \n";
            }

            std::cout << "\nSuccess! Package restored to Volca Sample 2.\n";
        }
        else {
            std::cerr << "Unknown command: " << cmd << "\n";
            print_help(argv[0]);
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

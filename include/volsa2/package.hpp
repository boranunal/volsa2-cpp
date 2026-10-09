/**
 * @file package.hpp
 * @brief Volca Sample 2 sound librarian package (.ivlcsplpreset) container support.
 * @details Handles serialization and deserialization of the official KORG .ivlcsplpreset
 *          ZIP format, bundling 16 pattern sequences, 200 sample metadata descriptors,
 *          raw PCM audio binaries, XML catalogs, and MD5 integrity verification.
 * @author Volsa2 Project Team
 * @date 2026
 */

#pragma once

#include "volsa2/proto.hpp"
#include <string>
#include <vector>
#include <optional>
#include <span>
#include <filesystem>

namespace volsa2 {

/**
 * @struct PackagePresetInfo
 * @brief Top-level metadata descriptor for an .ivlcsplpreset library package.
 */
struct PackagePresetInfo {
    std::string data_id{"Factory Presets"}; ///< Unique dataset identifier.
    std::string name{"Preset"};             ///< Human-readable preset collection name.
    std::string author;                     ///< Author or creator name.
    std::string version{"1"};               ///< Library schema version (default "1").
    std::string date;                       ///< Creation timestamp (e.g. "MM-DD-YYYY").
    std::string prefix;                     ///< Optional slot prefix.
    std::string copyright;                  ///< Optional copyright notice.
};

/**
 * @struct PackageSampleSlot
 * @brief Represents a single sample slot within a package (0-199).
 */
struct PackageSampleSlot {
    SampleHeader header;
    std::string author;
    std::string comment;
    std::optional<SampleData> data; ///< Non-empty if slot has audio data.
};

/**
 * @struct PackageProgramSlot
 * @brief Represents a single pattern / program slot within a package (0-15).
 */
struct PackageProgramSlot {
    PatternData pattern;
    std::string comment;
};

/**
 * @struct PackageData
 * @brief Complete representation of a Volca Sample 2 preset package.
 */
struct PackageData {
    PackagePresetInfo info;
    std::vector<PackageProgramSlot> programs; ///< Exactly 16 program slots (0-15).
    std::vector<PackageSampleSlot> samples;   ///< Exactly 200 sample slots (0-199).

    /**
     * @brief Creates a default PackageData initialized with 16 empty patterns and 200 empty samples.
     * @param name Name of the preset package.
     * @param author Author of the package.
     * @return Initialized PackageData.
     */
    static PackageData create_default(const std::string& name = "My Presets", const std::string& author = "");
};

/**
 * @brief Computes standard lowercase hexadecimal MD5 digest of a byte buffer.
 * @param data Byte view to hash.
 * @return 32-character lowercase hex string.
 */
std::string compute_md5_hex(std::span<const uint8_t> data);

/**
 * @brief Saves a PackageData structure to disk as a compliant .ivlcsplpreset ZIP archive.
 * @param path Destination file path.
 * @param package The package data to write.
 * @throws std::runtime_error on I/O or archive creation failure.
 */
void save_package(const std::filesystem::path& path, const PackageData& package);

/**
 * @brief Loads and parses an .ivlcsplpreset ZIP archive from disk.
 * @param path Path to .ivlcsplpreset file.
 * @return Parsed PackageData structure.
 * @throws std::runtime_error on format or decompression failure.
 */
PackageData load_package(const std::filesystem::path& path);

} // namespace volsa2

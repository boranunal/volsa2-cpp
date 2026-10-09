/**
 * @file package.cpp
 * @brief Implementation of Volca Sample 2 package (.ivlcsplpreset) container support.
 * @details Reads and writes ZIP archives containing KORG Sound Librarian presets.
 * @date 2026
 */

#include "volsa2/package.hpp"

#include <zip.h>
#include <openssl/evp.h>

#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <regex>

namespace volsa2 {

namespace {

std::string xml_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '\"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c; break;
        }
    }
    return out;
}

std::string get_current_date_str() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%m-%d-%Y", &tm_buf);
    return std::string(buf);
}

std::string extract_xml_tag(const std::string& xml, const std::string& tag) {
    std::string open_tag = "<" + tag + ">";
    std::string close_tag = "</" + tag + ">";
    auto p1 = xml.find(open_tag);
    if (p1 == std::string::npos) return "";
    p1 += open_tag.size();
    auto p2 = xml.find(close_tag, p1);
    if (p2 == std::string::npos) return "";
    return xml.substr(p1, p2 - p1);
}

void add_zip_buffer(zip_t* za, const std::string& entry_name, std::vector<uint8_t> buffer) {
    void* heap_buf = std::malloc(buffer.empty() ? 1 : buffer.size());
    if (!heap_buf) {
        throw std::bad_alloc();
    }
    if (!buffer.empty()) {
        std::memcpy(heap_buf, buffer.data(), buffer.size());
    }
    zip_source_t* src = zip_source_buffer(za, heap_buf, buffer.size(), 1); // 1: zip frees heap_buf
    if (!src) {
        std::free(heap_buf);
        throw std::runtime_error("add_zip_buffer: failed to create zip source for " + entry_name);
    }
    if (zip_file_add(za, entry_name.c_str(), src, ZIP_FL_OVERWRITE) < 0) {
        zip_source_free(src);
        throw std::runtime_error("add_zip_buffer: failed to add " + entry_name + " to archive");
    }
}

void add_zip_string(zip_t* za, const std::string& entry_name, const std::string& text) {
    std::vector<uint8_t> buf(text.begin(), text.end());
    add_zip_buffer(za, entry_name, std::move(buf));
}

std::vector<uint8_t> read_zip_entry(zip_t* za, const std::string& name) {
    zip_stat_t st;
    zip_stat_init(&st);
    if (zip_stat(za, name.c_str(), 0, &st) < 0) {
        throw std::runtime_error("read_zip_entry: entry not found: " + name);
    }
    zip_file_t* zf = zip_fopen(za, name.c_str(), 0);
    if (!zf) {
        throw std::runtime_error("read_zip_entry: failed to open " + name);
    }
    std::vector<uint8_t> data(st.size);
    zip_int64_t bytes_read = zip_fread(zf, data.data(), st.size);
    zip_fclose(zf);
    if (bytes_read < 0 || static_cast<zip_uint64_t>(bytes_read) != st.size) {
        throw std::runtime_error("read_zip_entry: failed to read complete entry: " + name);
    }
    return data;
}

bool zip_has_entry(zip_t* za, const std::string& name) {
    zip_stat_t st;
    zip_stat_init(&st);
    return zip_stat(za, name.c_str(), 0, &st) == 0;
}

} // anonymous namespace

std::string compute_md5_hex(std::span<const uint8_t> data) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw std::runtime_error("compute_md5_hex: failed to allocate EVP_MD_CTX");
    }
    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
    if (!data.empty()) {
        EVP_DigestUpdate(ctx, data.data(), data.size());
    }
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < hash_len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return oss.str();
}

PackageData PackageData::create_default(const std::string& name, const std::string& author) {
    PackageData pkg;
    pkg.info.name = name;
    pkg.info.data_id = name;
    pkg.info.author = author;
    pkg.info.date = get_current_date_str();

    pkg.programs.reserve(16);
    for (uint8_t i = 0; i < 16; ++i) {
        PackageProgramSlot prog;
        prog.pattern = PatternData::create(i, "Pattern " + std::to_string(i + 1));
        pkg.programs.push_back(std::move(prog));
    }

    pkg.samples.reserve(200);
    for (uint8_t i = 0; i < 200; ++i) {
        PackageSampleSlot smpl;
        smpl.header = SampleHeader::empty(i);
        pkg.samples.push_back(std::move(smpl));
    }

    return pkg;
}

void save_package(const std::filesystem::path& path, const PackageData& package) {
    int errorp = 0;
    zip_t* za = zip_open(path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errorp);
    if (!za) {
        zip_error_t zerr;
        zip_error_init_with_code(&zerr, errorp);
        std::string err_msg = zip_error_strerror(&zerr);
        zip_error_fini(&zerr);
        throw std::runtime_error("save_package: cannot create zip file " + path.string() + ": " + err_msg);
    }

    try {
        // 1. FileInformation.xml
        std::ostringstream file_info_xml;
        file_info_xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n"
                      << "<KorgMSLibrarian_Data>\n"
                      << "  <Product>volca sample 2</Product>\n"
                      << "  <Contents NumProgramData=\"16\" NumSampleData=\"200\" NumPresetInformation=\"1\">\n"
                      << "    <PresetInformation>\n"
                      << "      <File>PresetInformation.xml</File>\n"
                      << "    </PresetInformation>\n";

        for (int i = 0; i < 16; ++i) {
            char p_info_name[32];
            char p_bin_name[32];
            std::snprintf(p_info_name, sizeof(p_info_name), "Prog_%03d.prog_info", i);
            std::snprintf(p_bin_name, sizeof(p_bin_name), "Prog_%03d.prog_bin", i);

            file_info_xml << "    <ProgramData>\n"
                          << "      <Information>" << p_info_name << "</Information>\n"
                          << "      <ProgramBinary>" << p_bin_name << "</ProgramBinary>\n"
                          << "    </ProgramData>\n";
        }

        for (int i = 0; i < 200; ++i) {
            char s_info_name[32];
            char s_bin_name[32];
            std::snprintf(s_info_name, sizeof(s_info_name), "Smpl_%03d.smpl_info", i);
            std::snprintf(s_bin_name, sizeof(s_bin_name), "Smpl_%03d.smpl_bin", i);

            bool has_bin = (i < static_cast<int>(package.samples.size())) &&
                           package.samples[i].data.has_value() &&
                           !package.samples[i].data->data.empty();

            file_info_xml << "    <SampleData>\n"
                          << "      <Information>" << s_info_name << "</Information>\n";
            if (has_bin) {
                file_info_xml << "      <SampleBinary>" << s_bin_name << "</SampleBinary>\n";
            }
            file_info_xml << "    </SampleData>\n";
        }

        file_info_xml << "  </Contents>\n"
                      << "</KorgMSLibrarian_Data>\n";
        add_zip_string(za, "FileInformation.xml", file_info_xml.str());

        // 2. PresetInformation.xml
        std::string date_str = package.info.date.empty() ? get_current_date_str() : package.info.date;
        std::ostringstream preset_info_xml;
        preset_info_xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n"
                        << "<vlcspl2_Preset>\n"
                        << "  <DataID>" << xml_escape(package.info.data_id) << "</DataID>\n"
                        << "  <Name>" << xml_escape(package.info.name) << "</Name>\n"
                        << "  <Author>" << xml_escape(package.info.author) << "</Author>\n"
                        << "  <Version>" << xml_escape(package.info.version) << "</Version>\n"
                        << "  <NumPrograms>16</NumPrograms>\n"
                        << "  <NumSamples>200</NumSamples>\n"
                        << "  <Date>" << xml_escape(date_str) << "</Date>\n"
                        << "  <Prefix>" << xml_escape(package.info.prefix) << "</Prefix>\n"
                        << "  <Copyright>" << xml_escape(package.info.copyright) << "</Copyright>\n"
                        << "</vlcspl2_Preset>\n";
        add_zip_string(za, "PresetInformation.xml", preset_info_xml.str());

        // 3. 16 Programs
        for (int i = 0; i < 16; ++i) {
            char p_info_name[32];
            char p_bin_name[32];
            std::snprintf(p_info_name, sizeof(p_info_name), "Prog_%03d.prog_info", i);
            std::snprintf(p_bin_name, sizeof(p_bin_name), "Prog_%03d.prog_bin", i);

            PatternData pattern;
            std::string comment;
            if (i < static_cast<int>(package.programs.size())) {
                pattern = package.programs[i].pattern;
                comment = package.programs[i].comment;
            } else {
                pattern = PatternData::create(static_cast<uint8_t>(i), "Init Pattern");
            }

            if (pattern.raw_data.size() != PatternData::RAW_PATTERN_SIZE) {
                pattern.raw_data.resize(PatternData::RAW_PATTERN_SIZE, 0);
            }

            std::string md5 = compute_md5_hex(pattern.raw_data);
            add_zip_buffer(za, p_bin_name, pattern.raw_data);

            std::ostringstream p_info_xml;
            p_info_xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n"
                       << "<ProgramInformation>\n"
                       << "  <Name>" << xml_escape(pattern.name) << "</Name>\n"
                       << "  <Comment>" << xml_escape(comment) << "</Comment>\n"
                       << "  <MD5>" << md5 << "</MD5>\n"
                       << "</ProgramInformation>\n";
            add_zip_string(za, p_info_name, p_info_xml.str());
        }

        // 4. 200 Samples
        for (int i = 0; i < 200; ++i) {
            char s_info_name[32];
            char s_bin_name[32];
            std::snprintf(s_info_name, sizeof(s_info_name), "Smpl_%03d.smpl_info", i);
            std::snprintf(s_bin_name, sizeof(s_bin_name), "Smpl_%03d.smpl_bin", i);

            SampleHeader header = SampleHeader::empty(static_cast<uint8_t>(i));
            std::string author;
            std::string comment;
            std::optional<SampleData> sdata;

            if (i < static_cast<int>(package.samples.size())) {
                header = package.samples[i].header;
                author = package.samples[i].author;
                comment = package.samples[i].comment;
                sdata = package.samples[i].data;
            }

            std::string md5;
            if (sdata && !sdata->data.empty()) {
                // Convert int16 samples to raw little-endian bytes
                std::vector<uint8_t> raw_pcm(sdata->data.size() * 2);
                for (size_t s = 0; s < sdata->data.size(); ++s) {
                    uint16_t val = static_cast<uint16_t>(sdata->data[s]);
                    raw_pcm[s * 2] = static_cast<uint8_t>(val & 0xFF);
                    raw_pcm[s * 2 + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
                }
                md5 = compute_md5_hex(raw_pcm);
                add_zip_buffer(za, s_bin_name, std::move(raw_pcm));
            }

            std::ostringstream s_info_xml;
            s_info_xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n"
                       << "<SampleInformation>\n"
                       << "  <Name>" << xml_escape(header.name) << "</Name>\n"
                       << "  <Author>" << xml_escape(author) << "</Author>\n"
                       << "  <Comment>" << xml_escape(comment) << "</Comment>\n"
                       << "  <MD5>" << md5 << "</MD5>\n"
                       << "  <Length>" << header.length << "</Length>\n"
                       << "  <Level>" << (header.level == 0 && header.is_empty() ? 65535 : header.level) << "</Level>\n"
                       << "  <Speed>" << (header.speed == 0 && header.is_empty() ? 16384 : header.speed) << "</Speed>\n"
                       << "</SampleInformation>\n";
            add_zip_string(za, s_info_name, s_info_xml.str());
        }

        if (zip_close(za) < 0) {
            std::string err_str = zip_strerror(za);
            zip_discard(za);
            throw std::runtime_error("save_package: failed to finalize zip archive: " + err_str);
        }
    } catch (...) {
        zip_discard(za);
        throw;
    }
}

PackageData load_package(const std::filesystem::path& path) {
    int errorp = 0;
    zip_t* za = zip_open(path.string().c_str(), ZIP_RDONLY, &errorp);
    if (!za) {
        zip_error_t zerr;
        zip_error_init_with_code(&zerr, errorp);
        std::string err_msg = zip_error_strerror(&zerr);
        zip_error_fini(&zerr);
        throw std::runtime_error("load_package: cannot open zip file " + path.string() + ": " + err_msg);
    }

    try {
        PackageData pkg;

        // 1. Read PresetInformation.xml
        if (zip_has_entry(za, "PresetInformation.xml")) {
            auto buf = read_zip_entry(za, "PresetInformation.xml");
            std::string xml(reinterpret_cast<const char*>(buf.data()), buf.size());
            pkg.info.data_id = extract_xml_tag(xml, "DataID");
            pkg.info.name = extract_xml_tag(xml, "Name");
            pkg.info.author = extract_xml_tag(xml, "Author");
            pkg.info.version = extract_xml_tag(xml, "Version");
            pkg.info.date = extract_xml_tag(xml, "Date");
            pkg.info.prefix = extract_xml_tag(xml, "Prefix");
            pkg.info.copyright = extract_xml_tag(xml, "Copyright");
        }

        // 2. Read 16 Programs
        pkg.programs.reserve(16);
        for (int i = 0; i < 16; ++i) {
            char p_info_name[32];
            char p_bin_name[32];
            std::snprintf(p_info_name, sizeof(p_info_name), "Prog_%03d.prog_info", i);
            std::snprintf(p_bin_name, sizeof(p_bin_name), "Prog_%03d.prog_bin", i);

            PackageProgramSlot slot;
            slot.pattern.pattern_no = static_cast<uint8_t>(i);

            if (zip_has_entry(za, p_info_name)) {
                auto buf = read_zip_entry(za, p_info_name);
                std::string xml(reinterpret_cast<const char*>(buf.data()), buf.size());
                slot.pattern.name = extract_xml_tag(xml, "Name");
                slot.comment = extract_xml_tag(xml, "Comment");
            }

            if (zip_has_entry(za, p_bin_name)) {
                slot.pattern.raw_data = read_zip_entry(za, p_bin_name);
                if (slot.pattern.raw_data.size() != PatternData::RAW_PATTERN_SIZE) {
                    slot.pattern.raw_data.resize(PatternData::RAW_PATTERN_SIZE, 0);
                }
            } else {
                slot.pattern = PatternData::create(static_cast<uint8_t>(i), slot.pattern.name);
            }

            pkg.programs.push_back(std::move(slot));
        }

        // 3. Read 200 Samples
        pkg.samples.reserve(200);
        for (int i = 0; i < 200; ++i) {
            char s_info_name[32];
            char s_bin_name[32];
            std::snprintf(s_info_name, sizeof(s_info_name), "Smpl_%03d.smpl_info", i);
            std::snprintf(s_bin_name, sizeof(s_bin_name), "Smpl_%03d.smpl_bin", i);

            PackageSampleSlot slot;
            slot.header.sample_no = static_cast<uint8_t>(i);

            if (zip_has_entry(za, s_info_name)) {
                auto buf = read_zip_entry(za, s_info_name);
                std::string xml(reinterpret_cast<const char*>(buf.data()), buf.size());
                slot.header.name = extract_xml_tag(xml, "Name");
                slot.author = extract_xml_tag(xml, "Author");
                slot.comment = extract_xml_tag(xml, "Comment");
                std::string len_str = extract_xml_tag(xml, "Length");
                std::string lvl_str = extract_xml_tag(xml, "Level");
                std::string spd_str = extract_xml_tag(xml, "Speed");

                if (!len_str.empty()) slot.header.length = static_cast<uint32_t>(std::stoul(len_str));
                if (!lvl_str.empty()) slot.header.level = static_cast<uint16_t>(std::stoul(lvl_str));
                if (!spd_str.empty()) slot.header.speed = static_cast<uint16_t>(std::stoul(spd_str));
            }

            if (zip_has_entry(za, s_bin_name)) {
                auto raw_pcm = read_zip_entry(za, s_bin_name);
                SampleData sd;
                sd.sample_no = static_cast<uint8_t>(i);
                sd.data.reserve(raw_pcm.size() / 2);
                for (size_t s = 0; s + 1 < raw_pcm.size(); s += 2) {
                    uint16_t val = static_cast<uint16_t>(raw_pcm[s]) |
                                   (static_cast<uint16_t>(raw_pcm[s + 1]) << 8);
                    sd.data.push_back(static_cast<int16_t>(val));
                }
                slot.data = std::move(sd);
            }

            pkg.samples.push_back(std::move(slot));
        }

        zip_close(za);
        return pkg;
    } catch (...) {
        zip_close(za);
        throw;
    }
}

} // namespace volsa2

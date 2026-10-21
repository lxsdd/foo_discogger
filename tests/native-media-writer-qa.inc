// RUNNER-ONLY QA. Built ONLY when MSVC CL sets DISCOGGER_RUNNER_MEDIA_QA.
// Never present in any ordinary user build or distributed DLL.
#include "file_info_manager.h"
#include "discogger_async_write_lease.h"
#include "SDK/menu.h"
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {
static const GUID qa_mp3_command = {0x4a1d5a11,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_flac_command = {0x4a1d5a12,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_targets_command = {0x4a1d5a13,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};

void run_disposable_media_writer_qa(t_uint32 format) {
    const char* github = std::getenv("GITHUB_ACTIONS");
    const char* runner = std::getenv("RUNNER_TEMP");
    const char* directory = std::getenv("DISCOGGER_QA_MEDIA_DIR");
    if (!github || std::string(github) != "true" || !runner || !directory)
        throw std::runtime_error("Not an authorized ephemeral GitHub runner.");
    const auto root = std::filesystem::weakly_canonical(std::filesystem::u8path(runner));
    const auto folder = std::filesystem::weakly_canonical(std::filesystem::u8path(directory));
    const auto prefix = root.native() + std::filesystem::path::preferred_separator;
    if (folder.native().size() <= prefix.size() ||
        _wcsnicmp(folder.c_str(), prefix.c_str(), prefix.size()) != 0 ||
        !std::filesystem::is_directory(folder))
        throw std::runtime_error("Media test directory lies outside runner temp.");
    const char* filename = format == 0 ? "synthetic-metadata.mp3" : "synthetic-embedded.flac";
    const auto media = folder / filename;
    if (!std::filesystem::is_regular_file(media) || std::filesystem::is_symlink(media))
        throw std::runtime_error("QA target is missing or a symlink.");
    // Convert the canonical, newly-created disposable target to foobar path.
    const auto utf8_units = media.u8string();
    // MSVC's C++20 char8_t does not implicitly convert to std::string.
    // Retain UTF-8 code units verbatim for foobar's playable-location path.
    const std::string utf8path(utf8_units.begin(), utf8_units.end());
    const std::string logical = "file://" + utf8path;
    metadb_handle_ptr handle = metadb::get()->handle_create(logical.c_str(), 0);
    if (!handle || handle->get_subsong_index() != 0)
        throw std::runtime_error("QA could not obtain physical track handle.");
    metadb_handle_list list;
    list.add_item(handle);
    auto manager = std::make_shared<file_info_manager>(list);
    if (!manager->read_infos())
        throw std::runtime_error("QA complete physical metadata read failed.");
    manager->invalidate_all();
    auto& info = manager->get_item(0);
    const size_t oldCount = info.meta_get_count_by_name("DISCOGGER_QA_RUNNER");
    if (!discogger_write_safety::unchanged_single_value(
            oldCount, oldCount ? info.meta_get("DISCOGGER_QA_RUNNER", 0) : nullptr,
            "RUNNER-VERIFIED")) {
        info.meta_set("DISCOGGER_QA_RUNNER", "RUNNER-VERIFIED");
        manager->validate_item(0);
    }
    auto lease = discogger_write_safety::acquire_physical_write_lease();
    if (!lease) throw std::runtime_error("QA physical writer lease unavailable.");
    // A write is not accepted merely because foobar's command launcher
    // returned. Require completion callback evidence, once per invocation.
    const auto completion = folder /
        (format == 0 ? "qa-mp3-completed.txt" : "qa-flac-completed.txt");
    int previous = 0;
    if (std::filesystem::exists(completion)) {
        std::ifstream prior(completion);
        if (!(prior >> previous) || previous != 1)
            throw std::runtime_error("QA completion marker is stale or invalid.");
    }
    const int sequence = previous + 1;
    manager->write_infos([completion, sequence]() {
        std::ofstream output(completion, std::ios::trunc);
        if (!output) throw std::runtime_error("QA completion marker cannot be written.");
        output << sequence << '\n';
        output.flush();
        if (!output) throw std::runtime_error("QA completion marker could not be flushed.");
    }, std::move(lease), false);
}

void run_disposable_target_rejection_qa() {
    const char* github = std::getenv("GITHUB_ACTIONS");
    const char* runner = std::getenv("RUNNER_TEMP");
    const char* location = std::getenv("DISCOGGER_QA_MEDIA_DIR");
    if (!github || std::string(github) != "true" || !runner || !location)
        throw std::runtime_error("Unsafe runner environment for target guard QA.");
    const auto root = std::filesystem::weakly_canonical(std::filesystem::u8path(runner));
    const auto folder = std::filesystem::weakly_canonical(std::filesystem::u8path(location));
    const auto prefix = root.native() + std::filesystem::path::preferred_separator;
    if (folder.native().size() <= prefix.size() ||
        _wcsnicmp(folder.c_str(), prefix.c_str(), prefix.size()) != 0 ||
        !std::filesystem::is_directory(folder))
        throw std::runtime_error("Target-guard QA folder lies outside ephemeral runner.");
    const auto mp3 = folder / "synthetic-metadata.mp3";
    const auto flac = folder / "synthetic-embedded.flac";
    if (!std::filesystem::is_regular_file(mp3) || std::filesystem::is_symlink(mp3) ||
        !std::filesystem::is_regular_file(flac) || std::filesystem::is_symlink(flac))
        throw std::runtime_error("Synthetic guard fixtures missing or symbolic links.");

    const auto cueUnits = flac.u8string();
    const std::string cueUtf8(cueUnits.begin(), cueUnits.end());
    const std::string cueUrl = std::string("file://") + cueUtf8;
    metadb_handle_ptr virtualTrack = metadb::get()->handle_create(cueUrl.c_str(), 1);
    if (!virtualTrack || virtualTrack->get_subsong_index() != 1)
        throw std::runtime_error("Could not construct real virtual CUE host handle.");
    metadb_handle_list oneVirtual;
    oneVirtual.add_item(virtualTrack);
    bool deniedVirtual = false;
    try {
        auto target = std::make_shared<file_info_manager>(oneVirtual);
        auto lease = discogger_write_safety::acquire_physical_write_lease();
        if (!lease) throw std::runtime_error("Runner QA write lease unavailable.");
        target->write_infos({}, std::move(lease), false);
    } catch (const foo_discogs_exception&) { deniedVirtual = true; }
    if (!deniedVirtual) throw std::runtime_error("Virtual CUE selection reached physical writer.");

    const auto mp3Units = mp3.u8string();
    const std::string mp3Utf8(mp3Units.begin(), mp3Units.end());
    const std::string mp3Url = std::string("file://") + mp3Utf8;
    metadb_handle_ptr physicalTrack = metadb::get()->handle_create(mp3Url.c_str(), 0);
    if (!physicalTrack || physicalTrack->get_subsong_index() != 0)
        throw std::runtime_error("Could not construct real physical MP3 handle.");
    metadb_handle_list duplicates;
    duplicates.add_item(physicalTrack);
    duplicates.add_item(physicalTrack);
    bool deniedDuplicate = false;
    try {
        auto target = std::make_shared<file_info_manager>(duplicates);
        auto lease = discogger_write_safety::acquire_physical_write_lease();
        if (!lease) throw std::runtime_error("Runner QA write lease unavailable.");
        target->write_infos({}, std::move(lease), false);
    } catch (const foo_discogs_exception&) { deniedDuplicate = true; }
    if (!deniedDuplicate) throw std::runtime_error("Duplicate physical path reached metadata writer.");

    const auto marker = folder / "qa-unsafe-targets-rejected.txt";
    if (std::filesystem::exists(marker))
        throw std::runtime_error("Unsafe target rejection evidence already exists.");
    std::ofstream output(marker);
    if (!output) throw std::runtime_error("Could not open native guard evidence.");
    output << "VIRTUAL=REJECTED\nDUPLICATE=REJECTED\n";
    output.flush();
    if (!output) throw std::runtime_error("Could not flush native guard evidence.");
}

class discogger_runner_media_commands : public mainmenu_commands {
public:
    t_uint32 get_command_count() override { return 3; }
    GUID get_command(t_uint32 index) override {
        return index == 0 ? qa_mp3_command : (index == 1 ? qa_flac_command : qa_targets_command);
    }
    void get_name(t_uint32 index, pfc::string_base& out) override {
        out = index == 0 ? "Discogger QA MP3 write" :
            (index == 1 ? "Discogger QA FLAC write" : "Discogger QA unsafe targets");
    }
    bool get_description(t_uint32, pfc::string_base& out) override {
        out = "Test-only physical metadata writer for disposable GitHub runner media.";
        return true;
    }
    GUID get_parent() override { return mainmenu_groups::file; }
    void execute(t_uint32 index, service_ptr_t<service_base>) override {
        try {
            if (index > 2) throw std::runtime_error("Invalid synthetic QA action.");
            if (index == 2) run_disposable_target_rejection_qa();
            else run_disposable_media_writer_qa(index);
        } catch (const std::exception& e) {
            console::printf("DISCOGGER_RUNNER_MEDIA_QA_ERROR: %s", e.what());
        } catch (...) {
            console::print("DISCOGGER_RUNNER_MEDIA_QA_ERROR: unknown failure");
        }
    }
};
static mainmenu_commands_factory_t<discogger_runner_media_commands> runner_media_factory;
} // namespace

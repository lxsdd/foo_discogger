// RUNNER-ONLY QA. Built ONLY when MSVC CL sets DISCOGGER_RUNNER_MEDIA_QA.
// Never present in any ordinary user build or distributed DLL.
#include "file_info_manager.h"
#include "discogger_async_write_lease.h"
#include "SDK/mainmenu.h"
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {
static const GUID qa_mp3_command = {0x4a1d5a11,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_flac_command = {0x4a1d5a12,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};

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
    const auto utf8path = media.u8string();
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
    info.meta_set("DISCOGGER_QA_RUNNER", "RUNNER-VERIFIED");
    manager->validate_item(0);
    auto lease = discogger_write_safety::acquire_physical_write_lease();
    if (!lease) throw std::runtime_error("QA physical writer lease unavailable.");
    manager->write_infos({}, std::move(lease), false);
}

class discogger_runner_media_commands : public mainmenu_commands {
public:
    t_uint32 get_command_count() override { return 2; }
    GUID get_command(t_uint32 index) override {
        return index == 0 ? qa_mp3_command : qa_flac_command;
    }
    void get_name(t_uint32 index, pfc::string_base& out) override {
        out = index == 0 ? "Discogger QA MP3 write" : "Discogger QA FLAC write";
    }
    bool get_description(t_uint32, pfc::string_base& out) override {
        out = "Test-only physical metadata writer for disposable GitHub runner media.";
        return true;
    }
    GUID get_parent() override { return mainmenu_groups::file; }
    void execute(t_uint32 index, service_ptr_t<service_base>) override {
        try {
            if (index > 1) throw std::runtime_error("Invalid synthetic QA action.");
            run_disposable_media_writer_qa(index);
        } catch (const std::exception& e) {
            console::printf("DISCOGGER_RUNNER_MEDIA_QA_ERROR: %s", e.what());
        } catch (...) {
            console::print("DISCOGGER_RUNNER_MEDIA_QA_ERROR: unknown failure");
        }
    }
};
static mainmenu_commands_factory_t<discogger_runner_media_commands> runner_media_factory;
} // namespace

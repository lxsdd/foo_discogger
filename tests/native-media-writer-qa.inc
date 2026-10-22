// RUNNER-ONLY QA. Built ONLY when MSVC CL sets DISCOGGER_RUNNER_MEDIA_QA.
// Never present in any ordinary user build or distributed DLL.
#include "file_info_manager.h"
#include "discogger_async_write_lease.h"
#include "SDK/menu.h"
#include "SDK/input.h"
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <iterator>

namespace {
static const GUID qa_mp3_command = {0x4a1d5a11,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_flac_command = {0x4a1d5a12,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_targets_command = {0x4a1d5a13,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_batch_command = {0x4a1d5a14,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_populated_gui_command = {0x4a1d5a15,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_stale_disk_command = {0x4a1d5a16,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_removal_command = {0x4a1d5a17,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_partial_audit_command = {0x4a1d5a18,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};

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
    const bool unchanged = discogger_write_safety::unchanged_single_value(
        oldCount, oldCount ? info.meta_get("DISCOGGER_QA_RUNNER", 0) : nullptr,
        "RUNNER-VERIFIED");
    // SDK-only, read-only diagnostic: an embedded CUE can project a different
    // logical track view from the physical FLAC tags. No raw tags are logged.
    if (format == 1) {
        unsigned rawCount = 0, firstCount = 0, songCount = 0;
        bool rawRead = false, firstRead = false;
        try {
            abort_callback_dummy abort;
            service_ptr_t<input_info_reader> reader;
            input_entry::g_open_for_info_read(reader, nullptr, logical.c_str(), abort);
            songCount = reader->get_subsong_count();
            try {
                file_info_impl raw;
                reader->get_info(0, raw, abort);
                rawCount = static_cast<unsigned>(raw.meta_get_count_by_name("DISCOGGER_QA_RUNNER"));
                rawRead = true;
            } catch (const std::exception&) {}
            if (songCount > 0) {
                try {
                    file_info_impl first;
                    reader->get_info(reader->get_subsong(0), first, abort);
                    firstCount = static_cast<unsigned>(first.meta_get_count_by_name("DISCOGGER_QA_RUNNER"));
                    firstRead = true;
                } catch (const std::exception&) {}
            }
        } catch (const std::exception&) {}
        std::ofstream probe(folder / "qa-flac-input-reader-evidence.txt", std::ios::app);
        if (!probe) throw std::runtime_error("Missing SDK FLAC reader diagnostic stream.");
        probe << "projectedCount=" << oldCount << " subsongs=" << songCount
              << " raw0Read=" << (rawRead ? 1 : 0) << " raw0Count=" << rawCount
              << " firstRead=" << (firstRead ? 1 : 0)
              << " firstCount=" << firstCount << '\n';
        probe.flush();
        if (!probe) throw std::runtime_error("Could not flush SDK FLAC reader diagnostic.");
    }
    // Isolated diagnostics only: no values, no personal paths, no host token.
    if (format == 1) {
        std::ofstream diagnostic(folder / "qa-flac-stage-evidence.txt", std::ios::app);
        if (!diagnostic) throw std::runtime_error("Missing FLAC diagnostic stream.");
        diagnostic << "count=" << oldCount << " unchanged=" << (unchanged ? 1 : 0) << '\n';
    }
    if (!unchanged) {
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


void run_disposable_incomplete_batch_qa() {
    const char* github = std::getenv("GITHUB_ACTIONS");
    const char* runner = std::getenv("RUNNER_TEMP");
    const char* location = std::getenv("DISCOGGER_QA_MEDIA_DIR");
    if (!github || std::string(github) != "true" || !runner || !location)
        throw std::runtime_error("Incomplete-batch QA requires the ephemeral Actions runner.");
    const auto root = std::filesystem::weakly_canonical(std::filesystem::u8path(runner));
    const auto folder = std::filesystem::weakly_canonical(std::filesystem::u8path(location));
    const auto prefix = root.native() + std::filesystem::path::preferred_separator;
    if (folder.native().size() <= prefix.size() ||
        _wcsnicmp(folder.c_str(), prefix.c_str(), prefix.size()) != 0 ||
        !std::filesystem::is_directory(folder))
        throw std::runtime_error("Batch QA target lies outside ephemeral runner.");
    metadb_handle_list selection;
    for (const char* name : {"synthetic-metadata.mp3", "synthetic-embedded.flac"}) {
        const auto media = folder / name;
        if (!std::filesystem::is_regular_file(media) || std::filesystem::is_symlink(media))
            throw std::runtime_error("Unsafe synthetic batch media target.");
        const auto nativeUnits = media.u8string();
        const std::string path(nativeUnits.begin(), nativeUnits.end());
        metadb_handle_ptr handle = metadb::get()->handle_create(("file://" + path).c_str(), 0);
        if (!handle || handle->get_subsong_index() != 0)
            throw std::runtime_error("Invalid synthetic physical batch handle.");
        selection.add_item(handle);
    }
    auto manager = std::make_shared<file_info_manager>(selection);
    if (!manager->read_infos()) throw std::runtime_error("Incomplete-batch QA could not read original tags.");
    manager->invalidate_all();
    manager->get_item(0).meta_set("DISCOGGER_QA_BATCH", "MUST-NOT-WRITE");
    manager->validate_item(0);
    manager->get_item(1).meta_set("DISCOGGER_QA_BATCH", "MUST-NOT-WRITE");
    manager->validate_item(1);
    // Test-only injected stale snapshot on the second file. The real writer
    // must reject the *entire batch* before writing the first file.
    manager->info_wrappers[1]->original_captured = false;
    bool rejected = false;
    try {
        auto lease = discogger_write_safety::acquire_physical_write_lease();
        if (!lease) throw std::runtime_error("Incomplete-batch QA lease unavailable.");
        manager->write_infos({}, std::move(lease), false);
    } catch (const foo_discogs_exception&) { rejected = true; }
    if (!rejected)
        throw std::runtime_error("Incomplete physical batch accepted despite missing original snapshot.");
    const auto marker = folder / "qa-incomplete-batch-rejected.txt";
    if (std::filesystem::exists(marker))
        throw std::runtime_error("Incomplete-batch QA marker unexpectedly exists.");
    std::ofstream output(marker);
    if (!output) throw std::runtime_error("Cannot record incomplete-batch preflight evidence.");
    output << "BATCH=REJECTED_BEFORE_IO\n";
    output.flush();
    if (!output) throw std::runtime_error("Cannot flush incomplete-batch preflight evidence.");
}


void run_populated_search_gui_qa() {
    const char* actions = std::getenv("GITHUB_ACTIONS");
    const char* runner = std::getenv("RUNNER_TEMP");
    const char* location = std::getenv("DISCOGGER_QA_MEDIA_DIR");
    if (!actions || std::string(actions) != "true" || !runner || !location)
        throw std::runtime_error("Synthetic GUI model allowed only in ephemeral runner.");
    const auto root = std::filesystem::weakly_canonical(std::filesystem::u8path(runner));
    const auto folder = std::filesystem::weakly_canonical(std::filesystem::u8path(location));
    const auto prefix = root.native() + std::filesystem::path::preferred_separator;
    if (folder.native().size() <= prefix.size() ||
        _wcsnicmp(folder.c_str(), prefix.c_str(), prefix.size()) != 0 ||
        !std::filesystem::is_directory(folder))
        throw std::runtime_error("Synthetic GUI evidence must remain under runner temp.");
    if (!g_discogs || !g_discogs->find_release_dialog)
        throw std::runtime_error("Native Discogger Find Releases is not open.");
    const auto counts = g_discogs->find_release_dialog->runner_qa_populated_search_rows();
    if (counts.first != 1 || counts.second != 4)
        throw std::runtime_error("Native GUI did not populate one artist and four releases.");
    const auto evidence = folder / "qa-populated-gui.txt";
    if (std::filesystem::exists(evidence))
        throw std::runtime_error("Synthetic GUI evidence already existed.");
    std::ofstream out(evidence);
    if (!out) throw std::runtime_error("Could not write synthetic GUI evidence.");
    out << "ARTISTS=1\\nRELEASES=4\\n";
    out.flush();
    if (!out) throw std::runtime_error("Could not flush synthetic GUI evidence.");
}


void run_disposable_stale_disk_qa() {
    const char* github = std::getenv("GITHUB_ACTIONS");
    const char* runner = std::getenv("RUNNER_TEMP");
    const char* location = std::getenv("DISCOGGER_QA_MEDIA_DIR");
    if (!github || std::string(github) != "true" || !runner || !location)
        throw std::runtime_error("Stale physical metadata QA requires an ephemeral runner.");
    const auto root = std::filesystem::weakly_canonical(std::filesystem::u8path(runner));
    const auto folder = std::filesystem::weakly_canonical(std::filesystem::u8path(location));
    const auto prefix = root.native() + std::filesystem::path::preferred_separator;
    if (folder.native().size() <= prefix.size() ||
        _wcsnicmp(folder.c_str(), prefix.c_str(), prefix.size()) != 0 ||
        !std::filesystem::is_directory(folder))
        throw std::runtime_error("Unsafe synthetic stale-metadata test directory.");
    const auto mp3 = folder / "synthetic-metadata.mp3";
    if (!std::filesystem::is_regular_file(mp3) || std::filesystem::is_symlink(mp3))
        throw std::runtime_error("Stale metadata fixture missing or unsafe.");
    const auto pathUnits = mp3.u8string();
    const std::string url = "file://" + std::string(pathUnits.begin(), pathUnits.end());
    metadb_handle_ptr handle = metadb::get()->handle_create(url.c_str(), 0);
    if (!handle || handle->get_subsong_index() != 0)
        throw std::runtime_error("Stale metadata host handle unavailable.");
    metadb_handle_list targets;
    targets.add_item(handle);
    auto manager = std::make_shared<file_info_manager>(targets);
    if (!manager->read_infos())
        throw std::runtime_error("Cannot read disposable original MP3 tags.");
    manager->invalidate_all();
    manager->get_item(0).meta_set("DISCOGGER_QA_STALE", "MUST_NOT_COMMIT");
    manager->validate_item(0);

    // Simulate an independent tag editor changing the file AFTER Discogger
    // captured its snapshot but BEFORE Discogger schedules a write.
    abort_callback_dummy abort;
    service_ptr_t<input_info_writer> other_editor;
    input_entry::g_open_for_info_write(other_editor, nullptr, url.c_str(), abort);
    file_info_impl external_tags;
    other_editor->get_info(0, external_tags, abort);
    external_tags.meta_set("DISCOGGER_QA_EXTERNAL_CHANGE", "PRESERVE");
    other_editor->set_info(0, external_tags, abort);
    other_editor->commit(abort);
    other_editor.release();

    auto read_file = [&mp3]() {
        std::ifstream stream(mp3, std::ios::binary);
        if (!stream) throw std::runtime_error("Stale QA failed to open synthetic MP3.");
        return std::string(std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>());
    };
    const auto expected_bytes = read_file();
    const auto expected_mtime = std::filesystem::last_write_time(mp3);
    bool stale_rejected = false;
    try {
        auto lease = discogger_write_safety::acquire_physical_write_lease();
        if (!lease) throw std::runtime_error("Stale metadata write lease unavailable.");
        manager->write_infos({}, std::move(lease), false);
    } catch (const foo_discogs_exception&) {
        stale_rejected = true;
    }
    if (!stale_rejected ||
        expected_bytes != read_file() ||
        expected_mtime != std::filesystem::last_write_time(mp3))
        throw std::runtime_error("Outdated physical snapshot reached metadata writer.");
    service_ptr_t<input_info_reader> verify_reader;
    input_entry::g_open_for_info_read(verify_reader, nullptr, url.c_str(), abort);
    file_info_impl after;
    verify_reader->get_info(0, after, abort);
    if (after.meta_get_count_by_name("DISCOGGER_QA_EXTERNAL_CHANGE") != 1 ||
        after.meta_get_count_by_name("DISCOGGER_QA_STALE") != 0)
        throw std::runtime_error("Fresh external edits or stale write rejection were lost.");
    const auto marker = folder / "qa-stale-disk-rejected.txt";
    if (std::filesystem::exists(marker))
        throw std::runtime_error("Stale QA evidence unexpectedly pre-existing.");
    std::ofstream out(marker);
    if (!out) throw std::runtime_error("Cannot record stale-physical rejection.");
    out << "STALE=REJECTED\\nEXTERNAL=RETAINED\\n";
    out.flush();
    if (!out) throw std::runtime_error("Cannot flush stale-physical rejection.");
}


void run_disposable_unapproved_removal_qa() {
    const char* github = std::getenv("GITHUB_ACTIONS");
    const char* runner = std::getenv("RUNNER_TEMP");
    const char* directory = std::getenv("DISCOGGER_QA_MEDIA_DIR");
    if (!github || std::string(github) != "true" || !runner || !directory)
        throw std::runtime_error("Destructive-edit QA requires isolated Actions runner.");
    const auto root = std::filesystem::weakly_canonical(std::filesystem::u8path(runner));
    const auto folder = std::filesystem::weakly_canonical(std::filesystem::u8path(directory));
    const auto prefix = root.native() + std::filesystem::path::preferred_separator;
    if (folder.native().size() <= prefix.size() ||
        _wcsnicmp(folder.c_str(), prefix.c_str(), prefix.size()) != 0 ||
        !std::filesystem::is_directory(folder))
        throw std::runtime_error("Destructive-edit QA folder lies outside runner temp.");
    const auto file = folder / "synthetic-embedded.flac";
    if (!std::filesystem::is_regular_file(file) || std::filesystem::is_symlink(file))
        throw std::runtime_error("Missing safe synthetic FLAC.");
    const auto utf = file.u8string();
    const std::string path(utf.begin(), utf.end());
    metadb_handle_ptr handle = metadb::get()->handle_create(("file://" + path).c_str(), 0);
    if (!handle || handle->get_subsong_index() != 0)
        throw std::runtime_error("Invalid physical synthetic FLAC.");
    metadb_handle_list selection;
    selection.add_item(handle);
    const char* removals[] = {"GENRE", "CUESHEET"};
    for (const auto field : removals) {
        auto manager = std::make_shared<file_info_manager>(selection);
        if (!manager->read_infos())
            throw std::runtime_error("Cannot read pristine physical FLAC metadata.");
        manager->invalidate_all();
        auto& info = manager->get_item(0);
        if (info.meta_get_count_by_name(field) == 0)
            throw std::runtime_error("Synthetic FLAC lacks required removal test field.");
        info.meta_remove_field(field);
        manager->validate_item(0);
        bool denied = false;
        try {
            auto lease = discogger_write_safety::acquire_physical_write_lease();
            if (!lease) throw std::runtime_error("Destructive-edit QA write lease unavailable.");
            manager->write_infos({}, std::move(lease), false);
        } catch (const foo_discogs_exception&) { denied = true; }
        if (!denied)
            throw std::runtime_error("Unapproved physical metadata removal was not denied.");
    }
    const auto marker = folder / "qa-unapproved-removal-denied.txt";
    if (std::filesystem::exists(marker))
        throw std::runtime_error("Existing destructive-edit QA marker.");
    std::ofstream out(marker);
    if (!out) throw std::runtime_error("Cannot record destructive-edit denial.");
    out << "GENRE=DENIED\nCUESHEET=DENIED\n";
    out.flush();
    if (!out) throw std::runtime_error("Cannot flush destructive-edit evidence.");
}


void run_disposable_partial_write_audit_qa() {
    // Inject a second-file failure AFTER a REAL first-file physical commit.
    // This exercises the production readback on an isolated host; it is not
    // an induced failure of metadb_io::update_info_async_simple itself.
    const char* actions = std::getenv("GITHUB_ACTIONS");
    const char* runner = std::getenv("RUNNER_TEMP");
    const char* location = std::getenv("DISCOGGER_QA_MEDIA_DIR");
    if (!actions || std::string(actions) != "true" || !runner || !location)
        throw std::runtime_error("Partial write audit requires isolated Actions runner.");
    const auto root = std::filesystem::weakly_canonical(std::filesystem::u8path(runner));
    const auto folder = std::filesystem::weakly_canonical(std::filesystem::u8path(location));
    const auto prefix = root.native() + std::filesystem::path::preferred_separator;
    if (folder.native().size() <= prefix.size() ||
        _wcsnicmp(folder.c_str(), prefix.c_str(), prefix.size()) != 0 ||
        !std::filesystem::is_directory(folder))
        throw std::runtime_error("Unsafe partial write audit directory.");

    const char* filenames[] = {"synthetic-metadata.mp3", "synthetic-embedded.flac"};
    metadb_handle_list selection;
    std::string paths[2];
    for (size_t i = 0; i < 2; ++i) {
        const auto file = folder / filenames[i];
        if (!std::filesystem::is_regular_file(file) || std::filesystem::is_symlink(file))
            throw std::runtime_error("Partial audit synthetic fixture missing or unsafe.");
        const auto units = file.u8string();
        paths[i] = "file://" + std::string(units.begin(), units.end());
        auto handle = metadb::get()->handle_create(paths[i].c_str(), 0);
        if (!handle || handle->get_subsong_index() != 0)
            throw std::runtime_error("Could not open safe physical QA target.");
        selection.add_item(handle);
    }
    auto manager = std::make_shared<file_info_manager>(selection);
    if (!manager->read_infos())
        throw std::runtime_error("Partial QA could not read original physical metadata.");
    manager->invalidate_all();
    discogger_async_tag_payload payload;
    payload.items_to_update.set_size(2);
    payload.infos_to_write.set_size(2);
    payload.original_infos.set_size(2);
    for (size_t i = 0; i < 2; ++i) {
        file_info_impl proposed = manager->get_item(i);
        proposed.meta_set("DISCOGGER_QA_PARTIAL", i == 0 ? "FIRST" : "SECOND");
        payload.items_to_update[i] = selection[i];
        payload.original_infos[i] = payload.original_copies.append_copy(manager->get_item(i));
        payload.infos_to_write[i] = payload.staged_infos.append_copy(proposed);
    }
    payload.submitted_count = 2;

    // SDK write only the first synthetic file and inject failure before the
    // second. Actual, fresh physical input reader classifies both outcomes.
    abort_callback_dummy abort;
    service_ptr_t<input_info_writer> writer;
    input_entry::g_open_for_info_write(writer, nullptr, paths[0].c_str(), abort);
    file_info_impl edited;
    writer->get_info(0, edited, abort);
    edited.meta_set("DISCOGGER_QA_PARTIAL", "FIRST");
    writer->set_info(0, edited, abort);
    writer->commit(abort);
    writer.release();
    const auto audit = discogger_audit_failed_write(payload);
    if (audit.count() != 2 || audit.requested != 1 || audit.original != 1 ||
        audit.divergent != 0 || audit.unreadable != 0 ||
        !audit.cannot_confirm_unchanged())
        throw std::runtime_error("Physical partial-commit readback misclassified the two files.");
    // The failed-write cleanup must revoke old permission and in-memory edits.
    manager->discard_pending_changes();
    bool denied = false;
    try { manager->invalidate_all(); }
    catch (const foo_discogs_exception&) { denied = true; }
    if (!denied)
        throw std::runtime_error("Partial-commit path retained obsolete write approval.");

    const auto marker = folder / "qa-injected-partial-audit.txt";
    if (std::filesystem::exists(marker))
        throw std::runtime_error("Partial audit marker unexpectedly exists.");
    std::ofstream out(marker);
    if (!out) throw std::runtime_error("Cannot record partial audit marker.");
    out << "FIRST=REQUESTED\nSECOND=ORIGINAL\nSNAPSHOTS=REVOKED\n";
    out.flush();
    if (!out) throw std::runtime_error("Could not flush partial audit evidence.");
}

class discogger_runner_media_commands : public mainmenu_commands {
public:
    t_uint32 get_command_count() override { return 8; }
    GUID get_command(t_uint32 index) override {
        static const GUID commands[] = {
            qa_mp3_command, qa_flac_command, qa_targets_command, qa_batch_command,
            qa_populated_gui_command, qa_stale_disk_command, qa_removal_command,
            qa_partial_audit_command
        };
        return index < 8 ? commands[index] : GUID{};
    }
    void get_name(t_uint32 index, pfc::string_base& out) override {
        static const char* names[] = {
            "Discogger QA MP3 write", "Discogger QA FLAC write",
            "Discogger QA unsafe targets", "Discogger QA incomplete batch",
            "Discogger QA populated GUI", "Discogger QA stale disk",
            "Discogger QA unsafe removals", "Discogger QA injected partial"
        };
        out = index < 8 ? names[index] : "Invalid Discogger QA command";
    }
    bool get_description(t_uint32, pfc::string_base& out) override {
        out = "Test-only physical metadata writer for disposable GitHub runner media.";
        return true;
    }
    GUID get_parent() override { return mainmenu_groups::file; }
    void execute(t_uint32 index, service_ptr_t<service_base>) override {
        try {
            if (index > 7) throw std::runtime_error("Invalid synthetic QA action.");
            if (index == 7) run_disposable_partial_write_audit_qa();
            else if (index == 6) run_disposable_unapproved_removal_qa();
            else if (index == 5) run_disposable_stale_disk_qa();
            else if (index == 4) run_populated_search_gui_qa();
            else if (index == 3) run_disposable_incomplete_batch_qa();
            else if (index == 2) run_disposable_target_rejection_qa();
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

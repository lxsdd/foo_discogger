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
#include <cstring>
#include <array>
#include "live-discogs-search-corpus-qa.h"
#include "tests/runner-live-search-telemetry.h"

namespace {
static const GUID qa_mp3_command = {0x4a1d5a11,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_flac_command = {0x4a1d5a12,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_targets_command = {0x4a1d5a13,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_batch_command = {0x4a1d5a14,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_populated_gui_command = {0x4a1d5a15,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_stale_disk_command = {0x4a1d5a16,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_removal_command = {0x4a1d5a17,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_partial_audit_command = {0x4a1d5a18,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_sidecar_artwork_command = {0x4a1d5a19,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_embedded_artwork_command = {0x4a1d5a1a,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};
static const GUID qa_live_search_probe_command = {0x4a1d5a1b,0x9b8e,0x43a4,{0xa8,0x33,0x6b,0x8c,0x32,0x6d,0x21,0x10}};

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
    // Failures inside the original foobar GUI command are caught by the
    // component's command dispatcher. Export only this runner-local diagnostic
    // so the host CI reports the actual failed native assertion instead of
    // timing out on a missing success marker.
    std::pair<size_t, size_t> counts;
    try {
        counts = g_discogs->find_release_dialog->runner_qa_populated_search_rows();
    } catch (const std::exception& e) {
        const auto diagnostic = folder / "qa-populated-gui-error.txt";
        std::ofstream out(diagnostic, std::ios::binary | std::ios::trunc);
        if (out) { out << e.what() << '\n'; out.flush(); }
        throw;
    } catch (...) {
        const auto diagnostic = folder / "qa-populated-gui-error.txt";
        std::ofstream out(diagnostic, std::ios::binary | std::ios::trunc);
        if (out) { out << "unknown native GUI QA exception\n"; out.flush(); }
        throw;
    }
    if (counts.first != 1 || counts.second != 5)
        throw std::runtime_error("Native GUI did not populate one artist and five releases.");
    const auto evidence = folder / "qa-populated-gui.txt";
    if (std::filesystem::exists(evidence))
        throw std::runtime_error("Synthetic GUI evidence already existed.");
    std::ofstream out(evidence);
    if (!out) throw std::runtime_error("Could not write synthetic GUI evidence.");
    out << "ARTISTS=1\\nRELEASES=5\\n";
    out.flush();
    if (!out) throw std::runtime_error("Could not flush synthetic GUI evidence.");
}


void run_live_search_probe_qa() {
    const char* actions = std::getenv("GITHUB_ACTIONS");
    const char* runner = std::getenv("RUNNER_TEMP");
    const char* location = std::getenv("DISCOGGER_QA_MEDIA_DIR");
    if (!actions || std::string(actions) != "true" || !runner || !location)
        throw std::runtime_error("Live GUI inspection allowed only in ephemeral runner.");
    const auto root = std::filesystem::weakly_canonical(std::filesystem::u8path(runner));
    const auto folder = std::filesystem::weakly_canonical(std::filesystem::u8path(location));
    const auto prefix = root.native() + std::filesystem::path::preferred_separator;
    if (folder.native().size() <= prefix.size() ||
        _wcsnicmp(folder.c_str(), prefix.c_str(), prefix.size()) != 0 ||
        !std::filesystem::is_directory(folder))
        throw std::runtime_error("Live GUI evidence must remain under runner temp.");
    if (!g_discogs || !g_discogs->find_release_dialog)
        throw std::runtime_error("Native Find Releases dialog is absent.");
    std::string expected_release_id = "38012673";
    const auto requested_id_file = folder / "qa-live-expected-id.txt";
    if (std::filesystem::exists(requested_id_file)) {
        if (!std::filesystem::is_regular_file(requested_id_file) ||
            std::filesystem::is_symlink(requested_id_file))
            throw std::runtime_error("Unsafe public release QA selector.");
        std::ifstream input(requested_id_file);
        if (!input || !std::getline(input, expected_release_id))
            throw std::runtime_error("Unreadable public release QA selector.");
        std::string extra;
        if (std::getline(input, extra))
            throw std::runtime_error("Multi-line public release QA selector.");
        // The 14 independent API-cohort cases do not include the initial
        // Estiva baseline or the fifth mandatory Quivver GUI-only release.
        // Permit these two fixed public release identities without expanding
        // API probing or admitting arbitrary selector input.
        bool allowed = expected_release_id == "38012673" ||
                       expected_release_id == "35534398";
        for (const auto& entry : discogger_live_corpus_qa::cases) {
            if (expected_release_id == std::to_string(entry.expected_release_id)) {
                allowed = true;
                break;
            }
        }
        if (!allowed)
            throw std::runtime_error("Unrecognized public release QA selector.");
    }
    const auto counts = g_discogs->find_release_dialog->runner_qa_live_search_probe(expected_release_id.c_str());
    // Independent read-only requests distinguish OAuth/transport failures from
    // actual Discogger UI and result-model failures. Never record API bodies.
    // The initial Estiva baseline already authenticates and probes the full
    // public API cohort. Matrix probes must inspect only the native GUI state:
    // repeating a synchronous OAuth request on the foobar UI thread for every
    // case can stall evidence publication under network throttling.
    unsigned identity_ok = 0;
    if (expected_release_id == "38012673") {
        try {
            abort_callback_dummy abort;
            pfc::string8 body;
            discogs_interface->fetcher->fetch_html(
                "https://api.discogs.com/oauth/identity", "", body, abort);
            JSONParser parsed(body);
            const json_t* username = json_object_get(parsed.root, "username");
            identity_ok = json_is_string(username) && json_string_length(username) > 0 ? 1u : 0u;
        } catch (...) {}
    }
    const auto probe_search = [](const char* params, long long expected_id) -> std::array<unsigned, 4> {
        // [0]=valid results, [1]=total hits, [2]=target on this page,
        // [3]=advertised pages. Keep OAuth bodies and queries out of logs.
        std::array<unsigned, 4> result{0, 0, 0, 0};
        try {
            abort_callback_dummy abort;
            pfc::string8 body;
            discogs_interface->fetcher->fetch_html(
                "https://api.discogs.com/database/search", params, body, abort);
            JSONParser parsed(body);
            const json_t* rows = json_object_get(parsed.root, "results");
            if (!json_is_array(rows)) return result;
            result[0] = 1;
            result[1] = static_cast<unsigned>(
                parsed.get_object_int("pagination", "items"));
            const json_t* pagination = json_object_get(parsed.root, "pagination");
            const json_t* pages = json_object_get(pagination, "pages");
            if (json_is_integer(pages)) {
                const json_int_t page_count = json_integer_value(pages);
                if (page_count > 0 && page_count <= 100000)
                    result[3] = static_cast<unsigned>(page_count);
            }
            for (size_t i = 0; i < json_array_size(rows); ++i) {
                const json_t* record = json_array_get(rows, i);
                const json_t* id = json_object_get(record, "id");
                if (json_is_integer(id) && json_integer_value(id) == expected_id)
                    result[2] = 1;
            }
        } catch (...) {}
        return result;
    };
    std::array<unsigned, 4> api_exact{0, 0, 0, 0}, api_broad{0, 0, 0, 0};
    // These fixed API baselines belong to the Estiva reference, not to each
    // independently selected native GUI case (limit request load).
    if (expected_release_id == "38012673") {
        api_exact = probe_search(
            "artist=Estiva&title=Via%20Balearia&per_page=100&page=1", 38012673);
        api_broad = probe_search(
            "q=%22Via%20Balearia%22&per_page=100&page=1", 38012673);
    }
    // Independent public-search cohort: eleven diverse identities plus three
    // original user-reported missing releases,
    // including collaborations, ampersands, UTF-8 and punctuation. Use actual
    // authenticated API requests only in the known-positive live-GUI probe,
    // never again in the empty-result probe (avoids duplicate rate-limit load).
    unsigned cohort_ok = 0, cohort_nonempty = 0, cohort_known = 0;
    unsigned cohort_nonempty_mask = 0, cohort_known_mask = 0;
    // QA-only bounded diagnostics for absent first-page IDs. These do not
    // alter the original fourteen-request first-page evidence or the native
    // GUI search path. At most three extra page-two requests per architecture.
    unsigned cohort_page2_attempt_mask = 0, cohort_page2_ok_mask = 0;
    unsigned cohort_page2_known_mask = 0, cohort_page2_requests = 0;
    unsigned cohort_sunmoon_targeted_ok = 0, cohort_sunmoon_targeted_known = 0;
    const unsigned cohort_total = static_cast<unsigned>(discogger_live_corpus_qa::cases.size());
    if (counts[1] != 0 && expected_release_id == "38012673") {
        for (unsigned i = 0; i < cohort_total; ++i) {
            const auto& entry = discogger_live_corpus_qa::cases[i];
            const auto evidence = probe_search(entry.encoded_search_params, entry.expected_release_id);
            cohort_ok += evidence[0];
            if (evidence[0] && evidence[1] > 0) {
                ++cohort_nonempty;
                cohort_nonempty_mask |= (1u << i);
            }
            if (evidence[2] != 0) {
                ++cohort_known;
                cohort_known_mask |= (1u << i);
            } else if (evidence[0]) {
                // Diagnose pagination and query specificity separately. A
                // missing ID on page 1 is NOT proof it exists on page 2.
                if (evidence[3] >= 2 && cohort_page2_requests < 3) {
                    std::string page_two(entry.encoded_search_params);
                    const std::string suffix = "&page=1";
                    if (page_two.size() >= suffix.size() &&
                        page_two.compare(page_two.size() - suffix.size(),
                            suffix.size(), suffix) == 0) {
                        page_two.replace(page_two.size() - suffix.size(),
                            suffix.size(), "&page=2");
                        ++cohort_page2_requests;
                        cohort_page2_attempt_mask |= (1u << i);
                        const auto page_two_result =
                            probe_search(page_two.c_str(), entry.expected_release_id);
                        if (page_two_result[0]) cohort_page2_ok_mask |= (1u << i);
                        if (page_two_result[2]) cohort_page2_known_mask |= (1u << i);
                    }
                }
                // The historically unstable broad "Sun & Moon" query has
                // no artist filter. Compare it with a structured, release-only
                // query; never upgrade that diagnostic to a successful match.
                if (entry.expected_release_id == 2783977) {
                    const auto targeted = probe_search(
                        "artist=Above%20%26%20Beyond&release_title=Sun%20%26%20Moon&type=release&per_page=100&page=1",
                        entry.expected_release_id);
                    cohort_sunmoon_targeted_ok = targeted[0];
                    cohort_sunmoon_targeted_known = targeted[2];
                }
            }
        }
    }
    const auto evidence = folder / "qa-live-search-probe.txt";
    if (std::filesystem::exists(evidence))
        throw std::runtime_error("Live GUI evidence already existed.");
    std::ofstream out(evidence);
    if (!out) throw std::runtime_error("Could not write live GUI evidence.");
    // Counts and fixed public release identity only. Never record query,
    // headers, OAuth values, responses, profile data or arbitrary error text.
    out << "ARTISTS=" << counts[0] << '\n'
        << "KNOWN_RELEASE_MATCHES=" << counts[1] << '\n'
        << "TREE=" << counts[2] << '\n'
        << "BUTTON=" << discogger_live_search_qa::button.load() << '\n'
        << "INPUT_NONEMPTY=" << discogger_live_search_qa::input_nonempty.load() << '\n'
        << "INPUT_NONEMPTY_CALLS=" << discogger_live_search_qa::input_nonempty_calls.load() << '\n'
        << "INPUT_EMPTY_CALLS=" << discogger_live_search_qa::input_empty_calls.load() << '\n'
        << "INPUT_MEMBER_VALID=" << discogger_live_search_qa::input_member_valid.load() << '\n'
        << "INPUT_HWND_MATCH=" << discogger_live_search_qa::input_hwnd_match.load() << '\n'
        << "INPUT_MEMBER_LENGTH=" << discogger_live_search_qa::input_member_length.load() << '\n'
        << "INPUT_LIVE_LENGTH=" << discogger_live_search_qa::input_live_length.load() << '\n'
        << "PARSED_FIELDS=" << discogger_live_search_qa::parsed_fields.load() << '\n'
        << "QUERY_MINIMAL=" << discogger_live_search_qa::query_minimal.load() << '\n'
        << "QUERY_DETECTED=" << discogger_live_search_qa::query_detected.load() << '\n'
        << "ALT_MODE=" << discogger_live_search_qa::alt_mode.load() << '\n'
        << "REPEAT_QUERY=" << discogger_live_search_qa::repeat_query.load() << '\n'
        << "EARLY_EXIT=" << discogger_live_search_qa::early_exit.load() << '\n'
        << "ROUTED=" << discogger_live_search_qa::routed.load() << '\n'
        << "WORKER_STARTED=" << discogger_live_search_qa::worker_started.load() << '\n'
        << "WORKER_ADVANCED=" << discogger_live_search_qa::worker_advanced.load() << '\n'
        << "WORKER_SUCCESS=" << discogger_live_search_qa::worker_success.load() << '\n'
        << "WORKER_ERROR=" << discogger_live_search_qa::worker_error.load() << '\n'
        << "WORKER_ABORT=" << discogger_live_search_qa::worker_abort.load() << '\n'
        << "FALLBACK_ENTERED=" << discogger_live_search_qa::fallback_entered.load() << '\n'
        << "FALLBACK_PLANNED=" << discogger_live_search_qa::fallback_planned.load() << '\n'
        << "FALLBACK_REQUESTED=" << discogger_live_search_qa::fallback_requested.load() << '\n'
        << "FALLBACK_COMPLETED=" << discogger_live_search_qa::fallback_completed.load() << '\n'
        << "FALLBACK_PRIMARY=" << discogger_live_search_qa::fallback_primary.load() << '\n'
        << "FALLBACK_PARTNER=" << discogger_live_search_qa::fallback_partner.load() << '\n'
        << "FALLBACK_BROAD=" << discogger_live_search_qa::fallback_broad.load() << '\n'
        << "RESPONSE_PAGES=" << discogger_live_search_qa::response_pages.load() << '\n'
        << "RESPONSE_ITEMS=" << discogger_live_search_qa::response_items.load() << '\n'
        << "PARSED_MASTERS=" << discogger_live_search_qa::parsed_masters.load() << '\n'
        << "PARSED_RELEASES=" << discogger_live_search_qa::parsed_releases.load() << '\n'
        << "QUERY_LIMIT=" << discogger_live_search_qa::query_limit.load() << '\n'
        << "API_IDENTITY=" << identity_ok << '\n'
        << "API_EXACT_OK=" << api_exact[0] << '\n'
        << "API_EXACT_ITEMS=" << api_exact[1] << '\n'
        << "API_EXACT_KNOWN=" << api_exact[2] << '\n'
        << "API_BROAD_OK=" << api_broad[0] << '\n'
        << "API_BROAD_ITEMS=" << api_broad[1] << '\n'
        << "API_BROAD_KNOWN=" << api_broad[2] << '\n'
        << "API_COHORT_TOTAL=" << (counts[1] != 0 && expected_release_id == "38012673" ? cohort_total : 0) << '\n'
        << "API_COHORT_OK=" << cohort_ok << '\n'
        << "API_COHORT_NONEMPTY=" << cohort_nonempty << '\n'
        << "API_COHORT_KNOWN=" << cohort_known << '\n'
        << "API_COHORT_MASK=" << cohort_nonempty_mask << '\n'
        << "API_COHORT_KNOWN_MASK=" << cohort_known_mask << '\n'
        << "API_COHORT_PAGE2_ATTEMPT_MASK=" << cohort_page2_attempt_mask << '\n'
        << "API_COHORT_PAGE2_OK_MASK=" << cohort_page2_ok_mask << '\n'
        << "API_COHORT_PAGE2_KNOWN_MASK=" << cohort_page2_known_mask << '\n'
        << "API_COHORT_SUNMOON_TARGETED_OK=" << cohort_sunmoon_targeted_ok << '\n'
        << "API_COHORT_SUNMOON_TARGETED_KNOWN=" << cohort_sunmoon_targeted_known << '\n';
    out.flush();
    if (!out) throw std::runtime_error("Could not flush live GUI evidence.");
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
    const auto before_audit = discogger_audit_failed_write(payload);
    if (before_audit.original != 2 || before_audit.count() != 2)
        throw std::runtime_error(
            "Physical baseline mismatch: original=" + std::to_string(before_audit.original) +
            " requested=" + std::to_string(before_audit.requested) +
            " divergent=" + std::to_string(before_audit.divergent) +
            " unreadable=" + std::to_string(before_audit.unreadable));

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
        throw std::runtime_error(
            "Physical partial-commit readback: original=" + std::to_string(audit.original) +
            " requested=" + std::to_string(audit.requested) +
            " divergent=" + std::to_string(audit.divergent) +
            " unreadable=" + std::to_string(audit.unreadable));
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

void record_disposable_partial_qa_error(const char* reason) noexcept {
    try {
        const char* actions = std::getenv("GITHUB_ACTIONS");
        const char* runner = std::getenv("RUNNER_TEMP");
        const char* location = std::getenv("DISCOGGER_QA_MEDIA_DIR");
        if (!actions || std::string(actions) != "true" || !runner || !location)
            return;
        const auto root = std::filesystem::weakly_canonical(std::filesystem::u8path(runner));
        const auto folder = std::filesystem::weakly_canonical(std::filesystem::u8path(location));
        const auto prefix = root.native() + std::filesystem::path::preferred_separator;
        if (folder.native().size() <= prefix.size() ||
            _wcsnicmp(folder.c_str(), prefix.c_str(), prefix.size()) != 0 ||
            !std::filesystem::is_directory(folder))
            return;
        std::ofstream out(folder / "qa-injected-partial-error.txt");
        if (out) out << "PARTIAL_QA_FAILED=" << (reason ? reason : "unknown") << '\n';
    } catch (...) {}
}


static thread_local const char* qa_sidecar_artwork_stage = "dispatch";

void record_disposable_sidecar_qa_error() noexcept {
    try {
        const char* actions = std::getenv("GITHUB_ACTIONS");
        const char* runner = std::getenv("RUNNER_TEMP");
        const char* location = std::getenv("DISCOGGER_QA_MEDIA_DIR");
        if (!actions || std::string(actions) != "true" || !runner || !location) return;
        const auto root = std::filesystem::weakly_canonical(std::filesystem::u8path(runner));
        const auto folder = std::filesystem::weakly_canonical(std::filesystem::u8path(location));
        const auto prefix = root.native() + std::filesystem::path::preferred_separator;
        if (folder.native().size() <= prefix.size() ||
            _wcsnicmp(folder.c_str(), prefix.c_str(), prefix.size()) != 0 ||
            !std::filesystem::is_directory(folder)) return;
        std::ofstream out(folder / "qa-sidecar-error.txt");
        if (out) out << "STAGE=" << qa_sidecar_artwork_stage << '\n';
    } catch (...) {}
}

void run_disposable_sidecar_artwork_qa() {
    qa_sidecar_artwork_stage = "environment";
    const char* actions = std::getenv("GITHUB_ACTIONS");
    const char* runner = std::getenv("RUNNER_TEMP");
    const char* location = std::getenv("DISCOGGER_QA_MEDIA_DIR");
    if (!actions || std::string(actions) != "true" || !runner || !location || !g_discogs)
        throw std::runtime_error("Sidecar artwork QA requires isolated Actions runner.");
    const auto root = std::filesystem::weakly_canonical(std::filesystem::u8path(runner));
    const auto folder = std::filesystem::weakly_canonical(std::filesystem::u8path(location));
    const auto prefix = root.native() + std::filesystem::path::preferred_separator;
    if (folder.native().size() <= prefix.size() ||
        _wcsnicmp(folder.c_str(), prefix.c_str(), prefix.size()) != 0 ||
        !std::filesystem::is_directory(folder))
        throw std::runtime_error("Unsafe sidecar artwork QA directory.");
    qa_sidecar_artwork_stage = "fixture-create";
    const auto sidecar = folder / "synthetic-cover.jpg";
    const auto marker = folder / "qa-sidecar-artwork.txt";
    if (std::filesystem::exists(sidecar) || std::filesystem::exists(marker))
        throw std::runtime_error("Sidecar artwork QA fixture unexpectedly exists.");
    // These bytes are generated for this test; no user artwork or media is read.
    const std::string original = "synthetic-sidecar-original";
    const std::string replacement = "synthetic-sidecar-replaced";
    {
        std::ofstream out(sidecar, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("Cannot create disposable sidecar.");
        out.write(original.data(), static_cast<std::streamsize>(original.size()));
        out.flush();
        if (!out) throw std::runtime_error("Cannot flush disposable sidecar.");
    }
    if (!std::filesystem::is_regular_file(sidecar) || std::filesystem::is_symlink(sidecar))
        throw std::runtime_error("Unsafe disposable sidecar target.");
    const auto units = sidecar.u8string();
    const pfc::string8 host_path(std::string(units.begin(), units.end()).c_str());
    auto bytes = [&]() {
        std::ifstream in(sidecar, std::ios::binary);
        if (!in) throw std::runtime_error("Cannot read disposable sidecar.");
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };
    auto buffer_for = [](const std::string& value) {
        MemoryBlock buffer;
        buffer.set_size(value.size());
        std::memcpy(buffer.get_ptr(), value.data(), value.size());
        return buffer;
    };
    abort_callback_dummy abort;
    auto original_buffer = buffer_for(original);
    const auto original_time = std::filesystem::last_write_time(sidecar);
    qa_sidecar_artwork_stage = "first-noop";
    g_discogs->write_image(original_buffer, host_path, abort);
    if (bytes() != original || std::filesystem::last_write_time(sidecar) != original_time)
        throw std::runtime_error("Identical sidecar artwork was rewritten.");
    auto replacement_buffer = buffer_for(replacement);
    qa_sidecar_artwork_stage = "replacement";
    g_discogs->write_image(replacement_buffer, host_path, abort);
    if (bytes() != replacement)
        throw std::runtime_error("Sidecar artwork replacement did not commit.");
    const auto replacement_time = std::filesystem::last_write_time(sidecar);
    qa_sidecar_artwork_stage = "second-noop";
    g_discogs->write_image(replacement_buffer, host_path, abort);
    if (bytes() != replacement ||
        std::filesystem::last_write_time(sidecar) != replacement_time)
        throw std::runtime_error("Repeated sidecar artwork was rewritten.");
    qa_sidecar_artwork_stage = "empty-denial";
    MemoryBlock empty;
    bool denied = false;
    try { g_discogs->write_image(empty, host_path, abort); }
    catch (const foo_discogs_exception&) { denied = true; }
    if (!denied || bytes() != replacement ||
        std::filesystem::last_write_time(sidecar) != replacement_time)
        throw std::runtime_error("Empty sidecar artwork request changed existing file.");
    qa_sidecar_artwork_stage = "evidence";
    std::ofstream out(marker);
    if (!out) throw std::runtime_error("Cannot record sidecar artwork QA.");
    out << "NOOP=PASS\nREPLACE=PASS\nEMPTY=DENIED\n";
    out.flush();
    if (!out) throw std::runtime_error("Cannot flush sidecar artwork QA.");
}


const char* qa_embedded_artwork_stage = "dispatch";
void record_disposable_embedded_qa_error() noexcept {
    try {
        const char* actions = std::getenv("GITHUB_ACTIONS");
        const char* runner = std::getenv("RUNNER_TEMP");
        const char* location = std::getenv("DISCOGGER_QA_MEDIA_DIR");
        if (!actions || std::string(actions) != "true" || !runner || !location) return;
        const auto root = std::filesystem::weakly_canonical(std::filesystem::u8path(runner));
        const auto folder = std::filesystem::weakly_canonical(std::filesystem::u8path(location));
        const auto prefix = root.native() + std::filesystem::path::preferred_separator;
        if (folder.native().size() <= prefix.size() ||
            _wcsnicmp(folder.c_str(), prefix.c_str(), prefix.size()) != 0 ||
            !std::filesystem::is_directory(folder)) return;
        std::ofstream out(folder / "qa-embedded-error.txt");
        if (out) out << "STAGE=" << qa_embedded_artwork_stage << '\n';
    } catch (...) {}
}
void run_disposable_embedded_artwork_qa() {
    qa_embedded_artwork_stage = "environment";
    const char* actions = std::getenv("GITHUB_ACTIONS");
    const char* runner = std::getenv("RUNNER_TEMP");
    const char* location = std::getenv("DISCOGGER_QA_MEDIA_DIR");
    if (!actions || std::string(actions) != "true" || !runner || !location || !g_discogs)
        throw std::runtime_error("Embedded artwork QA requires isolated Actions runner.");
    const auto root = std::filesystem::weakly_canonical(std::filesystem::u8path(runner));
    const auto folder = std::filesystem::weakly_canonical(std::filesystem::u8path(location));
    const auto prefix = root.native() + std::filesystem::path::preferred_separator;
    if (folder.native().size() <= prefix.size() ||
        _wcsnicmp(folder.c_str(), prefix.c_str(), prefix.size()) != 0 ||
        !std::filesystem::is_directory(folder))
        throw std::runtime_error("Unsafe embedded artwork QA directory.");
    qa_embedded_artwork_stage = "fixture";
    const auto flac = folder / "synthetic-embedded.flac";
    const auto marker = folder / "qa-embedded-artwork.txt";
    if (!std::filesystem::is_regular_file(flac) || std::filesystem::is_symlink(flac) ||
        std::filesystem::exists(marker))
        throw std::runtime_error("Unsafe embedded artwork QA fixture.");
    const auto units = flac.u8string();
    const std::string url = "file://" + std::string(units.begin(), units.end());
    auto item = metadb::get()->handle_create(url.c_str(), 0);
    if (!item || item->get_subsong_index() != 0)
        throw std::runtime_error("Embedded artwork QA lacks physical FLAC handle.");
    abort_callback_dummy abort;
    auto raw_file = [&]() {
        std::ifstream in(flac, std::ios::binary);
        if (!in) throw std::runtime_error("Cannot read synthetic FLAC artwork target.");
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };
    auto query_image = [&](GUID guid) {
        album_art_editor_instance_ptr editor =
            album_art_editor_v2::g_open(nullptr, item->get_path(), abort);
        album_art_data_ptr data = editor->query(guid, abort);
        if (!data.is_valid() || data->size() == 0)
            throw std::runtime_error("Synthetic embedded picture is empty.");
        return std::string(static_cast<const char*>(data->data()), data->size());
    };
    qa_embedded_artwork_stage = "read-front";
    const auto front = query_image(album_art_ids::cover_front);
    bool back_missing = false;
    try { (void)query_image(album_art_ids::cover_back); }
    catch (const exception_album_art_not_found&) { back_missing = true; }
    if (!back_missing)
        throw std::runtime_error("Synthetic FLAC unexpectedly has back cover.");
    file_info_impl physical_before;
    {
        service_ptr_t<input_info_reader> reader;
        input_entry::g_open_for_info_read(reader, nullptr, item->get_path(), abort);
        reader->get_info(0, physical_before, abort);
    }
    MemoryBlock picture;
    picture.set_size(front.size());
    std::memcpy(picture.get_ptr(), front.data(), front.size());
    const auto original_bytes = raw_file();
    const auto original_time = std::filesystem::last_write_time(flac);
    qa_embedded_artwork_stage = "front-noop";
    g_discogs->embed_image(picture, item, album_art_ids::cover_front, abort);
    if (raw_file() != original_bytes || std::filesystem::last_write_time(flac) != original_time)
        throw std::runtime_error("Identical embedded front cover was rewritten.");
    qa_embedded_artwork_stage = "back-add";
    g_discogs->embed_image(picture, item, album_art_ids::cover_back, abort);
    const auto updated_bytes = raw_file();
    if (updated_bytes == original_bytes ||
        query_image(album_art_ids::cover_front) != front ||
        query_image(album_art_ids::cover_back) != front)
        throw std::runtime_error("Synthetic embedded back-cover commit/readback failed.");
    file_info_impl physical_after;
    {
        service_ptr_t<input_info_reader> reader;
        input_entry::g_open_for_info_read(reader, nullptr, item->get_path(), abort);
        reader->get_info(0, physical_after, abort);
    }
    qa_embedded_artwork_stage = "metadata";
    if (!file_info::g_is_meta_equal(physical_before, physical_after) ||
        !replaygain_info::g_equal(physical_before.get_replaygain(),
                                 physical_after.get_replaygain()))
        throw std::runtime_error("Embedded artwork commit changed physical FLAC tags.");
    const auto updated_time = std::filesystem::last_write_time(flac);
    qa_embedded_artwork_stage = "back-noop";
    g_discogs->embed_image(picture, item, album_art_ids::cover_back, abort);
    if (raw_file() != updated_bytes || std::filesystem::last_write_time(flac) != updated_time)
        throw std::runtime_error("Repeated embedded back cover was rewritten.");
    qa_embedded_artwork_stage = "empty-denial";
    MemoryBlock empty;
    bool denied = false;
    try { g_discogs->embed_image(empty, item, album_art_ids::cover_back, abort); }
    catch (const foo_discogs_exception&) { denied = true; }
    if (!denied || raw_file() != updated_bytes ||
        std::filesystem::last_write_time(flac) != updated_time)
        throw std::runtime_error("Empty embedded artwork request changed FLAC.");
    qa_embedded_artwork_stage = "evidence";
    std::ofstream out(marker);
    if (!out) throw std::runtime_error("Cannot record embedded artwork QA.");
    out << "FRONT_NOOP=PASS\nBACK_ADDED=PASS\nBACK_NOOP=PASS\nEMPTY=DENIED\nMETA=UNCHANGED\n";
    out.flush();
    if (!out) throw std::runtime_error("Cannot flush embedded artwork QA.");
}

class discogger_runner_media_commands : public mainmenu_commands {
public:
    t_uint32 get_command_count() override { return 11; }
    GUID get_command(t_uint32 index) override {
        static const GUID commands[] = {
            qa_mp3_command, qa_flac_command, qa_targets_command, qa_batch_command,
            qa_populated_gui_command, qa_stale_disk_command, qa_removal_command,
            qa_partial_audit_command, qa_sidecar_artwork_command,
            qa_embedded_artwork_command, qa_live_search_probe_command
        };
        return index < 11 ? commands[index] : GUID{};
    }
    void get_name(t_uint32 index, pfc::string_base& out) override {
        static const char* names[] = {
            "Discogger QA MP3 write", "Discogger QA FLAC write",
            "Discogger QA unsafe targets", "Discogger QA incomplete batch",
            "Discogger QA populated GUI", "Discogger QA stale disk",
            "Discogger QA unsafe removals", "Discogger QA injected partial",
            "Discogger QA sidecar artwork", "Discogger QA embedded artwork",
            "Discogger QA live search probe"
        };
        out = index < 11 ? names[index] : "Invalid Discogger QA command";
    }
    bool get_description(t_uint32, pfc::string_base& out) override {
        out = "Test-only physical metadata writer for disposable GitHub runner media.";
        return true;
    }
    GUID get_parent() override { return mainmenu_groups::file; }
    void execute(t_uint32 index, service_ptr_t<service_base>) override {
        try {
            if (index > 10) throw std::runtime_error("Invalid synthetic QA action.");
            if (index == 10) run_live_search_probe_qa();
            else if (index == 9) run_disposable_embedded_artwork_qa();
            else if (index == 8) run_disposable_sidecar_artwork_qa();
            else if (index == 7) run_disposable_partial_write_audit_qa();
            else if (index == 6) run_disposable_unapproved_removal_qa();
            else if (index == 5) run_disposable_stale_disk_qa();
            else if (index == 4) run_populated_search_gui_qa();
            else if (index == 3) run_disposable_incomplete_batch_qa();
            else if (index == 2) run_disposable_target_rejection_qa();
            else run_disposable_media_writer_qa(index);
        } catch (const std::exception& e) {
            if (index == 7) record_disposable_partial_qa_error(e.what());
            if (index == 8) record_disposable_sidecar_qa_error();
            if (index == 9) record_disposable_embedded_qa_error();
            console::printf("DISCOGGER_RUNNER_MEDIA_QA_ERROR: %s", e.what());
        } catch (...) {
            if (index == 7) record_disposable_partial_qa_error("unknown failure");
            if (index == 8) record_disposable_sidecar_qa_error();
            if (index == 9) record_disposable_embedded_qa_error();
            console::print("DISCOGGER_RUNNER_MEDIA_QA_ERROR: unknown failure");
        }
    }
};
static mainmenu_commands_factory_t<discogger_runner_media_commands> runner_media_factory;
} // namespace

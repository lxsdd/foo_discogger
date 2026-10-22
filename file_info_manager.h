#pragma once

#include "error_manager.h"
#include "discogger_write_safety.h"
#include "tag_deletion_preflight.h"
#include "discogger_async_write_lease.h"
#include <functional>
#include <memory>
#include <utility>

class file_info_manager;

class read_tags_completion : public completion_notify_dummy, public ErrorManager
{
private:
	const file_info_manager &helper;

public:
	void on_completion(unsigned p_code) override;

	read_tags_completion(const file_info_manager &helper) : helper(helper) {}
};


// The SDK receives borrowed pointers: keep both the list arrays and independent
// copies of the staged physical metadata alive through asynchronous completion.
struct discogger_async_tag_payload {
    pfc::array_t<metadb_handle_ptr> items_to_update;
    pfc::array_t<file_info *> infos_to_write;
    pfc::array_t<const file_info *> original_infos;
    discogger_write_safety::stable_write_copies<file_info_impl> staged_infos;
    discogger_write_safety::stable_write_copies<file_info_impl> original_copies;
    size_t submitted_count = 0;
};

// Read back physical metadata after failure, with a bounded I/O budget.
// Metadata/ReplayGain are checked; this is not a byte-integrity proof.
discogger_write_safety::failed_write_audit discogger_audit_failed_write(
    const discogger_async_tag_payload& payload, size_t read_limit = 16) noexcept;

class write_tags_completion : public completion_notify_dummy, public ErrorManager
{
    discogger_write_safety::write_completion_gate m_gate;
    std::function<void()> m_after_success;
    discogger_write_safety::async_write_gate::lease m_write_lease;
    // The SDK may retain file_info pointers until this callback runs.
    std::shared_ptr<file_info_manager> m_finfo_manager;
    std::shared_ptr<discogger_async_tag_payload> m_write_payload;
public:
    write_tags_completion(std::function<void()> after_success,
        discogger_write_safety::async_write_gate::lease write_lease,
        std::shared_ptr<file_info_manager> finfo_manager,
        std::shared_ptr<discogger_async_tag_payload> write_payload)
        : m_after_success(std::move(after_success)), m_write_lease(std::move(write_lease)),
          m_finfo_manager(std::move(finfo_manager)), m_write_payload(std::move(write_payload)) {}
    void on_completion(unsigned p_code) override;
};



class file_info_impl_wrapper
{
public:
	file_info_impl info;
	// Snapshot taken before Discogger modifies the selected physical file_info.
	file_info_impl original_info;
	bool original_captured = false;
	bool mask = false;
};
typedef std::shared_ptr<file_info_impl_wrapper> file_info_impl_wrapper_ptr;


class file_info_manager : public std::enable_shared_from_this<file_info_manager>
{
    discogger_write_safety::initial_metadata_read m_initial_read;
public:
	metadb_handle_list items;
	pfc::array_t<file_info_impl_wrapper_ptr> info_wrappers;

	file_info_manager() {}
	file_info_manager(metadb_handle_list items) : items(items) {
		const size_t count = items.get_count();
		for (size_t i = 0; i < count; i++) {
			file_info_impl_wrapper_ptr info_wrapper = std::make_shared<file_info_impl_wrapper>();
			info_wrapper->mask = false;
			info_wrappers.append_single(std::move(info_wrapper));
		}
	};

    // A new read or a failed commit invalidates all captured physical snapshots.
    // The next writer must obtain a complete new read before staging any tags.
    void begin_physical_read() {
        m_initial_read.begin();
        discogger_write_safety::revoke_snapshot_batch(info_wrappers.get_count(),
            [&](size_t i) {
                auto& wrapper = info_wrappers[i];
                if (wrapper) {
                    wrapper->original_captured = false;
                    wrapper->mask = false;
                }
            });
    }

	// TODO: replace with async version
	bool read_infos();
	void read_infos_new();

	// Dependent artwork must start only after the async tag write succeeds.
	void write_infos(std::function<void()> after_success,
        discogger_write_safety::async_write_gate::lease write_lease,
        bool tag_deletions_approved = false);

	void copy_from(file_info_manager &other, size_t index) {
		items.add_item(other.items[index]);
		info_wrappers.append_single(other.info_wrappers[index]);
	}

	inline  size_t get_item_count() const {
		return items.get_count();
	}

	inline bool is_item_valid(size_t p_index) const {
		return info_wrappers[p_index]->mask;
	}

	inline file_info_impl & get_item(size_t p_index) {
		return info_wrappers[p_index]->info;
	}

	inline metadb_handle_ptr get_item_handle(size_t p_index) const {
		return items[p_index];
	}

	inline void validate_item(size_t p_index) {
		info_wrappers[p_index]->mask = true;
	}


    // Cancelled or failed previews must not become the next "original" tag snapshot.
    void discard_pending_changes() {
        for (size_t index = 0; index < info_wrappers.get_count(); ++index) {
            auto& wrapper = info_wrappers[index];
            if (!wrapper) continue;
            if (wrapper->original_captured) wrapper->info = wrapper->original_info;
            wrapper->mask = false;
        }
        // A cancelled/failed write may have partially changed physical files.
        // Never offer the pre-write snapshot as current without a fresh read.
        begin_physical_read();
    }

	inline void invalidate_all() {
        // Never capture empty/stale projected tags as the original physical file.
        if (!m_initial_read.ready())
            throw foo_discogs_exception("Tagging stopped: complete physical metadata could not be read. No tags were submitted.");
        if (items.get_count() != info_wrappers.get_count())
            throw foo_discogs_exception("Tagging stopped: physical metadata count changed after the read.");
		const size_t count = info_wrappers.get_count();
        // Validate the entire batch before changing any original snapshot.
        for (size_t p_index = 0; p_index < count; ++p_index) {
            if (!info_wrappers[p_index] || !info_wrappers[p_index]->mask)
                throw foo_discogs_exception("Tagging stopped: incomplete physical metadata snapshot.");
        }
		for (size_t p_index = 0; p_index < count; p_index++) {
			auto& wrapper = info_wrappers[p_index];
			wrapper->original_info = wrapper->info;
			wrapper->original_captured = true;
			wrapper->mask = false;
		}
	}
};
typedef std::shared_ptr<file_info_manager> file_info_manager_ptr;

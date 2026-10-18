#pragma once

#include "error_manager.h"
#include "discogger_write_safety.h"
#include <functional>
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


class write_tags_completion : public completion_notify_dummy, public ErrorManager
{
    discogger_write_safety::write_completion_gate m_gate;
    std::function<void()> m_after_success;
public:
    explicit write_tags_completion(std::function<void()> after_success = {})
        : m_after_success(std::move(after_success)) {}
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


class file_info_manager
{
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

	// TODO: replace with async version
	bool read_infos();
	void read_infos_new();

	// Dependent artwork must start only after the async tag write succeeds.
	void write_infos(std::function<void()> after_success = {});

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

	inline void invalidate_all() {
		const size_t count = info_wrappers.get_count();
		for (size_t p_index = 0; p_index < count; p_index++) {
			auto& wrapper = info_wrappers[p_index];
			wrapper->original_info = wrapper->info;
			wrapper->original_captured = true;
			wrapper->mask = false;
		}
	}
};
typedef std::shared_ptr<file_info_manager> file_info_manager_ptr;

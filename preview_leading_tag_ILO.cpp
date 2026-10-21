#include "stdafx.h"

#include "utils.h"
#include "preview_leading_tag_dialog.h"
#include "preview_leading_tag_ILO.h"
#include "discogger_preview_restore.h"

const int kDataCol = 2;

void ILOD_preview_leading::CopyFromOriginal() {
    // A tag may store one shared original value or one value per visible
    // mapped track. Copy the *values*, not a pointer to the array container.
    // Reject missing or stale shapes without changing the approved preview.
    if (!m_ptag_result || !ilo_get_uilist()) return;
    const size_t visible_tracks = ilo_get_vtracks_desc().size();
    if (!discogger_preview::restore_original_values(
            m_ptag_result->value, m_ptag_result->old_value, visible_tracks))
        return;
    ilo_get_uilist()->UpdateItemsAll();
}

void ILOD_preview_leading::TriggerAction() {

	if (ilo_get_uilist()->GetSelectedCount()) {
		listItemAction((CListControlOwnerData*)this, ilo_get_uilist()->GetFirstSelected());
	}
}

size_t ILOD_preview_leading::listGetItemCount(ctx_t ctx) {
	return ilo_get_vtracks_desc().size();
}

// fb2k lib - get subitems

pfc::string8 ILOD_preview_leading::listGetSubItemText(ctx_t ctx, size_t item, size_t subItem) {
	if (item >= ilo_get_finfo_count()) {
		return "";
	}

	if (subItem == 0) {

		return std::to_string(item + 1).c_str(); //# column
	}
	else if (subItem == 1) {

		return ilo_get_vtracks_desc().at(item);
	}
	else if (subItem == kDataCol) {

		pfc::array_t<string_encoded_array>* arr_sea_val = {};
		pfc::array_t<string_encoded_array>* cmp_old_sea_val = {};

		if (get_mode() == PreView::Normal) {
			arr_sea_val = &m_ptag_result->value;
		}
		else if (get_mode() == PreView::Diff) {
			arr_sea_val = &m_ptag_result->value;
			cmp_old_sea_val = &m_ptag_result->old_value;
		}
		else if (get_mode() == PreView::Original) {
			arr_sea_val = &m_ptag_result->old_value;
		}
		else {
			arr_sea_val = &m_ptag_result->value;
		}

		//need to split (no edits) or already multi
		size_t moditem = item >= arr_sea_val->get_count() ? 0 : item;

		pfc::string8 buffer;
		pfc::string8 cmp_old_buffer;

		buffer = (*arr_sea_val)[moditem].print();


		if (get_mode() == PreView::Diff) {

			if (cmp_old_sea_val) {
				size_t moditem_cmp = item >= cmp_old_sea_val->get_count() ? 0 : item;
				cmp_old_buffer = (*cmp_old_sea_val)[moditem_cmp].print();

				if (STR_EQUAL(buffer, cmp_old_buffer)) {
					buffer = "";
				}
				else if (!buffer.get_length()) {
					buffer = "<empty>";
				} 
			}
		}
		return buffer.c_str();
	}
	else {
		return "na";
	}
}
// fb2k lib - action

void ILOD_preview_leading::listItemAction(ctx_t ctx, size_t item) {

	listSubItemClicked(ctx, item, kDataCol);
}

// fb2k lib - subitem clicked

void ILOD_preview_leading::listSubItemClicked(ctx_t ctx, size_t item, size_t subItem) {

	if (listIsColumnEditable(ctx, subItem)) {

		if (!m_ptag_result->tag_entry->freeze_tag_name) {

			//edit
			ilo_get_uilist()->TableEdit_Start(item, kDataCol);
		}
	}
}

// get edit field

pfc::string8 ILOD_preview_leading::listGetEditField(ctx_t ctx, size_t item, size_t subItem, size_t& lineCount) {

	pfc::string8 out = listGetSubItemText(ctx, item, subItem);

	if (subItem == 2) {

		auto col_width = ilo_get_uilist()->GetColumnWidthF(subItem);

		CWindowDC dc(*ctx);
		SelectObjectScope fontScope(dc, ctx->GetFont());
	
		bool enable_multiline = enable_multiline_list_input(*ctx, ctx->GetFont(), col_width, out);

		lineCount = enable_multiline ? LINES_LONGFIELD : 1;
	}

	return out;
}

// set edit field

void ILOD_preview_leading::listSetEditField(ctx_t ctx, size_t item, size_t subItem, const char* val) {

	string_encoded_array sea_val;
	string_encoded_array* value = &(m_ptag_result->value[0]);

	if (value->has_array()) {

		std::vector<pfc::string8>vsplit;
		split(val, ";", 0, vsplit);
		for (auto w : vsplit) {
			sea_val.append_item(w.trim(' '));
		}
		sea_val.encode();
	}
	else {
		sea_val = val;
	}
	if (m_ptag_result->value.get_count() == 1) {

		//src common to all tracks ?

		if (!STR_EQUAL(sea_val.print(), m_ptag_result->value[0].print())) {

			//build per track values

			size_t newcount = ilo_get_uilist()->GetItemCount();

			string_encoded_array previtem = m_ptag_result->value[0];

			m_ptag_result->value.force_reset();

			for (size_t it = 0; it < newcount; it++) {
				m_ptag_result->value.add_item(previtem);
			}
		}
	}
	m_ptag_result->value[item] = sea_val;

}

// column editable

bool ILOD_preview_leading::listIsColumnEditable(ctx_t ctx, size_t subItem) {

	return (get_mode() == PreView::Normal && subItem == kDataCol);
}

// mode set/get

PreView ILOD_preview_leading::get_mode() {

	return m_viewmode;
}

//todo: mutex?

void ILOD_preview_leading::SetMode(PreView view_mode) {

	m_viewmode = view_mode;
}

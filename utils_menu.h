#pragma once


static void build_sub_menu(HMENU parent_menu, UINT submenu_id, std::vector<std::pair<std::string, std::string>> &v, const TCHAR * wmenutitle, size_t wmenu_size) {

	// Win32 copies the mutable buffer when InsertMenuItem is called.
	std::basic_string<TCHAR> title_storage = (wmenu_size && wmenutitle)
		? wmenutitle : _T("Web release artist&s page");

	HMENU submenu;
	MENUITEMINFO submenu_info;
	submenu = CreatePopupMenu();
	submenu_info = { 0 };
	submenu_info.cbSize = sizeof(MENUITEMINFO);
	submenu_info.fMask = MIIM_SUBMENU | MIIM_STRING | MIIM_ID;
	submenu_info.hSubMenu = submenu;
	submenu_info.dwTypeData = title_storage.data(); // _T("Web artists page");
	submenu_info.wID = submenu_id;

	for (size_t n = 0; n < v.size(); n++) {

		uAppendMenu(submenu, MF_STRING, submenu_id + n, v.at(n).second.c_str());
	}
	InsertMenuItem(parent_menu, submenu_id, true, &submenu_info);
}

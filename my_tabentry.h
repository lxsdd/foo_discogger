#pragma once

class tab_entry
{
public:
	LPCSTR m_pszName;
	DLGPROC m_lpDialogFunc;
	WORD m_nID;

	tab_entry(LPCSTR pszName, DLGPROC lpDialogFunc, WORD nID) {
		m_pszName = pszName;
		m_lpDialogFunc = lpDialogFunc;
		m_nID = nID;
	}
	tab_entry() {
		m_pszName = "";
		m_lpDialogFunc = nullptr;
		m_nID = 0;
	}

	HWND CreateTabDialog(HWND hWndParent, LPARAM lInitParam = 0) {
		PFC_ASSERT(m_lpDialogFunc != nullptr);
		HWND dlg = uCreateDialog(m_nID, hWndParent, m_lpDialogFunc, lInitParam);
		return dlg;
	}
};
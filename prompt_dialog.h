#pragma once

#include "resource.h"
#include "helpers\DarkMode.h"

class CPromptDialog : public MyCDialogImpl<CPromptDialog>
{
private:
	pfc::string8 m_ask;
	pfc::string8 *mp_result;
	fb2k::CDarkModeHooks m_dark;

public:
	enum { IDD = IDD_DIALOG_PROMPT };

	MY_BEGIN_MSG_MAP(CPromptDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnButtonOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	MY_END_MSG_MAP()

	CPromptDialog() : m_ask(), mp_result(nullptr) {}
	CPromptDialog(const pfc::string8 &ask, pfc::string8 &result) : m_ask(ask), mp_result(&result) {}
	~CPromptDialog() {};

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnButtonOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	void SetAskResult(pfc::string8 ask, pfc::string8* result) { m_ask = ask; mp_result = result; }

	void enable(bool v) override {}
};

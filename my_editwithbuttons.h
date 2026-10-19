#pragma once

#include "resource.h"

#include "foo_discogs.h"
#include "tags.h"

#include "libPPUI/CEditWithButtons.h"

class CMyEditWithButtons : public CEditWithButtons {

public:

	typedef CEditWithButtons TParent;
	BEGIN_MSG_MAP_EX(CMyEditWithButtons)
	MSG_WM_SETFOCUS(OnSetFocus)
	MSG_WM_KILLFOCUS(OnKillFocus)
	MSG_WM_GETDLGCODE(OnEditGetDlgCode)
	CHAIN_MSG_MAP(TParent)
	END_MSG_MAP()

	UINT OnEditGetDlgCode(LPMSG lpMsg) {
		if (!lpMsg) return FALSE;
		SetMsgHandled(!!NoEscSteal); return 0;
	}

	CMyEditWithButtons() : CEditWithButtons(), m_stdf_call_history(nullptr)
	{
		m_bMsgHandled = true;
	}
	void OnSetFocus(HWND hWnd)
	{
		DefWindowProc();
		RefreshConditionedInvalidation();
		if (IsKeyPressed(VK_TAB)) {
			PostMessage(EM_SETSEL, 0, -1);
		}
	}

	void OnKillFocus(HWND hWnd)
	{
		DefWindowProc();
		RefreshConditionedInvalidation();
	}

	// Current libPPUI no longer supplies this extension. Preserve Wine light
	// theme redraws on initialization and focus transitions.
	void SetConditionedInvalidation(std::function<bool()> predicate) {
		m_conditioned_invalidation = std::move(predicate);
		RefreshConditionedInvalidation();
	}
	void RefreshConditionedInvalidation() {
		if (m_hWnd && m_conditioned_invalidation && m_conditioned_invalidation()) {
			Invalidate(); // redraw the edit and its embedded libPPUI buttons
		}
	}

	void SetHistoryHandler(std::function<bool(HWND, wchar_t* wchart)> stdf_call_history) {

		m_stdf_call_history = stdf_call_history;

		std::wstring histValCopy(L"");

		auto condition_hist = [histValCopy](const wchar_t* txt) -> bool {
			return CONF.history_enabled();
		};

		auto handler_onHistKey = [this, histValCopy] {

			wchar_t wstr[MAX_PATH + 1] = {};
			GetWindowText(wstr, MAX_PATH);
			bool updated = this->m_stdf_call_history(this->m_hWnd, wstr);
			if (updated) {
				std::wstring updValCopy(wstr);
				SetWindowText(updValCopy.c_str());
				::SetFocus(this->m_hWnd);
				PostMessage(EM_SETSEL, updValCopy.size(), updValCopy.size());
			}
			return updated;
		};

		AddButton(L"more", handler_onHistKey, condition_hist, L"\x2026");
	}

	void SetEnterEscHandlers() {

		auto handler_onEnterKey = [this] {
			return false;
		};

		std::wstring clearValCopy(L"");
		auto handler_onClearKey = [this, clearValCopy] {
			this->SetWindowText(clearValCopy.c_str());
			return false;
		};

		auto condition_clear = [clearValCopy](const wchar_t* txt) -> bool {
			return clearValCopy != txt;
		};

		this->onEnterKey = handler_onEnterKey;
		AddButton(L"clear", handler_onClearKey, condition_clear, L"\x00D7");
	}

	void FocusClear() {
		HWND wnd = this->GetNextDlgTabItem(m_hWnd, FALSE);
		if (wnd) {
			::SetFocus(wnd);
		}
	}

	void SetEnterOverride(std::function<bool()>stdf_func) {
		if (stdf_func) {

			this->onEnterKey = stdf_func;
		}
		else {
			this->onEnterKey = nullptr;
			this->onEscKey = nullptr;
		}
	}

protected:

	std::function<bool(HWND, wchar_t* wstr)> m_stdf_call_history;
	std::function<bool()> m_conditioned_invalidation;

};

class CMyEditQueryWithButtons : public CMyEditWithButtons {

public:

	typedef CMyEditWithButtons TParent;
	BEGIN_MSG_MAP_EX(CMyEditQueryWithButtons)
		MSG_WM_GETDLGCODE(OnEditGetDlgCode)
		CHAIN_MSG_MAP(TParent)
	END_MSG_MAP()

	UINT OnEditGetDlgCode(LPMSG lpMsg) {
		if (!lpMsg) return FALSE;
		if (!m_stdf_call_history_query_shortcut) {
			SetMsgHandled(!!NoEscSteal); return 0;
		}
		switch (lpMsg->message) {
		case WM_KEYDOWN:
			[[fallthrough]];
		case WM_SYSKEYDOWN:

			if (GetHotkeyModifierFlags() == MOD_CONTROL) {
				switch (lpMsg->wParam) {
				case '0':
					[[fallthrough]];
				case '1':
					[[fallthrough]];
				case '2':
					[[fallthrough]];
				case '3':
					[[fallthrough]];
				case '4':
					[[fallthrough]];
				case '5':
					[[fallthrough]];
				case '9': {

					wchar_t wstr[MAX_PATH + 1] = {};
					GetWindowText(wstr, MAX_PATH);
					pfc::string key; key.add_char(lpMsg->wParam);
					bool updated = this->m_stdf_call_history_query_shortcut(this->m_hWnd, wstr, atoi(key));
					if (updated) {
						std::wstring updValCopy(wstr);
						SetWindowText(updValCopy.c_str());
						::SetFocus(this->m_hWnd);
					}
					SetMsgHandled(!!NoEscSteal);
					return 0;
				}

				default:
					SetMsgHandled(!!NoEscSteal); return 0;
				}
			}
			[[fallthrough]];
		default:
			SetMsgHandled(!!NoEscSteal); return 0;
		}
	}

	CMyEditQueryWithButtons() : CMyEditWithButtons()
	{
		m_bMsgHandled = true;
	}

	void SetQueryHistoryHandler(std::function<bool(HWND/*, HMENU*/, wchar_t* wchart, size_t& ins_pos)> stdf_call_query_history) {

		m_stdf_call_query_history = stdf_call_query_history;

		std::wstring histValCopy(L"");

		auto condition_hist = [histValCopy](const wchar_t* txt) -> bool {
			return CONF.history_enabled();
		};

		auto handler_onHistKey = [this, histValCopy] {

			wchar_t wstr[MAX_PATH + 1] = {};
			GetWindowText(wstr, MAX_PATH);
			size_t ins_pos = SIZE_MAX;
			bool updated = this->m_stdf_call_query_history(this->m_hWnd, wstr, ins_pos);
			if (updated) {
				std::wstring updValCopy(wstr);
				SetWindowText(updValCopy.c_str());
				::SetFocus(this->m_hWnd); //todo?
				if (ins_pos != SIZE_MAX) {

					SendMessage(this->m_hWnd, EM_SETSEL, (WPARAM)ins_pos, (LPARAM)ins_pos);
				}
			}
			return updated;
		};

		this->WantAllKeys = true;
		AddButton(L"search query", handler_onHistKey, [](const wchar_t* txt) -> bool {
			return true; }, L"\x0192");
	}

	void SetQueryHistoryHandlerShortCut(std::function<bool(HWND hwnd, wchar_t* editval, size_t ndx)> stdf_call_history_query_shortcut) {
		m_stdf_call_history_query_shortcut = stdf_call_history_query_shortcut;
	}

private:

	std::function<bool(HWND, wchar_t* wstr, size_t& ins_pos)> m_stdf_call_query_history;
	std::function<bool(HWND hwnd, wchar_t* editval, size_t ndx)> m_stdf_call_history_query_shortcut;
};

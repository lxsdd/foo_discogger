#pragma once

class HeaderStatic : public CWindowImpl<HeaderStatic, CStatic> {
public:
	HeaderStatic() : m_iLeftSpacing(8) {}
	BOOL SubclassWindow(HWND hWnd);

	DECLARE_WND_SUPERCLASS(0, CStatic::GetWndClassName())
	BEGIN_MSG_MAP(HeaderStatic)
		MSG_WM_PAINT(OnPaint)
	END_MSG_MAP()

	void OnPaint(CDCHandle dcDummy);
	void PaintHeader();

private:
	int m_iLeftSpacing;
	CFont m_cFont;
};

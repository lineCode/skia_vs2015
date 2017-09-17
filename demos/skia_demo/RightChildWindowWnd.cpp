#include "RightChildWindowWnd.h"


CRightChildWindowWnd::CRightChildWindowWnd(void)
{
}


CRightChildWindowWnd::~CRightChildWindowWnd(void)
{

}

LPCTSTR CRightChildWindowWnd::GetWindowClassName() const
{
	return TEXT("UIRightChildWnd");
};

UINT CRightChildWindowWnd::GetClassStyle() const
{
	return UI_CLASSSTYLE_CHILD;
};

void CRightChildWindowWnd::OnFinalMessage(HWND /*hWnd*/)
{
	delete this;
};

LRESULT CRightChildWindowWnd::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_CREATE) {
		LOGFONT lf;
		memset(&lf, 0, sizeof(LOGFONT));
		lf.lfHeight = 37;
		lf.lfWeight = 0;
		lf.lfWeight = FW_NORMAL;
		lf.lfItalic = FALSE;
		lf.lfUnderline = FALSE;
		lf.lfCharSet = GB2312_CHARSET;

		lstrcpy(lf.lfFaceName, TEXT("Î¢ÈíÑÅºÚ"));
		hFont = CreateFontIndirect(&lf);
	}
	if (uMsg == WM_DESTROY) {
		DeleteObject(hFont);
		::PostQuitMessage(0L);
	}
	if (uMsg == WM_ERASEBKGND) {
		HDC hdc = (HDC)wParam;
		RECT rc;
		GetClientRect(m_hWnd, &rc);
		FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

		
		return 1L;
	}
	// Or when the window is clicked.
	if (uMsg == WM_LBUTTONDOWN) {
		SetFocus(m_hWnd);
		// Msg was handled, return zero.
		return 0;
	}
	if (uMsg == WM_SETFOCUS)
	{
		OutputDebugStringA("right child: setfocus\n");
	}
	if (uMsg == WM_KEYDOWN)
	{
	}
	if (uMsg == WM_SIZE) {
	}
	if (uMsg == WM_PAINT) {
		
		PAINTSTRUCT ps = { 0 };
		HDC hdc = ::BeginPaint(m_hWnd, &ps);
		// paint here
		drawText(hdc);

		::EndPaint(m_hWnd, &ps);

		
		
		return 1;
	}
	return CWindowWnd::HandleMessage(uMsg, wParam, lParam);
}
void CRightChildWindowWnd::drawText(HDC hdc)
{
	TCHAR text[64] = TEXT("sjig");
	int textLen = lstrlen(text);
	

	RECT rc;
	rc.left = 20;
	rc.top = 100;
	rc.right = rc.left + 100;
	rc.bottom = rc.top + 40;
	HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
	
	DrawText(hdc, text, textLen, &rc, DT_CENTER);
	
	SelectObject(hdc, hOldFont);
}



#include "FrameWindowWnd.h"

#include "skwin\LyraWindow.h"

#include "SkTypes.h"

#include "SkiaTest.h"



CFrameWindowWnd::CFrameWindowWnd(void)
{
}


CFrameWindowWnd::~CFrameWindowWnd(void)
{
}

LPCTSTR CFrameWindowWnd::GetWindowClassName() const 
{ 
	return TEXT("UIMainFrame"); 
};
   
UINT CFrameWindowWnd::GetClassStyle() const 
{ 
	return UI_CLASSSTYLE_FRAME; 
};
   
void CFrameWindowWnd::OnFinalMessage(HWND /*hWnd*/) 
{ 
	delete this; 
};

LRESULT CFrameWindowWnd::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if( uMsg == WM_CREATE ) {
		//SetIcon(IDR_MAINFRAME);
		m_pLeftChild = new CLeftChildWindowWnd();
		m_pLeftChild->Create(m_hWnd, TEXT("leftchild"), UI_WNDSTYLE_CHILD | WS_BORDER, 0);

		m_pRightChild = new CRightChildWindowWnd();
		m_pRightChild->Create(m_hWnd, TEXT("rightchild"), UI_WNDSTYLE_CHILD | WS_BORDER, 0);

	}
	if( uMsg == WM_DESTROY ) {
		::PostQuitMessage(0L);
	}
	if (uMsg == WM_ERASEBKGND) {
		HDC hdc = (HDC)wParam;
		RECT rc;
		GetClientRect(m_hWnd, &rc);
		SetMapMode(hdc, MM_ANISOTROPIC);
		SetWindowExtEx(hdc, 100, 100, NULL);
		SetViewportExtEx(hdc, rc.right, rc.bottom, NULL);
		FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

		MoveToEx(hdc, rc.left, rc.top+10, NULL);
		LineTo(hdc, rc.right, rc.top+10);

		return 1L;
	}
	if (uMsg == WM_KEYDOWN)
	{
		
	}
	if( uMsg == WM_SETFOCUS ) {
			
	}
	if( uMsg == WM_SIZE ) {
		RECT rcClient;
		::GetClientRect(m_hWnd, &rcClient);
		int w = rcClient.right - rcClient.left;
		int h = rcClient.bottom - rcClient.top;

		int padding = 40;
		int space = 40;

		int wChild = (w - 2 * padding - space) / 2;
		int hChild = (h - 2 * padding);

		int xLeftChild = padding;
		int yLeftChild = padding;
		int xRightChild = padding + space + wChild;
		int yRightChild = padding;
		
		MoveWindow(m_pLeftChild->GetHWND(), xLeftChild, yLeftChild, wChild, hChild, TRUE);
		MoveWindow(m_pRightChild->GetHWND(), xRightChild, yRightChild, wChild, hChild, TRUE);

		return 0;
	}

	return CWindowWnd::HandleMessage(uMsg, wParam, lParam);
}



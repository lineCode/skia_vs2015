
#include "FrameWindowWnd.h"

#include "skwin\LyraWindow.h"

#include "SkTypes.h"

#include "SkiaTest.h"

#define ID_TIMER_REDRAW  101


CFrameWindowWnd *frame;
CFrameWindowWnd::CFrameWindowWnd(void)
	: m_hasRedrawTimer(false)
{
	frame = NULL;
}


CFrameWindowWnd::~CFrameWindowWnd(void)
{
	frame = NULL;
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
	m_pPaintWnd->UnInit();
	delete this; 
};

LRESULT CFrameWindowWnd::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if( uMsg == WM_CREATE ) {
		//SetIcon(IDR_MAINFRAME);
		
		m_pPaintWnd = new CLyraWindow(this->GetHWND(), NULL);
		if (m_pPaintWnd)
		{
			m_pPaintWnd->Init();
		}
	}
	if( uMsg == WM_DESTROY ) {
		this->KillReDrawTimer();
		::PostQuitMessage(0L);
	}
	if (uMsg == WM_KEYDOWN)
	{
		if (wParam == VK_UP)
		{
			int type = 0;
			m_pPaintWnd->setDeviceType((DeviceType)type);
			InvalidateRect(m_hWnd, NULL, TRUE);
			return 0;
		}
		if (wParam == VK_DOWN)
		{
			int type = 2;
			m_pPaintWnd->setDeviceType((DeviceType)type);
			InvalidateRect(m_hWnd, NULL, TRUE);
			return 0;
		}
		if (wParam == VK_RIGHT)
		{
			CSkiaTest::getInstance()->next();
			m_pPaintWnd->forceInvalAll();
			InvalidateRect(m_hWnd, NULL, TRUE);			
			return 0;
		}
		if (wParam == VK_LEFT)
		{
			CSkiaTest::getInstance()->prev();
			m_pPaintWnd->forceInvalAll();
			InvalidateRect(m_hWnd, NULL, TRUE);
			//m_pPaintWnd->forceInvalAll();
			return 0;
		}
	}
	if( uMsg == WM_SETFOCUS ) {
			
	}
	if (uMsg == WM_TIMER)
	{
		m_pPaintWnd->forceInvalAll();
		InvalidateRect(m_hWnd, NULL, TRUE);
		return 0;
	}
	if( uMsg == WM_SIZE ) {
		RECT rcClient;
		::GetClientRect(m_hWnd, &rcClient);
		int w = rcClient.right - rcClient.left;
		int h = rcClient.bottom - rcClient.top;
		m_pPaintWnd->resize(w, h);
		return 0;
	}
	if( uMsg == WM_ERASEBKGND ) {
		return 1;
	}
	if( uMsg == WM_PAINT ) {
		{
			PAINTSTRUCT ps = { 0 };
			HDC hdc = ::BeginPaint(m_hWnd, &ps);
			m_pPaintWnd->doPaint(hdc);
			::EndPaint(m_hWnd, &ps);
			
		}
		return 1;
	}
	return CWindowWnd::HandleMessage(uMsg, wParam, lParam);
}

void CFrameWindowWnd::SetReDrawTimer(int elapse)
{
	if (!m_hasRedrawTimer)
	{
		::SetTimer(m_hWnd, ID_TIMER_REDRAW, elapse, NULL);
		m_hasRedrawTimer = true;
	}
}
void CFrameWindowWnd::KillReDrawTimer()
{
	if (m_hasRedrawTimer)
	{
		::KillTimer(m_hWnd, ID_TIMER_REDRAW);
		m_hasRedrawTimer = false;
	}
}



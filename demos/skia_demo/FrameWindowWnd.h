#ifndef __FRAMEWINDOWWND_H__
#define __FRAMEWINDOWWND_H__

#include <Windows.h>
#include "WindowWnd.h"

class CLyraWindow;
class CFrameWindowWnd : public CWindowWnd
{
public:
	CFrameWindowWnd(void);
	~CFrameWindowWnd(void);

	LPCTSTR GetWindowClassName() const;

	UINT GetClassStyle() const;

	void OnFinalMessage(HWND /*hWnd*/);

	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

	void SetReDrawTimer(int elapse);
	void KillReDrawTimer();

protected:
	//
	CLyraWindow * m_pPaintWnd;
	bool m_hasRedrawTimer;
};

extern CFrameWindowWnd *frame;

#endif//__FRAMEWINDOWWND_H__

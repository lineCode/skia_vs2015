#ifndef __LEFTCHILDWINDOWWND_H__
#define __LEFTCHILDWINDOWWND_H__

#include "WindowWnd.h"

class CLyraWindow;
class CLeftChildWindowWnd : public CWindowWnd
{
public:
	CLeftChildWindowWnd(void);
	~CLeftChildWindowWnd(void);

	LPCTSTR GetWindowClassName() const;

	UINT GetClassStyle() const;

	void OnFinalMessage(HWND /*hWnd*/);

	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

	void SetReDrawTimer(int elapse);
	void KillReDrawTimer();

protected:
	CLyraWindow * m_pPaintWnd;
	int fDeviceType;

	bool m_hasRedrawTimer;
};
extern CLeftChildWindowWnd *frame;
#endif//__LEFTCHILDWINDOWWND_H__

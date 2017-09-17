#ifndef __RIGHTCHILDWINDOWWND_H__
#define __RIGHTCHILDWINDOWWND_H__

#include "WindowWnd.h"

class CRightChildWindowWnd : public CWindowWnd
{
public:
	CRightChildWindowWnd(void);
	~CRightChildWindowWnd(void);

	LPCTSTR GetWindowClassName() const;

	UINT GetClassStyle() const;

	void OnFinalMessage(HWND /*hWnd*/);

	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
	void drawText(HDC hdc);
	HFONT hFont;
};

#endif//__RIGHTCHILDWINDOWWND_H__

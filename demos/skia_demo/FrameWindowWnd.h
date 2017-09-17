#ifndef __FRAMEWINDOWWND_H__
#define __FRAMEWINDOWWND_H__

#include <Windows.h>
#include "WindowWnd.h"

#include "LeftChildWindowWnd.h"
#include "RightChildWindowWnd.h"

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


protected:
	CLeftChildWindowWnd * m_pLeftChild;
	CRightChildWindowWnd * m_pRightChild;
};

#endif//__FRAMEWINDOWWND_H__

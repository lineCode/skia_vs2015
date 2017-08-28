// skia_demo.cpp : 定义应用程序的入口点。
//

#include <Windows.h>
#include "skia_demo.h"

#include "FrameWindowWnd.h"

int APIENTRY WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR    lpCmdLine,
	int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	CWindowWnd::SetResourceInstance(hInstance);



	CFrameWindowWnd* pFrame = new CFrameWindowWnd();
	if( pFrame == NULL ) return 0;
	pFrame->Create(NULL, TEXT("skia_demo : "), UI_WNDSTYLE_FRAME, WS_EX_WINDOWEDGE);
	frame = pFrame;

	CWindowWnd::MessageLoop();


	return 0;
}


#pragma once
#include "Essence.h"

class FApplication;

namespace Win32 {

struct Message {
	HWND	hWnd;
	UINT	message;
	WPARAM	wParam;
	LPARAM	lParam;
};

using				CustomMessageFunc = bool(*)(Message const&);

void				SetCustomMessageFunc(CustomMessageFunc func);
LRESULT CALLBACK	WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
int					Run(FApplication* app, HINSTANCE hInstance, int nCmdShow);
HWND				GetWndHandle();

}
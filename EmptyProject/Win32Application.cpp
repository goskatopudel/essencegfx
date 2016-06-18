#include "Win32Application.h"
#include "Application.h"

namespace Win32 {

HWND				Hwnd;
FApplication*		Application;
CustomMessageFunc	MessageFunc;

void	SetCustomMessageFunc(CustomMessageFunc func) {
	MessageFunc = func;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (MessageFunc && MessageFunc({ hWnd, message, wParam, lParam })) {
		return true;
	}

	switch (message)
	{
	case WM_SIZE:
		{
			u32 WndWidth = (UINT)(UINT64)lParam & 0xFFFF;
			u32 WndHeight = (UINT)(UINT64)lParam >> 16;

			if (WndWidth != GApplication::WndWidth || WndHeight != GApplication::WndHeight) {
				GApplication::WndWidth = WndWidth;
				GApplication::WndHeight = WndHeight;
				GApplication::WindowSizeChanged = 1;
			}
			return 0;
		}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	// Handle any messages the switch statement didn't.
	return DefWindowProc(hWnd, message, wParam, lParam);
}

int		Run(FApplication* app, HINSTANCE hInstance, int nCmdShow) {
	Application = app;
	// Initialize the window class.
	WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = L"EssenceClass";
	RegisterClassEx(&windowClass);

	RECT windowRect = { 0, 0, static_cast<LONG>(GApplication::WndWidth), static_cast<LONG>(GApplication::WndHeight) };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	// Create the window and store a handle to it.
	Hwnd = CreateWindow(
		windowClass.lpszClassName,
		GApplication::WndTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,		// We have no parent window.
		nullptr,		// We aren't using menus.
		hInstance,
		nullptr);

	Application->CoreInit();

	ShowWindow(Hwnd, nCmdShow);

	// Main sample loop.
	MSG msg = {};
	while (msg.message != WM_QUIT && Application->CoreUpdate())
	{
		// Process any messages in the queue.
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT) {
				break;
			}
		}
	}

	Application->CoreShutdown();

	// Return this part of the WM_QUIT message to Windows.
	return static_cast<char>(msg.wParam);
}

HWND GetWndHandle() {
	return Hwnd;
}

}
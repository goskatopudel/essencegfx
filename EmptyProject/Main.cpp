#include "Essence.h"
#include "Application.h"
#include "Win32Application.h"


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	FApplication SampleApp(L"Essence2", 1024, 768);
	return Win32::Run(&SampleApp, hInstance, nCmdShow);
}
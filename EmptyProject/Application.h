#pragma once
#include "Essence.h"

namespace GApplication {
extern bool				WindowSizeChanged;
extern u32				WndWidth;
extern u32				WndHeight;
extern const wchar_t*	WndTitle;
extern i64				Time;
extern i64				CpuFrequency;
}

class FApplication {
public:

	FApplication(const wchar_t*	wndTitle, u32 width, u32 height) {
		GApplication::WndWidth = width;
		GApplication::WndHeight = height;
		GApplication::WndTitle = wndTitle;
		GApplication::WindowSizeChanged = 0;
	}

	FApplication(FApplication const&) = delete;

	void Init();
	void Shutdown();
	bool Update();
};

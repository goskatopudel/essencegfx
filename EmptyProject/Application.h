#pragma once
#include "Essence.h"

class FApplication {
public:
	u32				WndWidth;
	u32				WndHeight;
	const wchar_t*	WndTitle;
	i64				Time;
	i64				CpuFrequency;

	FApplication(const wchar_t*	wndTitle, u32 width, u32 height) :
		WndTitle(wndTitle), WndWidth(width), WndHeight(height)
	{
	}

	FApplication(FApplication const&) = delete;

	void Init();
	void Shutdown();
	bool Update();

};

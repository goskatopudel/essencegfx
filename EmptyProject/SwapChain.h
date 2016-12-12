#pragma once
#include "Essence.h"
#include <d3d12.h>
#include "Device.h"
#include "Resource.h"

class WinSwapChain {
public:
	static const u32 MAX_BACKBUFFERS_NUM = 15;

	u32 VSync : 1;
	u32 BackbuffersNum : 4;
	u32 WaitToVBlank : 1;
	u32 CurrentBackbufferIndex : 4;
	u32 MaxLatency : 4;
	DXGI_FORMAT BackbufferFormat;
	HANDLE VBlankWaitable;

	unique_com_ptr<IDXGISwapChain3> SwapChain;
	eastl::vector<unique_com_ptr<ID3D12Resource>> RawBackbuffers;
	eastl::vector<FGPUResourceRef> Backbuffers;

	WinSwapChain();
	~WinSwapChain();
	void Present();
	void Resize(u32 width, u32 height);
};

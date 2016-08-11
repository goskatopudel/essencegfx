#pragma once
#include "Essence.h"
#include <d3d12.h>
#include <dxgi1_5.h>

#include "AssertionMacros.h"

#include <EASTL\unique_ptr.h>
#include <EASTL\shared_ptr.h>
#include <EASTL\vector.h>

struct com_deleter
{
	void operator()(IUnknown* p) const // We use a const argument type in order to be most flexible with what types we accept. 
	{
		if (p)
			p->Release();
	}
};

template<class T>
using unique_com_ptr = eastl::unique_ptr<T, com_deleter>;

typedef D3D12_GPU_VIRTUAL_ADDRESS	GPU_VIRTUAL_ADDRESS;

inline bool operator==(D3D12_CPU_DESCRIPTOR_HANDLE A, D3D12_CPU_DESCRIPTOR_HANDLE B) {
	return A.ptr == B.ptr;
}

inline bool operator!=(D3D12_CPU_DESCRIPTOR_HANDLE A, D3D12_CPU_DESCRIPTOR_HANDLE B) {
	return A.ptr != B.ptr;
}

class D3D12Device {
public:
	unique_com_ptr<ID3D12Device>		D12Device;
	unique_com_ptr<IDXGIAdapter3>		Adapter;
	void InitFromAdapterIndex(u32 adapterIndex, D3D_FEATURE_LEVEL featureLevel);
	void InitWarp();
};

class FGPUResource;
class WinSwapChain;

void							SetDebugName(ID3D12DeviceChild * child, const wchar_t * name);
void							InitDevices(u32 adapterIndex, bool debugMode);
D3D12Device*					GetPrimaryDevice();
WinSwapChain*					GetSwapChain();
FGPUResource*					GetBackbuffer();
DXGI_FORMAT						GetBackbufferFormat();
void							ListAdapters();
DXGI_QUERY_VIDEO_MEMORY_INFO	GetLocalMemoryInfo();
DXGI_QUERY_VIDEO_MEMORY_INFO	GetNonLocalMemoryInfo();

const u32 ALL_SUBRESOURCES = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
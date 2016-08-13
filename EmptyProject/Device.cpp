#include "Device.h"
#include "Print.h"
#include "Win32Application.h"
#include "Commands.h"
#include "PointerMath.h"
#include "SwapChain.h"

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "D3D12.lib")

IDXGIFactory4* GetDXGIFactory() {
	static IDXGIFactory4* DXGIFactory;
	if (!DXGIFactory) {
		VERIFYDX12(CreateDXGIFactory1(IID_PPV_ARGS(&DXGIFactory)));
	}
	return DXGIFactory;
}

void SetDebugName(ID3D12DeviceChild * child, const wchar_t * name) {
	if (child != nullptr && name != nullptr) {
		VERIFYDX12(child->SetPrivateData(WKPDID_D3DDebugObjectNameW, (UINT)wcslen(name), name));
	}
}

void PrintDeviceInfo(ID3D12Device* device) {

	D3D12_FEATURE_DATA_D3D12_OPTIONS options;
	VERIFYDX12(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)));
	D3D12_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT addressSupport;
	VERIFYDX12(device->CheckFeatureSupport(D3D12_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT, &addressSupport, sizeof(addressSupport)));

	PrintFormated(
		L"Node count: %u\n"
		L"Node sharing tier: %u\n"
		L"Resource binding tier: %d\n"
		L"Resource heap tier: %d\n"
		L"Tiled resources tier: %d\n"
		L"Conservative rasterization tier: %d\n"
		L"Virtual address bits: %d\n"
		L"ROVs support: %d\n"
		L"Standard swizzle 64kb support: %d\n"
		L"Typed UAV load additional formats: %d\n"
		L"VP and RT array index with no GS: %d\n",
		device->GetNodeCount(),
		options.CrossNodeSharingTier,
		options.ResourceBindingTier,
		options.ResourceHeapTier,
		options.TiledResourcesTier,
		options.ConservativeRasterizationTier,
		options.MaxGPUVirtualAddressBitsPerResource,
		options.ROVsSupported,
		options.StandardSwizzle64KBSupported,
		options.TypedUAVLoadAdditionalFormats,
		options.VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation
		);
}

void PrintAdapterExInfo(IDXGIAdapter3* adapter) {
	DXGI_ADAPTER_DESC2 desc;
	adapter->GetDesc2(&desc);

	PrintFormated(
		L"Device id: %u\n"
		L"System memory: %llu Mb\n"
		L"Video memory: %llu Mb\n"
		L"Shared memory: %llu Mb\n",
		desc.DeviceId,
		Megabytes(desc.DedicatedSystemMemory),
		Megabytes(desc.DedicatedVideoMemory),
		Megabytes(desc.SharedSystemMemory));
}

void ListAdapters() {
	IDXGIFactory4* DXGIFactory = GetDXGIFactory();

	u32 adapterIndex = 0u;
	IDXGIAdapter1* adapter1Ptr = nullptr;
	VERIFYDX12(DXGIFactory->EnumAdapters1(adapterIndex, &adapter1Ptr));


	while (SUCCEEDED(DXGIFactory->EnumAdapters1(adapterIndex, &adapter1Ptr))) {
		DXGI_ADAPTER_DESC1 desc;
		adapter1Ptr->GetDesc1(&desc);
		
		PrintFormated(L"================\n[%u]\nLUID: %d%d, Description: %s\n",
			adapterIndex, desc.AdapterLuid.HighPart, desc.AdapterLuid.LowPart, desc.Description);

		IDXGIAdapter3* adapter3Ptr = nullptr;
		VERIFYDX12(adapter1Ptr->QueryInterface(IID_PPV_ARGS(&adapter3Ptr)));

		auto CheckFeatureSupport = [adapter3Ptr](D3D_FEATURE_LEVEL fl) {
			eastl::unique_ptr<ID3D12Device, com_deleter> D12Device;
			D3D_FEATURE_LEVEL MinFeatureLevel = fl;
			if (SUCCEEDED(D3D12CreateDevice(
				adapter3Ptr,
				MinFeatureLevel,
				IID_PPV_ARGS(D12Device.get_init())
				))) {
				return true;
			}
			return false;
		};

		bool SupportsFeature11 = CheckFeatureSupport(D3D_FEATURE_LEVEL_11_0);
		bool SupportsFeature12 = CheckFeatureSupport(D3D_FEATURE_LEVEL_12_0);
		PrintFormated(
			L"D3D_FEATURE_LEVEL_11_0: %d\n"
			L"D3D_FEATURE_LEVEL_12_0: %d\n",
			SupportsFeature11, 
			SupportsFeature12);

		PrintAdapterExInfo(adapter3Ptr);
		if (SupportsFeature11) {
			eastl::unique_ptr<ID3D12Device, com_deleter> D12Device;
			if (SUCCEEDED(D3D12CreateDevice(
				adapter3Ptr,
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(D12Device.get_init())
				))) {
				PrintDeviceInfo(D12Device.get());
			}
		}

		adapterIndex++;
	}

	PrintFormated(L"================\n");
}

void D3D12Device::InitFromAdapterIndex(u32 adapterIndex, D3D_FEATURE_LEVEL featureLevel) {
	IDXGIAdapter1* adapter1Ptr = nullptr;
	VERIFYDX12(GetDXGIFactory()->EnumAdapters1(adapterIndex, &adapter1Ptr));

	VERIFYDX12(D3D12CreateDevice(
		adapter1Ptr,
		featureLevel,
		IID_PPV_ARGS(D12Device.get_init())
		));

	VERIFYDX12(adapter1Ptr->QueryInterface(IID_PPV_ARGS(Adapter.get_init())));
}

void D3D12Device::InitWarp() {
	IDXGIAdapter *warpAdapter = nullptr;
	VERIFYDX12(GetDXGIFactory()->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

	check(D12Device.get() == nullptr);

	VERIFYDX12(D3D12CreateDevice(
		warpAdapter,
		D3D_FEATURE_LEVEL_12_0,
		IID_PPV_ARGS(D12Device.get_init())
		));
}

#include <EASTL\vector.h>

unique_com_ptr<ID3D12Debug>						D12DebugLayer;
eastl::vector<eastl::shared_ptr<D3D12Device>>	DevicesList;

void InitDevices(u32 adapterIndex, bool debugMode) {
	if (debugMode) {
		VERIFYDX12(D3D12GetDebugInterface(IID_PPV_ARGS(D12DebugLayer.get_init())));
		D12DebugLayer->EnableDebugLayer();
	}

	DevicesList.emplace_back(eastl::make_shared<D3D12Device>());
	DevicesList.back()->InitFromAdapterIndex(adapterIndex, D3D_FEATURE_LEVEL_11_0);

	/*DevicesList.emplace_back(new D3D12Device);
	DevicesList.back()->InitWarp();*/
}

D3D12Device* GetPrimaryDevice() {
	return DevicesList[0].get();
}

FGPUResource*	GetBackbuffer() {
	return GetSwapChain()->Backbuffers[GetSwapChain()->CurrentBackbufferIndex].get();
}

DXGI_FORMAT GetBackbufferFormat() {
	return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
}

DXGI_QUERY_VIDEO_MEMORY_INFO GetLocalMemoryInfo() {
	DXGI_QUERY_VIDEO_MEMORY_INFO memInfo;
	VERIFYDX12(GetPrimaryDevice()->Adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo));
	return memInfo;
}

DXGI_QUERY_VIDEO_MEMORY_INFO GetNonLocalMemoryInfo() {
	DXGI_QUERY_VIDEO_MEMORY_INFO memInfo;
	VERIFYDX12(GetPrimaryDevice()->Adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &memInfo));
	return memInfo;
}

WinSwapChain::WinSwapChain() :
	VSync(1), BackbuffersNum(3), MaxLatency(3), WaitToVBlank(0), BackbufferFormat(DXGI_FORMAT_R8G8B8A8_UNORM), CurrentBackbufferIndex(0), VBlankWaitable{}
{
	DXGI_SWAP_CHAIN_DESC descSwapChain;
	ZeroMemory(&descSwapChain, sizeof(descSwapChain));
	descSwapChain.BufferCount = BackbuffersNum;
	descSwapChain.BufferDesc.Format = BackbufferFormat;
	descSwapChain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	descSwapChain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	descSwapChain.OutputWindow = Win32::GetWndHandle();
	descSwapChain.SampleDesc.Count = 1;
	descSwapChain.Windowed = TRUE;
	descSwapChain.Flags =
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
		// using waitable flag and ingoring it causes trouble
		| (WaitToVBlank ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT : 0);

	IDXGISwapChain* swapChain;
	VERIFYDX12(GetDXGIFactory()->CreateSwapChain(
		GetDirectQueue()->D12CommandQueue.get(),
		&descSwapChain,
		&swapChain));

	VERIFYDX12(swapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)SwapChain.get_init()));
	swapChain->Release();

	if (WaitToVBlank) {
		VBlankWaitable = SwapChain->GetFrameLatencyWaitableObject();
		VERIFYDX12(SwapChain->SetMaximumFrameLatency(MaxLatency));
	}

	Backbuffers.reserve(BackbuffersNum);

	for (u32 i = 0; i < BackbuffersNum; ++i) {
		ID3D12Resource* resource;
		VERIFYDX12(SwapChain->GetBuffer(i, IID_PPV_ARGS(&resource)));
		Backbuffers.emplace_back(eastl::make_unique<FGPUResource>(resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB));
		AllocateResourceViews(Backbuffers.back().get());
	}
}

WinSwapChain::~WinSwapChain() {
	if (VBlankWaitable) {
		CloseHandle(VBlankWaitable);
	}
}

void WinSwapChain::Resize(u32 width, u32 height) {
	for (u32 i = 0; i < BackbuffersNum; ++i) {
		FreeResourceViews(Backbuffers[i].get(), GetDummyGPUSyncPoint());
	}
	Backbuffers.clear();

	VERIFYDX12(SwapChain->ResizeBuffers(BackbuffersNum, width, height, BackbufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | (WaitToVBlank ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT : 0)));

	for (u32 i = 0; i < BackbuffersNum; ++i) {
		ID3D12Resource* resource;
		VERIFYDX12(SwapChain->GetBuffer(i, IID_PPV_ARGS(&resource)));
		Backbuffers.emplace_back(eastl::make_unique<FGPUResource>(resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB));
		AllocateResourceViews(Backbuffers.back().get());
	}

	CurrentBackbufferIndex = 0;
}

void WinSwapChain::Present() {
	VERIFYDX12(SwapChain->Present(VSync, 0));

	if (WaitToVBlank) {
		auto result = WaitForSingleObject(VBlankWaitable, INFINITE);
		check(result == WAIT_OBJECT_0);
	}

	CurrentBackbufferIndex = (CurrentBackbufferIndex + 1) % BackbuffersNum;
}

eastl::unique_ptr<WinSwapChain> GWinSwapChain;

WinSwapChain* GetSwapChain() {
	if (!GWinSwapChain.get()) {
		GWinSwapChain = eastl::make_unique<WinSwapChain>();
	}
	return GWinSwapChain.get();
}
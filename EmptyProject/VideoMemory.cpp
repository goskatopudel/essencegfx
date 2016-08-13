#include "VideoMemory.h"
#include "d3dx12.h"
#include "Descriptors.h"
#include <EASTL/queue.h>
#include "PointerMath.h"

eastl::unique_ptr<FDescriptorAllocator>		OnlineSOVsAllocator;
eastl::unique_ptr<FDescriptorAllocator>		SOVsAllocator;
eastl::unique_ptr<FDescriptorAllocator>		DSVsAllocator;
eastl::unique_ptr<FDescriptorAllocator>		RTVsAllocator;

eastl::unique_ptr<FTextureAllocator>		TexturesAllocator;
eastl::unique_ptr<FLinearAllocator>			ConstantsAllocator;
eastl::unique_ptr<FUploadBufferAllocator>	UploadAllocator;

void TickDescriptors(FGPUSyncPoint FrameEndSync) {
	SOVsAllocator->FenceTemporaryAllocations(FrameEndSync);
	SOVsAllocator->Tick();
	DSVsAllocator->Tick();
	RTVsAllocator->Tick();
}

FResourceAllocator::FResourceAllocator(u32 MaxResources) :
	MAX_RESOURCES(MaxResources),
	FREELIST_GUARD(0xFFFFFFFF),
	NextFreeSlot(0)
{
	ResourcesPool.resize(MaxResources);
	NextFreeSlotList.reserve(MaxResources);
	for (u32 index = 0; index < MaxResources; ++index) {
		NextFreeSlotList.push_back(index + 1);
	}
	NextFreeSlotList[MaxResources - 1] = FREELIST_GUARD;
}

u32		FResourceAllocator::AllocateSlot() {
	check(NextFreeSlot != FREELIST_GUARD && NextFreeSlot < MAX_RESOURCES);
	u32 Slot = NextFreeSlot;
	NextFreeSlot = NextFreeSlotList[NextFreeSlot];
	NextFreeSlotList[Slot] = FREELIST_GUARD;
	return Slot;
}
void	FResourceAllocator::FreeSlot(u32 Slot) {
	check(NextFreeSlotList[Slot] == FREELIST_GUARD);
	// in-place recreate (it's owned by a vector)
	ResourcesPool[Slot].~FGPUResource();
	new (&ResourcesPool[Slot]) FGPUResource();
	NextFreeSlotList[Slot] = NextFreeSlot;
	NextFreeSlot = Slot;
}

FGPUResourceRef	FResourceAllocator::Allocate() {
	FGPUResource* Slim = ResourcesPool.data() + AllocateSlot();

	Slim->FatData = eastl::make_unique<FGPUResourceFat>();
	*Slim->FatData.get() = {};
	Slim->FatData->Allocator = this;

	return FGPUResourceRef(Slim);
}

u32	 FResourceAllocator::GetSlotIndex(FGPUResource* Resource) const {
	check(0 <= Resource - ResourcesPool.data());
	check(Resource - ResourcesPool.data() < MAX_RESOURCES);
	return (u32)(Resource - ResourcesPool.data());
}

void FResourceAllocator::Free(FGPUResource* Resource) {
	FreeSlot(GetSlotIndex(Resource));
}

void	FResourceAllocator::Tick() {
	while (!DeferredDeletionSyncBlocks.empty() && DeferredDeletionSyncBlocks.front().first.IsCompleted()) {
		u32 num = DeferredDeletionSyncBlocks.front().second;
		for (u32 index = 0; index < num; ++index) {
			Free(DeferredDeletionQueue.front());
			DeferredDeletionQueue.pop();
		}
		DeferredDeletionSyncBlocks.pop();
	}
}

void	FResourceAllocator::Free(FGPUResource* Resource, FGPUSyncPoint sync) {
	if (sync.IsCompleted()) {
		Free(Resource);
	}
	else if (DeferredDeletionSyncBlocks.size() && DeferredDeletionSyncBlocks.back().first == sync) {
		DeferredDeletionSyncBlocks.back().second++;
		DeferredDeletionQueue.push(Resource);
	}
	else {
		DeferredDeletionSyncBlocks.push(SyncPair(sync, 1));
		DeferredDeletionQueue.push(Resource);
	}
}

FResourceAllocator::~FResourceAllocator() {
	Tick();
	check(DeferredDeletionSyncBlocks.size() == 0);
}

u32 GIgnoreRelease = 0;

void eastl::default_delete<FGPUResource>::operator()(FGPUResource* GPUResource) const EA_NOEXCEPT {
	if (!GIgnoreRelease && GPUResource->FatData->Allocator) {
		if (!GPUResource->FatData->DeletionFGPUSyncPoint.IsSet()) {
			GPUResource->FenceDeletion(GetCurrentFrameGPUSyncPoint());
		}

		GPUResource->FatData->Allocator->Free(GPUResource, GPUResource->FatData->DeletionFGPUSyncPoint);
	}
	else if(!GIgnoreRelease && !GPUResource->FatData->Allocator) {
		GPUResource->~FGPUResource();
	}
}

FLinearAllocator::~FLinearAllocator() {
	check(PendingQueue.size() == 0);
}

void	FLinearAllocator::AllocateNewBlock() {
	if (CurrentBlock) {
		PendingBlocks.push(std::move(CurrentBlock));
		++CurrentFrameBlocks;
	}

	CurrentBlockOffset = 0;

	if (ReadyBlocks.size()) {
		CurrentBlock = std::move(ReadyBlocks.back());
		ReadyBlocks.pop_back();
		return;
	}

	CurrentBlock = std::move(HelperAllocator->CreateBuffer(BlockSize, 0));
	CurrentBlock->SetDebugName(L"FLinearAllocator Block");
}

FFastUploadAllocation		FLinearAllocator::Allocate(u64 size, u64 alignment) {
	check(size <= BlockSize);
	u64 allocationOffset = ((CurrentBlockOffset + alignment - 1) & ~(alignment - 1));
	u64 nextOffset = allocationOffset + size;

	if (!CurrentBlock || nextOffset > BlockSize) {
		AllocateNewBlock();
		allocationOffset = ((CurrentBlockOffset + alignment - 1) & ~(alignment - 1));
		nextOffset = allocationOffset + size;
	}

	CurrentBlockOffset = nextOffset;

	auto handle = SOVsAllocator->FastTemporaryAllocate(1);
	FFastUploadAllocation result = {};
	result.CPUPtr = pointer_add(CurrentBlock->FatData->CpuPtr, allocationOffset);
	result.CPUHandle = handle.GetCPUHandle(0);

	D3D12_CONSTANT_BUFFER_VIEW_DESC CbvDesc = {};
	CbvDesc.BufferLocation = CurrentBlock->D12Resource->GetGPUVirtualAddress() + allocationOffset;
	CbvDesc.SizeInBytes = (u32)(size + 255) & ~255;
	GetPrimaryDevice()->D12Device->CreateConstantBufferView(&CbvDesc, result.CPUHandle);

	return result;
}

void	FLinearAllocator::FenceFrameAllocations(FGPUSyncPoint sync) {
	if (CurrentBlock) {
		PendingBlocks.push(std::move(CurrentBlock));
		++CurrentFrameBlocks;
	}

	PendingQueue.push(FencedBlocks(sync, CurrentFrameBlocks));
	CurrentFrameBlocks = 0;
}

void	FLinearAllocator::Tick() {
	FResourceAllocator::Tick();
	HelperAllocator->Tick();

	while (PendingQueue.size() && PendingQueue.front().first.IsCompleted()) {
		u32 Num = PendingQueue.front().second;
		for (u32 i = 0; i < Num; ++i) {
			ReadyBlocks.push_back(std::move(PendingBlocks.front()));
			PendingBlocks.pop();
		}
		PendingQueue.pop();
	}
}

FLinearAllocator *		GetConstantsAllocator() {
	if (!ConstantsAllocator.get()) {
		ConstantsAllocator = eastl::make_unique<FLinearAllocator>(64 * 1024);
	}

	return ConstantsAllocator.get();
}

FUploadBufferAllocator *	GetUploadAllocator() {
	if (!UploadAllocator.get()) {
		UploadAllocator = eastl::make_unique<FUploadBufferAllocator>(64 * 1024);
	}

	return UploadAllocator.get();
}

FGPUResourceRef FUploadBufferAllocator::CreateBuffer(u64 size, u64 alignment) {
	FGPUResourceRef resource = Allocate();
	resource->FatData->Type = ResourceType::BUFFER;
	resource->FatData->IsCpuWriteable = 1;

	D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	resource->FatData->HeapProperties = heapProperties;

	VERIFYDX12(GetPrimaryDevice()->D12Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_NONE, 0),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(resource->D12Resource.get_init())));

	VERIFYDX12(resource->D12Resource->Map(0, nullptr, &resource->FatData->CpuPtr));
	return resource;
}

DXGI_FORMAT GetDepthStencilFormat(DXGI_FORMAT format) {
	switch (format) {
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

DXGI_FORMAT GetDepthReadFormat(DXGI_FORMAT format) {
	switch (format) {
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

DXGI_FORMAT GetStencilReadFormat(DXGI_FORMAT format) {
	switch (format) {
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

bool HasStencil(DXGI_FORMAT format) {
	switch (format) {
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		return true;
	}
	return false;
}

FDescriptorAllocator* GetOnlineDescriptorsAllocator() {
	if (!OnlineSOVsAllocator.get()) {
		OnlineSOVsAllocator = eastl::make_unique<FDescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 999'936, true);
	}

	return OnlineSOVsAllocator.get();
}

D3D12_CPU_DESCRIPTOR_HANDLE	NULL_CBV_VIEW;
D3D12_CPU_DESCRIPTOR_HANDLE	NULL_TEXTURE2D_VIEW;
D3D12_CPU_DESCRIPTOR_HANDLE	NULL_TEXTURECUBE_VIEW;
D3D12_CPU_DESCRIPTOR_HANDLE	NULL_TEXTURE2D_UAV_VIEW;

void InitDescriptorHeaps() {
	if (!SOVsAllocator.get()) {
		SOVsAllocator = eastl::make_unique<FDescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 32768, false);
		DSVsAllocator = eastl::make_unique<FDescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 32768, false);
		RTVsAllocator = eastl::make_unique<FDescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 32768, false);

		auto NullViewsAllocation = SOVsAllocator->Allocate(4);

		NULL_CBV_VIEW = NullViewsAllocation.GetCPUHandle(0);
		NULL_TEXTURE2D_VIEW = NullViewsAllocation.GetCPUHandle(1);
		NULL_TEXTURECUBE_VIEW = NullViewsAllocation.GetCPUHandle(2);
		NULL_TEXTURE2D_UAV_VIEW = NullViewsAllocation.GetCPUHandle(3);

		D3D12_CONSTANT_BUFFER_VIEW_DESC CbvDesc = {};
		GetPrimaryDevice()->D12Device->CreateConstantBufferView(&CbvDesc, NULL_CBV_VIEW);

		D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
		SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		SrvDesc.Texture2D.MipLevels = 1;
		GetPrimaryDevice()->D12Device->CreateShaderResourceView(nullptr, &SrvDesc, NULL_TEXTURE2D_VIEW);

		SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		SrvDesc.TextureCube = {};
		SrvDesc.TextureCube.MipLevels = 1;
		GetPrimaryDevice()->D12Device->CreateShaderResourceView(nullptr, &SrvDesc, NULL_TEXTURECUBE_VIEW);

		D3D12_UNORDERED_ACCESS_VIEW_DESC UavDesc = {};
		UavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		GetPrimaryDevice()->D12Device->CreateUnorderedAccessView(nullptr, nullptr, &UavDesc, NULL_TEXTURE2D_UAV_VIEW);
	}
}

void InitNullDescriptors() {
	InitDescriptorHeaps();
}

FTextureAllocator* GetTexturesAllocator() {
	if (!TexturesAllocator.get()) {
		TexturesAllocator = eastl::make_unique<FTextureAllocator>(128 * 1024);
	}
	return TexturesAllocator.get();
}

void FreeResourceViews(FGPUResource* resource, FGPUSyncPoint sync) {
	resource->FatData->Views.MainSet.MainSRV.Free(sync);
	resource->FatData->Views.MainSet.SubresourcesSRVs.Free(sync);
	if (resource->FatData->IsDepthStencil) {
		resource->FatData->Views.MainSet.SubresourcesDSVs.Free(sync);
	}
	else {
		resource->FatData->Views.MainSet.SubresourcesRTVs.Free(sync);
		resource->FatData->Views.MainSet.SubresourcesUAVs.Free(sync);
	}

	for (auto& set : resource->FatData->Views.CustomSets) {
		set.second.MainSRV.Free(sync);
		set.second.SubresourcesSRVs.Free(sync);
		if (resource->FatData->IsDepthStencil) {
			set.second.SubresourcesDSVs.Free(sync);
		}
		else {
			set.second.SubresourcesRTVs.Free(sync);
			set.second.SubresourcesUAVs.Free(sync);
		}
	}

	resource->FatData->Views.CustomSets.clear();
}

void AllocateResourceViews(FGPUResource* resource, DXGI_FORMAT format, FResourceViewsSet& outViews) {
	InitDescriptorHeaps();

	if (resource->FatData->ViewDimension == D3D12_SRV_DIMENSION_TEXTURE3D) {
		check(0);
	}
	else if (resource->FatData->ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBEARRAY) {
		check(0);
	}
	else if (resource->FatData->ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBE) {
		check(0);
	}
	else if (resource->FatData->ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DARRAY) {
		check(0);
	}
	else if(resource->FatData->ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D) {
		if (resource->FatData->IsShaderReadable) {
			check(!outViews.MainSRV.IsValid());
			outViews.MainSRV = SOVsAllocator->Allocate(resource->FatData->PlanesNum);

			D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			SRVDesc.Texture2D.MipLevels = resource->FatData->Desc.MipLevels;
			SRVDesc.Texture2D.MostDetailedMip = 0;
			SRVDesc.Texture2D.ResourceMinLODClamp = 0;
			SRVDesc.Texture2D.PlaneSlice = 0;
			SRVDesc.Format = format;
			SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			if (resource->FatData->IsDepthStencil) {
				SRVDesc.Format = GetDepthReadFormat(resource->FatData->Desc.Format);
			}

			GetPrimaryDevice()->D12Device->CreateShaderResourceView(resource->D12Resource.get(), &SRVDesc, outViews.MainSRV.GetCPUHandle(0));

			if (resource->FatData->IsDepthStencil && resource->FatData->PlanesNum > 1) {
				SRVDesc.Format = GetStencilReadFormat(format);
				SRVDesc.Texture2D.PlaneSlice = 1;
				GetPrimaryDevice()->D12Device->CreateShaderResourceView(resource->D12Resource.get(), &SRVDesc, outViews.MainSRV.GetCPUHandle(1));
			}

			if (resource->IsWritable() && resource->FatData->Desc.MipLevels > 1) {
				check(!outViews.SubresourcesSRVs.IsValid());
				u32 subresourcesWithViewsNum = resource->GetSubresourcesNum();
			
				resource->FatData->Views.MainSet.SubresourcesSRVs = SOVsAllocator->Allocate(subresourcesWithViewsNum);

				for (u32 s = 0; s < subresourcesWithViewsNum; ++s) {
					auto subresInfo = resource->GetSubresourceInfo(s);
					SRVDesc.Texture2D.MipLevels = 1;
					SRVDesc.Texture2D.MostDetailedMip = subresInfo.Mip;
					SRVDesc.Texture2D.PlaneSlice = subresInfo.Plane;
					SRVDesc.Format = GetDepthReadFormat(format);
					GetPrimaryDevice()->D12Device->CreateShaderResourceView(resource->D12Resource.get(), &SRVDesc, outViews.SubresourcesSRVs.GetCPUHandle(s));
				}
			}

		}

		if (resource->FatData->IsRenderTarget) {
			check(!outViews.SubresourcesRTVs.IsValid());
			u32 subresourcesWithViewsNum = resource->GetSubresourcesNum();
			outViews.SubresourcesRTVs = RTVsAllocator->Allocate(subresourcesWithViewsNum);
		
			D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
			RTVDesc.Format = format;
			RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			RTVDesc.Texture2D.MipSlice = 0;
			RTVDesc.Texture2D.PlaneSlice = 0;
		
			for (u32 s = 0; s < subresourcesWithViewsNum; ++s) {
				auto subresInfo = resource->GetSubresourceInfo(s);
				RTVDesc.Texture2D.MipSlice = s;
				RTVDesc.Texture2D.PlaneSlice = subresInfo.Plane;
				GetPrimaryDevice()->D12Device->CreateRenderTargetView(resource->D12Resource.get(), &RTVDesc, outViews.SubresourcesRTVs.GetCPUHandle(s));
			}
		}

		if (resource->FatData->IsDepthStencil) {
			check(!outViews.SubresourcesDSVs.IsValid());
			const u32 planesNum = resource->FatData->PlanesNum;
			const u32 mipmapsNum = resource->GetMipmapsNum();
			u32 viewsNum = mipmapsNum * planesNum * 2; // for read-only accesses
			outViews.SubresourcesDSVs = DSVsAllocator->Allocate(viewsNum);

			D3D12_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
			DSVDesc.Format = GetDepthStencilFormat(format);
			DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			
			for (u32 v = 0; v < mipmapsNum; ++v) {
				auto subresInfo = resource->GetSubresourceInfo(v);
				DSVDesc.Texture2D.MipSlice = subresInfo.Mip;
				DSVDesc.Flags = D3D12_DSV_FLAG_NONE;
				GetPrimaryDevice()->D12Device->CreateDepthStencilView(resource->D12Resource.get(), &DSVDesc, outViews.SubresourcesDSVs.GetCPUHandle(v));
				DSVDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
				GetPrimaryDevice()->D12Device->CreateDepthStencilView(resource->D12Resource.get(), &DSVDesc, outViews.SubresourcesDSVs.GetCPUHandle(v + mipmapsNum * planesNum));
				if(planesNum == 2) {
					DSVDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_STENCIL;
					GetPrimaryDevice()->D12Device->CreateDepthStencilView(resource->D12Resource.get(), &DSVDesc, outViews.SubresourcesDSVs.GetCPUHandle(v + 2 * mipmapsNum * planesNum));
					DSVDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH | D3D12_DSV_FLAG_READ_ONLY_STENCIL;
					GetPrimaryDevice()->D12Device->CreateDepthStencilView(resource->D12Resource.get(), &DSVDesc, outViews.SubresourcesDSVs.GetCPUHandle(v + 3 * mipmapsNum * planesNum));
				}
			}
		}

		if (resource->FatData->IsUnorderedAccess) {
			check(!outViews.SubresourcesUAVs.IsValid());
			u32 subresourcesWithViewsNum = resource->GetSubresourcesNum();
			outViews.SubresourcesUAVs = SOVsAllocator->Allocate(subresourcesWithViewsNum);

			D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
			UAVDesc.Format = format;
			UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			UAVDesc.Texture2D.MipSlice = 0;
			UAVDesc.Texture2D.PlaneSlice = 0;

			for (u32 s = 0; s < subresourcesWithViewsNum; ++s) {
				auto subresInfo = resource->GetSubresourceInfo(s);
				UAVDesc.Texture2D.MipSlice = s;
				UAVDesc.Texture2D.PlaneSlice = subresInfo.Plane;
				GetPrimaryDevice()->D12Device->CreateUnorderedAccessView(resource->D12Resource.get(), nullptr, &UAVDesc, outViews.SubresourcesUAVs.GetCPUHandle(s));
			}
		}
	}
	else if (resource->FatData->ViewDimension == D3D12_SRV_DIMENSION_BUFFER) {
		if (resource->FatData->IsShaderReadable) {
			check(!outViews.MainSRV.IsValid());
			outViews.MainSRV = SOVsAllocator->Allocate(1);

			D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			SRVDesc.Buffer.FirstElement = 0;
			SRVDesc.Buffer.NumElements = (u32)resource->FatData->Desc.Width / resource->FatData->BufferStride;
			SRVDesc.Buffer.StructureByteStride = resource->FatData->BufferStride;
			SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
			SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			GetPrimaryDevice()->D12Device->CreateShaderResourceView(resource->D12Resource.get(), &SRVDesc, outViews.MainSRV.GetCPUHandle(0));
		}
	}
}

void AllocateResourceViews(FGPUResource* resource) {
	AllocateResourceViews(resource, resource->FatData->ViewFormat, resource->FatData->Views.MainSet);
}

void ConstructTexture(FGPUResource * Resource, u64 width, u32 height, u32 depthOrArraySize, DXGI_FORMAT format, TextureFlags flags, wchar_t const* debugName, DXGI_FORMAT clearFormat, float4 clearColor, float clearDepth, u8 clearStencil) {
	check(!((flags & (ALLOW_RENDER_TARGET | ALLOW_UNORDERED_ACCESS)) && (flags & ALLOW_DEPTH_STENCIL)));
	check(!((flags & TEXTURE_3D) && (flags & TEXTURE_CUBEMAP)));
	check(!((flags & TEXTURE_CUBEMAP) && (depthOrArraySize % 6) != 0));

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = (flags & TEXTURE_3D) ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = width;
	desc.Height = height;
	desc.DepthOrArraySize = depthOrArraySize;
	desc.MipLevels = (flags & TEXTURE_MIPMAPPED) ? 0 : 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = (flags & TEXTURE_TILED) ? D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE : D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags =
		(flags & ALLOW_RENDER_TARGET) ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE
		| (flags & ALLOW_DEPTH_STENCIL) ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_NONE
		| (flags & ALLOW_UNORDERED_ACCESS) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE
		;

	Resource->FatData->PlanesNum = 1;
	if ((flags & ALLOW_DEPTH_STENCIL) && HasStencil(format)) {
		Resource->FatData->PlanesNum = 2;
	}

	if ((flags & ALLOW_RENDER_TARGET) && clearFormat == DXGI_FORMAT_UNKNOWN) {
		clearFormat = format;
	}

	if ((flags & ALLOW_DEPTH_STENCIL) && clearFormat == DXGI_FORMAT_UNKNOWN) {
		clearFormat = GetDepthStencilFormat(format);
	}

	D3D12_CLEAR_VALUE clearValue;
	if (clearFormat != DXGI_FORMAT_UNKNOWN) {
		clearValue.Format = clearFormat;
		bool needsClearValue = false;
		if (flags & ALLOW_DEPTH_STENCIL) {
			Resource->FatData->IsDepthStencil = 1;
			needsClearValue = true;
			clearValue.Format = clearFormat;
			clearValue.DepthStencil.Depth = clearDepth;
			clearValue.DepthStencil.Stencil = clearStencil;
			Resource->FatData->ViewFormat = GetDepthReadFormat(clearFormat);
		}
		else if (flags & ALLOW_RENDER_TARGET) {
			Resource->FatData->IsRenderTarget = 1;
			needsClearValue = true;
			clearValue.Color[0] = clearColor.x;
			clearValue.Color[1] = clearColor.y;
			clearValue.Color[2] = clearColor.z;
			clearValue.Color[3] = clearColor.w;
			Resource->FatData->ViewFormat = clearFormat;
		}
	}
	else {
		Resource->FatData->ViewFormat = format;
	}

	Resource->FatData->IsShaderReadable = 1;

	if (flags & TEXTURE_3D) {
		Resource->FatData->ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
	}
	else if ((flags & TEXTURE_CUBEMAP) && (flags & TEXTURE_ARRAY)) {
		Resource->FatData->ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
	}
	else if (flags & TEXTURE_CUBEMAP) {
		Resource->FatData->ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	}
	else if (flags & TEXTURE_ARRAY) {
		Resource->FatData->ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	}
	else {
		Resource->FatData->ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	}

	Resource->FatData->IsUnorderedAccess = (flags & ALLOW_UNORDERED_ACCESS) > 0;

	if (flags & TEXTURE_TILED) {
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

		Resource->FatData->IsReserved = 1;
		Resource->FatData->HeapProperties = {};

		VERIFYDX12(GetPrimaryDevice()->D12Device->CreateReservedResource(
			&desc,
			initialState,
			clearFormat != DXGI_FORMAT_UNKNOWN ? &clearValue : nullptr,
			IID_PPV_ARGS(Resource->D12Resource.get_init())
		));
	}
	else {
		D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
		if (Resource->FatData->IsRenderTarget) {
			initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
			Resource->FatData->AutomaticBarriers = 1;
			GetResourceStateRegistry()->SetCurrentState(Resource, ALL_SUBRESOURCES, EAccessType::WRITE_RT);
		}
		else if (Resource->FatData->IsDepthStencil) {
			initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
			Resource->FatData->AutomaticBarriers = 1;
			GetResourceStateRegistry()->SetCurrentState(Resource, ALL_SUBRESOURCES, EAccessType::WRITE_DEPTH);
		}
		else if (Resource->FatData->IsUnorderedAccess) {
			initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			Resource->FatData->AutomaticBarriers = 1;
			GetResourceStateRegistry()->SetCurrentState(Resource, ALL_SUBRESOURCES, EAccessType::WRITE_UAV);
		}
		else {
			initialState = D3D12_RESOURCE_STATE_COPY_DEST;
		}

		Resource->FatData->IsCommited = 1;
		Resource->FatData->HeapProperties = heapProperties;

		VERIFYDX12(GetPrimaryDevice()->D12Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			initialState,
			clearFormat != DXGI_FORMAT_UNKNOWN ? &clearValue : nullptr,
			IID_PPV_ARGS(Resource->D12Resource.get_init())
		));
	}

	Resource->FatData->Desc = Resource->D12Resource->GetDesc();
	Resource->FatData->Name = eastl::wstring(debugName);

	if (debugName) {
		SetDebugName(Resource->D12Resource.get(), debugName);
	}

	AllocateResourceViews(Resource, Resource->FatData->ViewFormat, Resource->FatData->Views.MainSet);
	if (Resource->IsReadOnly()) {
		Resource->ReadOnlySRV = Resource->FatData->Views.MainSet.MainSRV.GetCPUHandle(0);
	}
}

FGPUResourceRef FTextureAllocator::CreateTexture(u64 width, u32 height, u32 depthOrArraySize, DXGI_FORMAT format, TextureFlags flags, wchar_t const* debugName, DXGI_FORMAT clearFormat, float4 clearColor, float clearDepth, u8 clearStencil) {
	FGPUResourceRef result = Allocate();

	ConstructTexture(result.get(), width, height, depthOrArraySize, format, flags, debugName, clearFormat, clearColor, clearDepth, clearStencil);

	return result;
}

FGPUResourceRef FBuffersAllocator::CreateSimpleBuffer(u64 size, u64 alignment, wchar_t const * debugName) {
	FGPUResourceRef result = Allocate();

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = size;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	result->FatData->ViewDimension = D3D12_SRV_DIMENSION_UNKNOWN;
	D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

	result->FatData->IsCommited = 1;
	result->FatData->HeapProperties = heapProperties;

	VERIFYDX12(GetPrimaryDevice()->D12Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		initialState,
		nullptr,
		IID_PPV_ARGS(result->D12Resource.get_init())
		));

	result->FatData->Desc = result->D12Resource->GetDesc();
	result->FatData->Name = eastl::wstring(debugName);

	if (debugName) {
		SetDebugName(result->D12Resource.get(), debugName);
	}

	return result;
}

bool Any(EBufferFlags E) {
	return (u32)E != 0;
}

FGPUResourceRef	FBuffersAllocator::CreateBuffer(u64 size, u64 alignment, u32 stride, EBufferFlags flags, wchar_t const * debugName) {
	FGPUResourceRef result = Allocate();

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = size * stride;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	result->FatData->BufferStride = stride;
	result->FatData->IsShaderReadable = Any(flags & EBufferFlags::ShaderReadable) ? 1 : 0;
	result->FatData->ViewDimension = Any(flags & EBufferFlags::ShaderReadable) ? D3D12_SRV_DIMENSION_BUFFER : D3D12_SRV_DIMENSION_UNKNOWN;
	D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

	result->FatData->IsCommited = 1;
	result->FatData->HeapProperties = heapProperties;

	VERIFYDX12(GetPrimaryDevice()->D12Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		initialState,
		nullptr,
		IID_PPV_ARGS(result->D12Resource.get_init())
	));

	result->FatData->Desc = result->D12Resource->GetDesc();
	result->FatData->Name = eastl::wstring(debugName);

	if (debugName) {
		SetDebugName(result->D12Resource.get(), debugName);
	}

	AllocateResourceViews(result, result->FatData->ViewFormat, result->FatData->Views.MainSet);
	if (result->IsReadOnly()) {
		result->ReadOnlySRV = result->FatData->Views.MainSet.MainSRV.GetCPUHandle(0);
	}

	return result;
}

eastl::unique_ptr<FBuffersAllocator>	BuffersAllocator;

FBuffersAllocator *			GetBuffersAllocator() {
	if (!BuffersAllocator.get()) {
		BuffersAllocator = eastl::make_unique<FBuffersAllocator>(64 * 1024);
	}
	return BuffersAllocator.get();
}

eastl::unique_ptr<FPooledRenderTargetAllocator>	PooledRenderTargetAllocator;

struct FHeap {
	unique_com_ptr<ID3D12Heap> D3D12Heap;
	u64 Size;
};

typedef eastl::tuple<u64, u64> FRangeTuple;

struct FSharedHeapInfo {
	eastl::vector<FRangeTuple> RangesInUse;
};

// AllocateFromPool
	// find free <resource, range>
		// if found return
	// find free range (create heap if needed)
		// placement create new <resource, range>, return

// FreeToPool
	// mark range as free: return <resource, range> to free pools


FPooledRenderTargetAllocator * GetPooledRenderTargetAllocator() {
	if (!PooledRenderTargetAllocator.get()) {
		PooledRenderTargetAllocator = eastl::make_unique<FPooledRenderTargetAllocator>(64 * 1024);
	}
	return PooledRenderTargetAllocator.get();
}
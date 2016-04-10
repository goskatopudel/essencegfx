#include "Resource.h"

FGPUResource::FGPUResource(ID3D12Resource* resource, DXGI_FORMAT viewFormat) : D12Resource(resource), FatData(new FGPUResourceFat()) {
	FatData->Desc = resource->GetDesc();

	FatData->IsRenderTarget = (FatData->Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) > 0;
	FatData->ViewFormat = viewFormat;
	FatData->ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	FatData->PlanesNum = 1;
}

FGPUResource::~FGPUResource() {
	if (FatData.get() && FatData->CpuPtr) {
		D12Resource->Unmap(0, nullptr);
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE FGPUResource::GetRTV() const {
	return FatData->Views.MainSet.SubresourcesRTVs.GetCPUHandle(0);
}

D3D12_CPU_DESCRIPTOR_HANDLE FGPUResource::GetRTV(DXGI_FORMAT format) {
	if (format == FatData->ViewFormat) {
		return GetRTV();
	}

	auto lookup = FatData->Views.CustomSets.insert((u32)format);
	if (lookup.second == true) {
		AllocateResourceViews(this, format, lookup.first->second);
	}

	return lookup.first->second.SubresourcesRTVs.GetCPUHandle(0);
}

D3D12_CPU_DESCRIPTOR_HANDLE FGPUResource::GetDSV() const {
	return FatData->Views.MainSet.SubresourcesDSVs.GetCPUHandle(0);
}

D3D12_CPU_DESCRIPTOR_HANDLE FGPUResource::GetSRV() const {
	if (ReadOnlySRV.ptr) {
		return ReadOnlySRV;
	}
	return FatData->Views.MainSet.MainSRV.GetCPUHandle(0);
}

D3D12_VIEWPORT FGPUResource::GetSizeAsViewport() const {
	return { 0.f, 0.f, (float)FatData->Desc.Width, (float)FatData->Desc.Height, 0.f, 1.f } ;
}

void*	FGPUResource::GetMappedPtr() const {
	return FatData->CpuPtr;
}

GPU_VIRTUAL_ADDRESS	FGPUResource::GetGPUAddress() const {
	return D12Resource->GetGPUVirtualAddress();
}

bool	FGPUResource::IsTexture3D() const {
	return FatData->ViewDimension == D3D12_SRV_DIMENSION_TEXTURE3D;
}

u32		FGPUResource::GetSubresourceIndex(u32 mip, u32 arraySlice, u32 planeSlice) const {
	return D3D12CalcSubresource(mip, arraySlice, planeSlice, FatData->Desc.MipLevels, (IsTexture3D() ? 1 : FatData->Desc.DepthOrArraySize));
}

EAccessType	FGPUResource::GetDefaultAccess() const {
	if (FatData->IsRenderTarget) {
		return EAccessType::WRITE_RT;
	}
	if (FatData->IsDepthStencil) {
		return EAccessType::WRITE_DEPTH;
	}
	if (FatData->IsUnorderedAccess) {
		return EAccessType::WRITE_UAV;
	}
	return EAccessType::INVALID;
}

bool	FGPUResource::IsFixedState() const {
	return FatData->HeapProperties.Type == D3D12_HEAP_TYPE_UPLOAD || FatData->HeapProperties.Type == D3D12_HEAP_TYPE_READBACK;
}

FSubresourceInfo FGPUResource::GetSubresourceInfo(u32 subresourceIndex) const {
	FSubresourceInfo Info = {};
	u32 Mips = FatData->Desc.MipLevels;
	u32 ArraySize = (IsTexture3D() ? 1 : FatData->Desc.DepthOrArraySize);
	u32 Planes = FatData->PlanesNum;
	
	Info.Mip = subresourceIndex % (ArraySize * Planes);
	Info.ArrayIndex = (subresourceIndex / Mips) % ArraySize;
	Info.Plane = subresourceIndex / (Mips * ArraySize);
	
	return Info;
}

u32		FGPUResource::GetSubresourcesNum() const {
	return FatData->PlanesNum * FatData->Desc.MipLevels * (IsTexture3D() ? 1 : FatData->Desc.DepthOrArraySize);
}

bool	FGPUResource::IsReadOnly() const {
	return !FatData->IsRenderTarget && !FatData->IsDepthStencil && !FatData->IsUnorderedAccess;
}

bool	FGPUResource::IsWritable() const {
	return !IsReadOnly();
}

bool	FGPUResource::HasStencil() const {
	return FatData->IsDepthStencil && FatData->PlanesNum > 1;
}
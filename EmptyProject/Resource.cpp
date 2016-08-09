#include "Resource.h"

bool IsExclusiveAccess(EAccessType Type) {
	switch (Type) {
	case EAccessType::WRITE_RT:
	case EAccessType::WRITE_DEPTH:
	case EAccessType::WRITE_UAV:
	case EAccessType::COPY_DEST:
	case EAccessType::COMMON:
		return true;
	}

	return false;
}

bool IsReadAccess(EAccessType Type) {
	return (Type & (EAccessType::READ_PIXEL | EAccessType::READ_NON_PIXEL | EAccessType::READ_DEPTH | EAccessType::COPY_SRC | EAccessType::READ_IB | EAccessType::READ_VB_CB)) != EAccessType::UNSPECIFIED;
}

FGPUResource::FGPUResource(ID3D12Resource* resource, DXGI_FORMAT viewFormat) : D12Resource(resource), FatData(new FGPUResourceFat()) {
	FatData->Desc = resource->GetDesc();

	FatData->IsRenderTarget = (FatData->Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) > 0;
	FatData->ViewFormat = viewFormat;
	FatData->ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	FatData->PlanesNum = 1;

	FatData->AutomaticBarriers = 1;
	GetResourceStateRegistry()->SetCurrentState(this, ALL_SUBRESOURCES, EAccessType::COMMON);
}

FGPUResource::~FGPUResource() {
	GetResourceStateRegistry()->Deregister(this);
	if (FatData.get() && FatData->CpuPtr) {
		D12Resource->Unmap(0, nullptr);
	}
}

void FGPUResource::SetDebugName(const wchar_t* Name) {
	::SetDebugName(D12Resource.get(), Name);
	FatData->Name = Name;
}

void FGPUResource::FenceDeletion(SyncPoint Sync) {
	// support only one for now
	check(!FatData->DeletionSyncPoint.IsSet());
	FatData->DeletionSyncPoint = Sync;
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

D3D12_CPU_DESCRIPTOR_HANDLE FGPUResource::GetRTV(u32 MipLevel) const {
	return FatData->Views.MainSet.SubresourcesRTVs.GetCPUHandle(GetSubresourceIndex(MipLevel));
}
D3D12_CPU_DESCRIPTOR_HANDLE FGPUResource::GetSRV(u32 MipLevel) const {
	return FatData->Views.MainSet.SubresourcesSRVs.GetCPUHandle(GetSubresourceIndex(MipLevel));
}

D3D12_CPU_DESCRIPTOR_HANDLE FGPUResource::GetDSV(u32 MipLevel) const {
	return FatData->Views.MainSet.SubresourcesDSVs.GetCPUHandle(GetSubresourceIndex(MipLevel));
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

D3D12_CPU_DESCRIPTOR_HANDLE FGPUResource::GetUAV() const {
	return FatData->Views.MainSet.SubresourcesUAVs.GetCPUHandle(0);
}

Vec3u FGPUResource::GetDimensions() const {
	return Vec3u((u32)FatData->Desc.Width, FatData->Desc.Height, FatData->Desc.DepthOrArraySize);
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
	return EAccessType::UNSPECIFIED;
}

bool	FGPUResource::IsFixedState() const {
	return FatData->HeapProperties.Type == D3D12_HEAP_TYPE_UPLOAD || FatData->HeapProperties.Type == D3D12_HEAP_TYPE_READBACK;
}

DXGI_FORMAT	FGPUResource::GetFormat() const {
	return FatData->Desc.Format;
}

DXGI_FORMAT FGPUResource::GetWriteFormat(bool bSRGB) const {
	if (FatData->IsDepthStencil) {
		return GetDepthStencilFormat(GetFormat());
	}

	if (bSRGB && !IsSRGB(GetFormat())) {
		return MakeSRGB(GetFormat());
	}

	return GetFormat();
}

FSubresourceInfo FGPUResource::GetSubresourceInfo(u32 subresourceIndex) const {
	FSubresourceInfo Info = {};
	u32 Mips = FatData->Desc.MipLevels;
	u32 ArraySize = (IsTexture3D() ? 1 : FatData->Desc.DepthOrArraySize);
	u32 Planes = FatData->PlanesNum;
	
	Info.Mip = subresourceIndex % Mips;
	Info.ArrayIndex = (subresourceIndex / Mips) % ArraySize;
	Info.Plane = subresourceIndex / (Mips * ArraySize);
	
	return Info;
}

u32		FGPUResource::GetSubresourcesNum() const {
	return FatData->PlanesNum * FatData->Desc.MipLevels * (IsTexture3D() ? 1 : FatData->Desc.DepthOrArraySize);
}

u32 FGPUResource::GetMipmapsNum() const {
	return FatData->Desc.MipLevels;
}

bool	FGPUResource::IsReadOnly() const {
	return !FatData->IsRenderTarget && !FatData->IsDepthStencil && !FatData->IsUnorderedAccess;
}

bool	FGPUResource::IsWritable() const {
	return !IsReadOnly();
}

bool FGPUResource::IsDepthStencil() const {
	return FatData->IsDepthStencil;
}
bool FGPUResource::IsRenderTarget() const {
	return FatData->IsRenderTarget;
}

bool	FGPUResource::HasStencil() const {
	return FatData->IsDepthStencil && FatData->PlanesNum > 1;
}

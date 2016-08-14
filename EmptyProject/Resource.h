#pragma once
#include "Essence.h"

#include "Descriptors.h"
#include <d3d12.h>

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/hash_map.h>

bool IsExclusiveAccess(EAccessType Type);
bool IsReadAccess(EAccessType Type);

enum TextureFlags {
	TEXTURE_NO_FLAGS = 0,
	ALLOW_RENDER_TARGET = 1,
	ALLOW_DEPTH_STENCIL = 2,
	ALLOW_UNORDERED_ACCESS = 4,
	TEXTURE_MIPMAPPED = 8,
	TEXTURE_CUBEMAP = 0x10,
	TEXTURE_TILED = 0x20,
	TEXTURE_3D = 0x40,
	TEXTURE_ARRAY = 0x80,
};
DEFINE_ENUM_FLAG_OPERATORS(TextureFlags);

struct FResourceViewsSet {
	FDescriptorsAllocation					MainSRV;
	FDescriptorsAllocation					SubresourcesSRVs;
	union {
		FDescriptorsAllocation				SubresourcesDSVs;
		struct {
			FDescriptorsAllocation			SubresourcesRTVs;
			FDescriptorsAllocation			SubresourcesUAVs;
		};
	};
};

class FResourceViews {
public:
	FResourceViewsSet						MainSet;
	eastl::hash_map<u32, FResourceViewsSet>	CustomSets;
};

struct FSubresourceInfo {
	u32		Mip;
	u32		Plane;
	u32		ArrayIndex;
};

struct FResourceView {
	D3D12_CPU_DESCRIPTOR_HANDLE DescHandle;
	DXGI_FORMAT Format;
	FGPUResource * Resource;
	u32 Subresource;
	EAccessType RequiredAccess;
};

class FResourceAllocator;
class FGPUResourceFat;

enum class ResourceType : u8 {
	TEXTURE,
	BUFFER
};

class FGPUResource {
public:
	unique_com_ptr<ID3D12Resource> D12Resource;
	D3D12_CPU_DESCRIPTOR_HANDLE ReadOnlySRV; // if resource is read-only texture this is set to current srv, if not this is 0 and slow lookup is needed
	eastl::unique_ptr<FGPUResourceFat> FatData;

	FGPUResource() = default;
	FGPUResource(const FGPUResource&) = default;
	FGPUResource(FGPUResource&&) = default;
	FGPUResource& operator = (FGPUResource const&) = default;
	FGPUResource& operator = (FGPUResource&&) = default;
	FGPUResource(ID3D12Resource*, DXGI_FORMAT);
	~FGPUResource();

	D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(DXGI_FORMAT);
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(u32 MipLevel) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSV(u32 MipLevel) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRV(u32 MipLevel) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetUAV() const;

	Vec3u GetDimensions() const;
	D3D12_VIEWPORT GetSizeAsViewport() const;
	void* GetMappedPtr() const;
	GPU_VIRTUAL_ADDRESS GetGPUAddress() const;

	u32 GetSubresourceIndex(u32 mip, u32 arraySlice = 0, u32 planeSlice = 0) const;
	FSubresourceInfo GetSubresourceInfo(u32 subresourceIndex) const;
	u32 GetSubresourcesNum() const;
	u32 GetMipmapsNum() const;
	bool IsReadOnly() const;
	bool IsWritable() const;
	bool IsDepthStencil() const;
	bool IsRenderTarget() const;
	bool IsUnorderedAccess() const;
	bool HasStencil() const;
	bool IsTexture3D() const;

	EAccessType GetDefaultAccess() const;
	bool IsFixedState() const;
	DXGI_FORMAT GetRawFormat() const;
	DXGI_FORMAT GetReadFormat(bool bSRGB = false) const;
	DXGI_FORMAT GetWriteFormat(bool bSRGB = false) const;

	void FenceDeletion(FGPUSyncPoint Sync);
	void SetDebugName(const wchar_t*);
};

template <>
struct eastl::default_delete<FGPUResource>
{
	void operator()(FGPUResource* GPUResource) const EA_NOEXCEPT;
};

struct FGPUResourceRef : public eastl::shared_ptr<FGPUResource> {
	using shared_ptr<FGPUResource>::shared_ptr;

	operator FGPUResource *() const { return get(); };
};

class FGPUResourceFat {
public:
	eastl::wstring			Name;
	ResourceType			Type;
	FResourceViews			Views;
	D3D12_RESOURCE_DESC		Desc;
	u32						BufferStride;
	D3D12_SRV_DIMENSION		ViewDimension;
	D3D12_HEAP_PROPERTIES	HeapProperties;
	FResourceAllocator*		Allocator = nullptr;
	u32						PlanesNum : 2;
	u32						IsCommited : 1;
	u32						IsPlaced : 1;
	u32						IsReserved : 1;
	u32						IsCpuReadable : 1;
	u32						IsCpuWriteable : 1;
	u32						IsRenderTarget : 1;
	u32						IsDepthStencil : 1;
	u32						IsUnorderedAccess : 1;
	u32						IsShaderReadable : 1;
	u32						AutomaticBarriers : 1;
	DXGI_FORMAT				ViewFormat;
	u64						DataSizeBytes;
	u64						UnaliasedHeapMemoryBytes;
	FGPUResourceRef			CounterResource;
	void*					CpuPtr = nullptr;
	FGPUSyncPoint			DeletionGPUSyncPoint;
};


void			AllocateResourceViews(FGPUResource* resource);
void			AllocateResourceViews(FGPUResource* resource, DXGI_FORMAT format, FResourceViewsSet& outViews);
void			FreeResourceViews(FGPUResource* resource, FGPUSyncPoint sync);

extern			D3D12_CPU_DESCRIPTOR_HANDLE	NULL_CBV_VIEW;
extern			D3D12_CPU_DESCRIPTOR_HANDLE	NULL_TEXTURE2D_VIEW;
extern			D3D12_CPU_DESCRIPTOR_HANDLE	NULL_TEXTURECUBE_VIEW;
extern			D3D12_CPU_DESCRIPTOR_HANDLE	NULL_TEXTURE2D_UAV_VIEW;

void			InitNullDescriptors();

u64				BitsPerPixel(DXGI_FORMAT fmt);
bool			IsSRGB(DXGI_FORMAT format);
DXGI_FORMAT		MakeSRGB(DXGI_FORMAT format);
DXGI_FORMAT		GetDepthStencilFormat(DXGI_FORMAT format);
DXGI_FORMAT		GetDepthReadFormat(DXGI_FORMAT format);
DXGI_FORMAT		GetStencilReadFormat(DXGI_FORMAT format);
bool			HasStencil(DXGI_FORMAT format);


FGPUResourceRef	LoadDDS(const wchar_t * Filename, bool ForceSrgb, FGPUContext & CopyContext);


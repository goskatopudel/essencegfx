#pragma once
#include "Essence.h"
#include "Device.h"
#include "Commands.h"
#include "Resource.h"
#include <EASTL/vector.h>
#include <EASTL/hash_map.h>

struct memory_stats_t {
	u64		heaps_memory;
	u32		shader_visible_descriptors;
};

class FResourceAllocator {
public:
	typedef eastl::pair<FGPUSyncPoint, u32> SyncPair;
	eastl::queue<FGPUResource*> DeferredDeletionQueue;
	eastl::queue<SyncPair> DeferredDeletionSyncBlocks;

	const u32 MAX_RESOURCES;
	const u32 FREELIST_GUARD;

	eastl::vector<FGPUResource> ResourcesPool;
	eastl::vector<u32> NextFreeSlotList;
	u32 NextFreeSlot;

	FResourceAllocator(u32 MaxResources);

	u32 AllocateSlot();
	void FreeSlot(u32 Slot);

	u32 GetSlotIndex(FGPUResource*) const;

	FGPUResourceRef Allocate();
	// used on pointer to destruct data (and construct back immediately)
	// vector of resources owns data and on allocator destruction needs to call destructor on all elements, they can't have stale data (hence this destructor makes sure to recreate it)
	void Free(FGPUResource*);
	void Free(FGPUResource*, FGPUSyncPoint);

	virtual void Tick();
	virtual ~FResourceAllocator();
};

class FUploadBufferAllocator : public FResourceAllocator {
public:
	FUploadBufferAllocator(u32 MaxResources) : FResourceAllocator(MaxResources) {}
	FGPUResourceRef CreateBuffer(u64 size, u64 alignment);
};

struct FFastUploadAllocation {
	void * CPUPtr;
	D3D12_CPU_DESCRIPTOR_HANDLE	CPUHandle;
};

class FLinearAllocator : public FResourceAllocator {
	eastl::unique_ptr<FUploadBufferAllocator> HelperAllocator;

	const u32 BlockSize;
	FGPUResourceRef CurrentBlock;
	u64 CurrentBlockOffset = 0;

	typedef eastl::pair<FGPUSyncPoint, u32> FencedBlocks;
	eastl::queue<FencedBlocks> PendingQueue;
	u32 CurrentFrameBlocks = 0;
	eastl::queue<FGPUResourceRef> PendingBlocks;
	eastl::vector<FGPUResourceRef> ReadyBlocks;
public:
	FLinearAllocator(u32 MaxResources, u32 blockSize = 1048576) : BlockSize(blockSize), FResourceAllocator(MaxResources) {
		HelperAllocator = eastl::make_unique<FUploadBufferAllocator>(1024);
	}

	~FLinearAllocator();

	void AllocateNewBlock();
	FFastUploadAllocation Allocate(u64 size, u64 alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	void FenceFrameAllocations(FGPUSyncPoint sync);
	void Tick() override;
};

class FTextureAllocator : public FResourceAllocator {
public:
	FTextureAllocator(u32 MaxResources) : FResourceAllocator(MaxResources) {}
	FGPUResourceRef	CreateTexture(u64 width, u32 height, u32 depthOrArraySize, DXGI_FORMAT format, TextureFlags flags, wchar_t const * debugName, DXGI_FORMAT clearFormat = DXGI_FORMAT_UNKNOWN, float4 clearColor = float4(0, 0, 0, 0), float clearDepth = 1.f, u8 clearStencil = 0);
};

enum class EBufferFlags {
	NoFlags = 0,
	ShaderReadable = 1,
};
DEFINE_ENUM_FLAG_OPERATORS(EBufferFlags);

class FBuffersAllocator : public FResourceAllocator {
public:
	FBuffersAllocator(u32 MaxResources) : FResourceAllocator(MaxResources) {}
	FGPUResourceRef	CreateSimpleBuffer(u64 size, u64 alignment, wchar_t const * debugName);
	FGPUResourceRef	CreateBuffer(u64 size, u64 alignment, u32 stride, EBufferFlags flags, wchar_t const * debugName);
};

class FPooledRenderTargetAllocator : public FResourceAllocator {
public:
	FPooledRenderTargetAllocator(u32 MaxResources) : FResourceAllocator(MaxResources) {}
	FGPUResourceRef CreateTexture(u64 width, u32 height, u32 depthOrArraySize, DXGI_FORMAT format, TextureFlags flags, wchar_t const * debugName, DXGI_FORMAT clearFormat = DXGI_FORMAT_UNKNOWN, float4 clearColor = float4(0, 0, 0, 0), float clearDepth = 1.f, u8 clearStencil = 0);
	FGPUResourceRef CreateShadow(FGPUResource * Resource);
};

FLinearAllocator * GetConstantsAllocator();
FTextureAllocator * GetTexturesAllocator();
FDescriptorAllocator * GetOnlineDescriptorsAllocator();
FUploadBufferAllocator * GetUploadAllocator();
FBuffersAllocator * GetBuffersAllocator();
FPooledRenderTargetAllocator * GetPooledRenderTargetAllocator();
void TickDescriptors(FGPUSyncPoint FrameEndSync);
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

struct FResourceAllocationInfo {
	u32		RefNum;
};

class FResourceAllocator {
public:
	typedef eastl::pair<SyncPoint, u32> SyncPair;
	eastl::queue<FGPUResource*>							DeferredDeletionQueue;
	eastl::queue<SyncPair>								DeferredDeletionSyncBlocks;

	const u32 MAX_RESOURCES;
	const u32 FREELIST_GUARD;

	eastl::vector<FGPUResource>							ResourcesPool;
	eastl::vector<u32>									NextFreeSlotList;
	eastl::vector<FResourceAllocationInfo>				AllocationInfo;
	u32													NextFreeSlot;

	FResourceAllocator(u32 MaxResources);

	u32		AllocateSlot();
	void	FreeSlot(u32 Slot);

	u32				GetSlotIndex(FGPUResource*) const;

	FOwnedResource	Allocate();
	// used on pointer to destruct data (and construct back immediately)
	// vector of resources owns data and on allocator destruction needs to call destructor on all elements, they can't have stale data (hence this destructor makes sure to recreate it)
	void			Destruct(FGPUResource*);
	void			AddRef(FGPUResource*);
	void			Release(FGPUResource*, SyncPoint);
	u32				GetRefCount(FGPUResource*) const;

	virtual void	Tick();
	virtual			~FResourceAllocator();
};

class FUploadBufferAllocator : public FResourceAllocator {
public:
	FUploadBufferAllocator(u32 MaxResources) : FResourceAllocator(MaxResources) {}
	FOwnedResource	CreateBuffer(u64 size, u64 alignment);
};

struct FFastUploadAllocation {
	void *						CPUPtr;
	D3D12_CPU_DESCRIPTOR_HANDLE	CPUHandle;
};

class FLinearAllocator : public FResourceAllocator {
	eastl::unique_ptr<FUploadBufferAllocator>		HelperAllocator;

	const u32							BlockSize;
	FOwnedResource						CurrentBlock;
	u64									CurrentBlockOffset = 0;

	typedef eastl::pair<SyncPoint, u32> FencedBlocks;
	eastl::queue<FencedBlocks>			PendingQueue;
	u32									CurrentFrameBlocks = 0;
	eastl::queue<FOwnedResource>		PendingBlocks;
	eastl::vector<FOwnedResource>		ReadyBlocks;
public:
	FLinearAllocator(u32 MaxResources, u32 blockSize = 1048576) : BlockSize(blockSize), FResourceAllocator(MaxResources) {
		HelperAllocator.reset(new FUploadBufferAllocator(1024));
	}

	~FLinearAllocator();

	void	AllocateNewBlock();
	FFastUploadAllocation		Allocate(u64 size, u64 alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	void	FenceFrameAllocations(SyncPoint sync);
	void	Tick() override;
};

class FTextureAllocator : public FResourceAllocator {
public:
	FTextureAllocator(u32 MaxResources) : FResourceAllocator(MaxResources) {}
	FOwnedResource	CreateTexture(u64 width, u32 height, u32 depthOrArraySize, DXGI_FORMAT format, TextureFlags flags, wchar_t const * debugName, DXGI_FORMAT clearFormat = DXGI_FORMAT_UNKNOWN, float4 clearColor = float4(0, 0, 0, 0), float clearDepth = 1.f, u8 clearStencil = 0);
};

class FBuffersAllocator : public FResourceAllocator {
public:
	FBuffersAllocator(u32 MaxResources) : FResourceAllocator(MaxResources) {}
	FOwnedResource	CreateBuffer(u64 size, u64 alignment, wchar_t const * debugName);
};

FLinearAllocator *			GetConstantsAllocator();
FTextureAllocator *			GetTexturesAllocator();
FDescriptorAllocator *		GetOnlineDescriptorsAllocator();
FUploadBufferAllocator *	GetUploadAllocator();
FBuffersAllocator *			GetBuffersAllocator();
void						TickDescriptors(SyncPoint FrameEndSync);
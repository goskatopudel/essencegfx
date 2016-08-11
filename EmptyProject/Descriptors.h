#pragma once
#include "Essence.h"
#include "Device.h"
#include "Commands.h"
#include <EASTL/vector.h>
#include <EASTL/queue.h>
#include <EASTL/array.h>

inline u32 FastLog2(u32 v) {
	u32 r; // result of log2(v) will go here
	u32 shift;

	r = (v > 0xFFFF) << 4; v >>= r;
	shift = (v > 0xFF) << 3; v >>= shift; r |= shift;
	shift = (v > 0xF) << 2; v >>= shift; r |= shift;
	shift = (v > 0x3) << 1; v >>= shift; r |= shift;
	r |= (v >> 1);

	return r;
}

class FDescriptorAllocator {
public:
	const u32								MaxDescriptors;
	const u32								IncrementSize;
	const D3D12_DESCRIPTOR_HEAP_TYPE		Type;
	const bool								IsShaderVisible;
	unique_com_ptr<ID3D12DescriptorHeap>	D12DescriptorHeap;

	// blocks are used for bucketed long-term allocations and fast allocations
	static const u32						BlockSize = 512;
	u32										NextFreeBlock = 0;
	eastl::vector<u32>						FreeBlocks;

	u32										AllocateBlock();
	void									FreeBlock(u32 index);

	class BucketBlock {
	public:
		u32 BlockIndex = -1;
		u32 NextFreeRange = 0;
		eastl::vector<u32> FreeRanges;
	};

	eastl::vector<BucketBlock>				Blocks; // pool
	
	static const u32						BucketsNum = 9;
	eastl::array<eastl::vector<u32>, BucketsNum> Buckets; // index to pool

	FDescriptorsAllocation AllocateFromBucketBlock(BucketBlock &Block, u32 num);
	void FreeToBucketBlock(BucketBlock &Block, u32 num);

	FDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 maxDescriptors, bool shaderVisible);

	FDescriptorsAllocation Allocate(u32 num);
	void FreeInternal(FDescriptorsAllocation allocation);

	typedef eastl::pair<FGPUSyncPoint, FDescriptorsAllocation> QueuedElement;
	eastl::queue<QueuedElement>	DeferredDeletionQueue;

	void					Free(FDescriptorsAllocation allocation, FGPUSyncPoint sync);
	void					Free(FDescriptorsAllocation allocation);
	void					Tick();

	struct FastAllocationBlock {
		u32 BlockIndex;
		u32 NextOffset;
	};

	eastl::queue<FastAllocationBlock>	FastAllocationBlocks;
	typedef eastl::pair<FGPUSyncPoint, u32> FencedFastAllocations;
	eastl::queue<FencedFastAllocations>	DeferredFastAllocationDeletionQueue;
	u32 FrameFastAllocationBlocksNum = 0;

	FDescriptorsAllocation	FastTemporaryAllocate(u32 num);
	void					FenceTemporaryAllocations(FGPUSyncPoint sync);
};

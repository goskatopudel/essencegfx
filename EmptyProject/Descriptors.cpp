#include "Descriptors.h"
#include "d3dx12.h"

FDescriptorAllocator::FDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 maxDescriptors, bool shaderVisible) :
	Type(type), MaxDescriptors(maxDescriptors), IsShaderVisible(shaderVisible),
	IncrementSize(GetPrimaryDevice()->D12Device->GetDescriptorHandleIncrementSize(type))
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = maxDescriptors;
	desc.Type = type;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;

	VERIFYDX12(GetPrimaryDevice()->D12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(D12DescriptorHeap.get_init())));

	check((maxDescriptors % BlockSize) == 0);
	const u32 BlocksPoolSize = maxDescriptors / BlockSize;
	Blocks.resize(BlocksPoolSize);
	for (u32 index = 0; index < BlocksPoolSize; ++index) {
		Blocks[index].BlockIndex = index;
	}
}

u32 FDescriptorAllocator::AllocateBlock() {
	if (FreeBlocks.size()) {
		u32 val = FreeBlocks.back();
		FreeBlocks.pop_back();
		return val;
	}

	check(NextFreeBlock * BlockSize < MaxDescriptors);
	return NextFreeBlock++;
}

void FDescriptorAllocator::FreeBlock(u32 index) {
	FreeBlocks.push_back(index);
}

const u32 FREELIST_GUARD = 0xFFFFFFFF;

FDescriptorsAllocation FDescriptorAllocator::AllocateFromBucketBlock(BucketBlock &Block, u32 num) {
	FDescriptorsAllocation result = {};
	result.Allocator = this;
	result.DescriptorsNum = num;

	u32 rangeIndex = Block.NextFreeRange;
	result.HeapOffset = Block.BlockIndex * BlockSize + rangeIndex * num;
	Block.NextFreeRange = Block.FreeRanges[rangeIndex];
	Block.FreeRanges[rangeIndex] = FREELIST_GUARD;

	return result;
}

void FDescriptorAllocator::FreeToBucketBlock(BucketBlock &Block, u32 freedIndex) {
	Block.FreeRanges[Block.NextFreeRange] = freedIndex;
	Block.NextFreeRange = freedIndex;
}

FDescriptorsAllocation FDescriptorAllocator::Allocate(u32 num) {
	u32 bucketIndex = FastLog2(num);
	check(bucketIndex < BucketsNum);

	for (auto blockIndex : Buckets[bucketIndex]) {
		auto& Block = Blocks[blockIndex];
		if (Block.NextFreeRange != FREELIST_GUARD) {
			return AllocateFromBucketBlock(Block, num);
		}
	}

	Buckets[bucketIndex].push_back(AllocateBlock());
	auto& Block = Blocks[Buckets[bucketIndex].back()];

	u32 rangesNum = BlockSize / (1 << bucketIndex);
	for (u32 index = 0; index < rangesNum; ++index) {
		Block.FreeRanges.push_back(index + 1);
	}
	Block.FreeRanges.back() = FREELIST_GUARD;

	return AllocateFromBucketBlock(Block, num);
}

void FDescriptorAllocator::FreeInternal(FDescriptorsAllocation allocation) {
	u32 bucketIndex = FastLog2(allocation.DescriptorsNum);
	check(bucketIndex < BucketsNum);

	u32 blockIndex = allocation.HeapOffset / BlockSize;
	FreeToBucketBlock(Blocks[blockIndex], (allocation.HeapOffset - blockIndex * BlockSize) / (1 << bucketIndex));
}

void FDescriptorAllocator::Free(FDescriptorsAllocation allocation, SyncPoint sync) {
	if (!sync.IsCompleted()) {
		DeferredDeletionQueue.push(QueuedElement(sync, allocation));
	}
	else {
		FreeInternal(allocation);
	}
}

void FDescriptorAllocator::Free(FDescriptorsAllocation allocation) {
	FreeInternal(allocation);
}

void FDescriptorAllocator::Tick() {
	while (DeferredDeletionQueue.size() && DeferredDeletionQueue.front().first.IsCompleted()) {
		FreeInternal(DeferredDeletionQueue.front().second);
		DeferredDeletionQueue.pop();
	}

	while (DeferredFastAllocationDeletionQueue.size() && DeferredFastAllocationDeletionQueue.front().first.IsCompleted()) {
		u32 num = DeferredFastAllocationDeletionQueue.front().second;

		while (num--) {
			check(FastAllocationBlocks.size());
			FreeBlock(FastAllocationBlocks.front().BlockIndex);
			FastAllocationBlocks.pop();
		}

		DeferredFastAllocationDeletionQueue.pop();
	}
}

FDescriptorsAllocation FDescriptorAllocator::FastTemporaryAllocate(u32 num) {
	FDescriptorsAllocation result = {};
	result.Allocator = this;
	result.DescriptorsNum = num;
	result.HeapOffset;

	if (!FastAllocationBlocks.size() || FastAllocationBlocks.back().NextOffset + num > BlockSize) {
		FastAllocationBlock fab = {};
		fab.BlockIndex = AllocateBlock();
		fab.NextOffset = 0;
		FastAllocationBlocks.push(fab);
		FrameFastAllocationBlocksNum++;
	}
	check(FastAllocationBlocks.back().NextOffset + num <= BlockSize);
	result.HeapOffset = FastAllocationBlocks.back().BlockIndex * BlockSize + FastAllocationBlocks.back().NextOffset;
	FastAllocationBlocks.back().NextOffset += num;

	return result;
}

void FDescriptorAllocator::FenceTemporaryAllocations(SyncPoint sync) {
	DeferredFastAllocationDeletionQueue.push(FencedFastAllocations(sync, FrameFastAllocationBlocksNum));
	FrameFastAllocationBlocksNum = 0;
	if (FastAllocationBlocks.size()) {
		FastAllocationBlocks.back().NextOffset = BlockSize;
	}
}

void FDescriptorsAllocation::Free(SyncPoint sync) {
	if (Allocator) {
		Allocator->Free(*this, sync);
		Allocator = nullptr;
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE FDescriptorsAllocation::GetCPUHandle(i32 offset) const {
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(Allocator->D12DescriptorHeap->GetCPUDescriptorHandleForHeapStart(), HeapOffset + offset, Allocator->IncrementSize);
}

D3D12_GPU_DESCRIPTOR_HANDLE FDescriptorsAllocation::GetGPUHandle(i32 offset) const {
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(Allocator->D12DescriptorHeap->GetGPUDescriptorHandleForHeapStart(), HeapOffset + offset, Allocator->IncrementSize);
}
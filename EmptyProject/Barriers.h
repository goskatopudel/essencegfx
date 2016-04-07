#pragma once
#include "Essence.h"
#include "Resource.h"

struct FResourcePart {
	FGPUResource*	Resource;
	u32				Subresource;

	FResourcePart() = default;
	FResourcePart(FGPUResource* resource) : Resource(resource), Subresource(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {}
	FResourcePart(FGPUResource* resource, u32 subresource) : Resource(resource), Subresource(subresource) {}
};

struct FAccessTransaction {
	u32				Subresource;
	EAccessType		From;
	EAccessType		To;
};

typedef eastl::hash_map<FGPUResource*, eastl::vector<FAccessTransaction>> FPassTransactionsMap;

struct FStoredBarrier {
	FGPUResource *	Resource;
	u32				Subresource;
	EAccessType		From;
	EAccessType		To;

	FStoredBarrier() = default;
	FStoredBarrier(FGPUResource * resource, EAccessType from, EAccessType to) : Resource(resource), From(from), To(to), Subresource(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {}
	FStoredBarrier(FGPUResource * resource, u32 subresource, EAccessType from, EAccessType to) : FStoredBarrier(resource, from, to) { Subresource = subresource; }
};

class FRenderPass {
public:
	eastl::wstring					DebugName;
	typedef eastl::pair<FResourcePart, EAccessType> FAccessPair;
	eastl::vector<FAccessPair>		ResourcesAccess;
	FPassTransactionsMap			Transactions;
	eastl::vector<FStoredBarrier>	Barriers;

	void SetName(const wchar_t* name);

	void SetAccess(FGPUResource* Resource, EAccessType Access, u32 Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void EndWriteAccess(FGPUResource* Resource, u32 Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void EndReadAccess(FGPUResource* Resource, u32 Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
};

class FRenderPassSequence {
public:
	FRenderPassSequence() = default;
	FRenderPassSequence(std::initializer_list<FRenderPass*> Passes);

	const bool UseSplitBarriers = true;
	eastl::vector<FRenderPass*>		PassesList;
	eastl::vector<FStoredBarrier>	FinalBarriers;
};
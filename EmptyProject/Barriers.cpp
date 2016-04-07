#include "Barriers.h"


bool IsWriteAccess(EAccessType Type) {
	switch (Type) {
	case EAccessType::WRITE_RT:
	case EAccessType::WRITE_DEPTH:
	case EAccessType::WRITE_UAV:
		return true;
	}

	return false;
}

bool IsReadAccess(EAccessType Type) {
	return (Type & (EAccessType::READ_PIXEL | EAccessType::READ_NON_PIXEL | EAccessType::READ_DEPTH)) != EAccessType::INVALID;
}

void FRenderPass::SetName(const wchar_t* name) {
	DebugName = eastl::wstring(name);
}

void FRenderPass::SetAccess(FGPUResource* Resource, EAccessType Access, u32 Subresource) {
	ResourcesAccess.push_back(FAccessPair(FResourcePart(Resource, Subresource), Access));
}

void FRenderPass::EndWriteAccess(FGPUResource* Resource, u32 Subresource) {
	ResourcesAccess.push_back(FAccessPair(FResourcePart(Resource, Subresource), EAccessType::END_WRITE));
}

void FRenderPass::EndReadAccess(FGPUResource* Resource, u32 Subresource) {
	ResourcesAccess.push_back(FAccessPair(FResourcePart(Resource, Subresource), EAccessType::END_READ));
}

#include <EASTL\set.h>
#include "Print.h"

FRenderPassSequence::FRenderPassSequence(std::initializer_list<FRenderPass*> Passes) {

	FRenderPass	PostPass;

	for (auto Pass : Passes) {
		PassesList.push_back(Pass);
	}
	PassesList.push_back(&PostPass);
	
	struct FPrevTransaction {
		FRenderPass *	Pass;
		u32				TransactionIndex;

		EAccessType		From;
		EAccessType		To;

		FAccessTransaction& LookupTransaction(FGPUResource* Resource) {
			return Pass->Transactions[Resource][TransactionIndex];
		}
	};

	eastl::set<FGPUResource*>	AccessedResourcesSet;

	for (u32 PassIndex = 0; PassIndex < PassesList.size(); PassIndex++) {
		auto Pass = PassesList[PassIndex];

		for (auto ResourceAccess : Pass->ResourcesAccess) {

			// first pass always keeps track of intitial split barrier

			auto & InitialTransactions = PassesList[0]->Transactions;

			auto FindIter = InitialTransactions.find(ResourceAccess.first.Resource);
			if (FindIter == InitialTransactions.end()) {
				FindIter = InitialTransactions.insert(ResourceAccess.first.Resource).first;

				FAccessTransaction Initial = {};
				Initial.From = EAccessType::INVALID;
				Initial.To = ResourceAccess.first.Resource->GetDefaultAccess();
				Initial.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				FindIter->second.push_back(Initial);
			}

			FPrevTransaction	SubresourceTransaction = {};
			FPrevTransaction	ResourceTransaction = {};

			auto Resource = ResourceAccess.first.Resource;
			if (AccessedResourcesSet.find(Resource) == AccessedResourcesSet.end()) {
				AccessedResourcesSet.insert(Resource);
				PostPass.SetAccess(ResourceAccess.first.Resource, ResourceAccess.first.Resource->GetDefaultAccess(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			}

			for (i32 ReverseIndex = (i32)PassIndex - 1; ReverseIndex >= 0; ReverseIndex--) {
				auto PastPass = Passes.begin()[ReverseIndex];
				auto FindIter = PastPass->Transactions.find(Resource);
				if (FindIter != PastPass->Transactions.end()) {
					auto& Transitions = FindIter->second;
					for (i32 Index = (i32)Transitions.size() - 1; Index >= 0; Index--) {
						if (Transitions[Index].Subresource == ResourceAccess.first.Subresource) {
							if (ResourceAccess.first.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
								ResourceTransaction.Pass = PastPass;
								ResourceTransaction.TransactionIndex = Index;
								ResourceTransaction.From = Transitions[Index].From;
								ResourceTransaction.To = Transitions[Index].To;
								break;
							}
							else {
								SubresourceTransaction.Pass = PastPass;
								SubresourceTransaction.TransactionIndex = Index;
								ResourceTransaction.From = Transitions[Index].From;
								ResourceTransaction.To = Transitions[Index].To;
							}
						}
						else if (Transitions[Index].Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
							ResourceTransaction.Pass = PastPass;
							ResourceTransaction.TransactionIndex = Index;
							ResourceTransaction.From = Transitions[Index].From;
							ResourceTransaction.To = Transitions[Index].To;
							break;
						}
					}

					if (ResourceTransaction.Pass) {
						break;
					}
				}
			}

			if (IsReadAccess(ResourceAccess.second)) {
				// READ->READ
				// we OR all subsequent read access flags
				if (SubresourceTransaction.Pass && IsReadAccess(SubresourceTransaction.To)) {
					SubresourceTransaction.LookupTransaction(Resource).To |= ResourceAccess.second;
				}
				else if (IsReadAccess(ResourceTransaction.To)) {
					SubresourceTransaction.LookupTransaction(Resource).To |= ResourceAccess.second;
				}

				FAccessTransaction NewTransaction = {};
				NewTransaction.Subresource = ResourceAccess.first.Subresource;
				NewTransaction.To = ResourceAccess.second;
				// WRITE->READ
				if (SubresourceTransaction.Pass && IsWriteAccess(SubresourceTransaction.To)) {
					NewTransaction.From = SubresourceTransaction.To;
				}
				else if (IsWriteAccess(ResourceTransaction.To)) {
					NewTransaction.From = ResourceTransaction.To;
				}
				Pass->Transactions[Resource].push_back(NewTransaction);
			}
			else {
				FAccessTransaction NewTransaction = {};
				NewTransaction.Subresource = ResourceAccess.first.Subresource;
				NewTransaction.To = ResourceAccess.second;
				// READ->WRITE
				// WRITE->WRITE
				// SPLIT->WRITE
				if (SubresourceTransaction.Pass) {
					if (SubresourceTransaction.To != ResourceAccess.second) {
						NewTransaction.From = SubresourceTransaction.To;
					}
				}
				else {
					if (ResourceTransaction.To != ResourceAccess.second) {
						NewTransaction.From = ResourceTransaction.To;
					}
				}

				if (NewTransaction.From != EAccessType::INVALID) {
					Pass->Transactions[Resource].push_back(NewTransaction);
				}
			}
		}
	}

	FAccessTransaction const*						PrevTransaction = nullptr;
	eastl::hash_map<u32, FAccessTransaction const*> PrevSubresTransaction;



	for (auto Resource : AccessedResourcesSet) {
		PrevTransaction = nullptr;
		PrevSubresTransaction.empty();

		for (u32 PassIndex = 0; PassIndex < PassesList.size(); PassIndex++) {
			auto Pass = PassesList[PassIndex];

			auto TransactionsFindIter = Pass->Transactions.find(Resource);
			if (TransactionsFindIter == Pass->Transactions.end()) {
				continue;
			}
			auto & ResourceTransactions = TransactionsFindIter->second;

			u32 TransactionsNum = (u32)ResourceTransactions.size();
			for (u32 TransactionIndex = 0; TransactionIndex < TransactionsNum; ++TransactionIndex) {
				auto &Transaction = ResourceTransactions[TransactionIndex];

				if (PrevTransaction == nullptr) {
					check(Transaction.From == EAccessType::INVALID && Transaction.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
					PrevTransaction = &Transaction;
				}
				else {
					auto PrevFindIter = PrevSubresTransaction.find(Transaction.Subresource);

					// WHOLE TO WHOLE
					if (PrevSubresTransaction.size() == 0 && Transaction.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
						FStoredBarrier Barrier = {};
						Barrier.Resource = Resource;
						Barrier.From = Transaction.From;
						Barrier.To = Transaction.To;
						Barrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
						Pass->Barriers.push_back(Barrier);
						PrevTransaction = &Transaction;
					}
					// TO SUBRES
					else if (Transaction.Subresource != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
						FStoredBarrier Barrier = {};
						Barrier.Resource = Resource;
						Barrier.From = Transaction.From;
						Barrier.To = Transaction.To;
						Barrier.Subresource = Transaction.Subresource;
						Pass->Barriers.push_back(Barrier);

						PrevSubresTransaction[Transaction.Subresource] = &Transaction;
					}
					// SUBRES TO WHOLE
					else {
						check(Transaction.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && PrevSubresTransaction.size());

						eastl::set<u32>	SubresourcesSet;

						for (auto SubresTransaction : PrevSubresTransaction) {
							if (SubresTransaction.second->To != Transaction.To) {
								FStoredBarrier Barrier = {};
								Barrier.Resource = Resource;
								Barrier.From = SubresTransaction.second->To;
								Barrier.To = Transaction.To;
								Barrier.Subresource = SubresTransaction.first;
								Pass->Barriers.push_back(Barrier);

								SubresourcesSet.insert(SubresTransaction.first);
							}
						}
						if (PrevTransaction->To != Transaction.To) {
							u32 SubresourcesNum = Resource->GetSubresourcesNum();
							for (u32 SubresourceIndex = 0; SubresourceIndex < SubresourcesNum; SubresourceIndex++) {
								if (SubresourcesSet.find(SubresourceIndex) == SubresourcesSet.end()) {
									FStoredBarrier Barrier = {};
									Barrier.Resource = Resource;
									Barrier.From = PrevTransaction->To;
									Barrier.To = Transaction.To;
									Barrier.Subresource = SubresourceIndex;
									Pass->Barriers.push_back(Barrier);
								}
							}
						}

						PrevSubresTransaction.empty();
						PrevTransaction = &Transaction;
					}
				}
			}
		}
	}

	PassesList.back()->SetName(L"%finalize");
	for (auto Pass : PassesList) {
		PrintFormated(L"# Pass: %s\n", Pass->DebugName.c_str());
		for (auto Barrier : Pass->Barriers) {
			PrintFormated(L"Resource: %s / %u, %x -> %x\n", Barrier.Resource->FatData->Name.c_str(), Barrier.Subresource, Barrier.From, Barrier.To);
		}
	}

	FinalBarriers = std::move(PassesList.back()->Barriers);
	PassesList.pop_back();
}
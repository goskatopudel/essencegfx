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

#include <EASTL\set.h>

void FRenderPass::SetAccess(FGPUResource* Resource, EAccessType Access, u32 Subresource) {
	eastl::set<FGPUResource*> PartialResources;
	for (u32 Index = 0; Index < ResourcesAccess.size(); Index++) {
		if (ResourcesAccess[Index].first.Subresource != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
			PartialResources.insert(ResourcesAccess[Index].first.Resource);
		}
		check(!(ResourcesAccess[Index].first.Resource == Resource && ResourcesAccess[Index].first.Subresource == Subresource));
	}
	check(!(Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && PartialResources.find(Resource) != PartialResources.end()));

	ResourcesAccess.push_back(FAccessPair(FResourcePart(Resource, Subresource), Access));
}

void FRenderPass::EndAccess(FGPUResource* Resource, u32 Subresource) {
	ResourcesAccess.push_back(FAccessPair(FResourcePart(Resource, Subresource), EAccessType::SPLIT_NO_ACCESS));
}

#include "Print.h"

FRenderPassSequence::FRenderPassSequence(std::initializer_list<FRenderPass*> Passes) {

	FRenderPass StartPass;
	FRenderPass	PostPass;

	PassesList.push_back(&StartPass);
	for (auto Pass : Passes) {
		PassesList.push_back(Pass);
	}
	PassesList.push_back(&PostPass);
	
	struct FPrevTransaction {
		FRenderPass *	Pass;
		u32				TransactionIndex;

		EAccessType		From;
		EAccessType		To;

		FAccessTransaction& GetReference(FGPUResource* Resource) {
			return Pass->Transactions[Resource][TransactionIndex];
		}
	};

	// generating transactions
	for (u32 PassIndex = 0; PassIndex < PassesList.size(); PassIndex++) {
		auto Pass = PassesList[PassIndex];


		for (u32 OuterIndex = 0; OuterIndex < Pass->ResourcesAccess.size(); OuterIndex++) {
			auto & ResourceAccess = Pass->ResourcesAccess[OuterIndex];

			// initialize resource
			auto & InitialTransactions = PassesList[0]->Transactions;
			auto FindIter = InitialTransactions.find(ResourceAccess.first.Resource);
			auto Resource = ResourceAccess.first.Resource;
			if (FindIter == InitialTransactions.end()) {
				FindIter = InitialTransactions.insert(ResourceAccess.first.Resource).first;

				FAccessTransaction Initial = {};
				Initial.From = EAccessType::INVALID;
				Initial.To = ResourceAccess.first.Resource->GetDefaultAccess();
				Initial.IsInitial = true;
				Initial.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				FindIter->second.push_back(Initial);

				PostPass.SetAccess(ResourceAccess.first.Resource, ResourceAccess.first.Resource->GetDefaultAccess(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			}

			// find prev transactions
			// A) whole resource transaction ( 0) when whole->whole or 1) whole->subres)
			// B) subresource transaction (when subres->subres)
			// C) whole resource transaction + holes flag (when ?->whole and we have subresources in different states)
			FPrevTransaction	SubresourceTransaction = {};
			FPrevTransaction	ResourceTransaction = {};
			bool				OtherSubresourcesTransactions = false;

			for (i32 ReverseIndex = (i32)PassIndex; ReverseIndex >= 0; ReverseIndex--) {
				auto PastPass = PassesList[ReverseIndex];
				auto FindIter = PastPass->Transactions.find(Resource);
				if (FindIter != PastPass->Transactions.end()) {
					auto& Transitions = FindIter->second;
					i32 StartIndex = ReverseIndex == PassIndex ? (i32)OuterIndex - 1 : (i32)Transitions.size() - 1;
					for (i32 Index = StartIndex; Index >= 0; Index--) {
						if (Transitions[Index].Subresource == ResourceAccess.first.Subresource) {
							// A0
							if (ResourceAccess.first.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
								ResourceTransaction.Pass = PastPass;
								ResourceTransaction.TransactionIndex = Index;
								ResourceTransaction.From = Transitions[Index].From;
								ResourceTransaction.To = Transitions[Index].To;
								break;
							}
							// B
							else {
								SubresourceTransaction.Pass = PastPass;
								SubresourceTransaction.TransactionIndex = Index;
								SubresourceTransaction.From = Transitions[Index].From;
								SubresourceTransaction.To = Transitions[Index].To;
							}
						}
						// A1
						else if (Transitions[Index].Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
							ResourceTransaction.Pass = PastPass;
							ResourceTransaction.TransactionIndex = Index;
							ResourceTransaction.From = Transitions[Index].From;
							ResourceTransaction.To = Transitions[Index].To;
							break;
						}
						// C
						else if ((Transitions[Index].Subresource != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
							&& (ResourceAccess.first.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)) {
							OtherSubresourcesTransactions = true;
						}
					}

					// we end on first whole resource transition
					if (ResourceTransaction.Pass) {
						break;
					}
				}
			}

			// we can combine it in 2 cases:
			// A') whole->whole, whole->subres, subres->subres
			// B') holes -> whole

			// we can combine changes in access in 2 cases:
			// 0) READ->READ (OR transitions)
			// 1) READ->WRITE, WRITE->READ, WRITE->WRITE (check if different)

			// A'0
			FPrevTransaction LookBackTransaction = SubresourceTransaction.Pass ? SubresourceTransaction : ResourceTransaction;
			if (!OtherSubresourcesTransactions && IsReadAccess(ResourceAccess.second) && IsReadAccess(LookBackTransaction.To)) {
				LookBackTransaction.GetReference(Resource).To |= ResourceAccess.second;
			}
			// A'1
			else if (!OtherSubresourcesTransactions && (LookBackTransaction.To != ResourceAccess.second)) {
				FAccessTransaction NewTransaction = {};
				NewTransaction.Subresource = ResourceAccess.first.Subresource;
				NewTransaction.From = LookBackTransaction.To;
				NewTransaction.To = ResourceAccess.second;
				Pass->Transactions[Resource].push_back(NewTransaction);
			}
			// B'0/1
			else if (OtherSubresourcesTransactions) {
				FAccessTransaction NewTransaction = {};
				NewTransaction.Subresource = ResourceAccess.first.Subresource;
				NewTransaction.From = EAccessType::INVALID;
				NewTransaction.To = ResourceAccess.second;
				NewTransaction.SubresourcesToAll = true;
				Pass->Transactions[Resource].push_back(NewTransaction);
			}
		}
	}

	FAccessTransaction const*						RecentTransaction = nullptr;
	eastl::hash_map<u32, FAccessTransaction const*> RecentSubresTransactions;

	

	// second pass, transactions to barriers
	for(auto SetElement : PassesList[0]->Transactions) {
		auto Resource = SetElement.first;

		RecentTransaction = nullptr;
		RecentSubresTransactions.clear();

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

				if (RecentTransaction == nullptr) {
					check(Transaction.IsInitial && Transaction.From == EAccessType::INVALID && Transaction.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
					RecentTransaction = &Transaction;
				}
				else {
					auto PrevFindIter = RecentSubresTransactions.find(Transaction.Subresource);

					check(!Transaction.IsInitial);

					// WHOLE TO WHOLE, WHOLE TO SUBRES
					if (RecentSubresTransactions.size() == 0 || (Transaction.Subresource != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && PrevFindIter == RecentSubresTransactions.end())) {
						check(!Transaction.SubresourcesToAll);
						FStoredBarrier Barrier = {};
						Barrier.Resource = Resource;
						Barrier.From = Transaction.From;
						Barrier.To = Transaction.To;
						Barrier.Subresource = Transaction.Subresource;
						Pass->Barriers.push_back(Barrier);

						if (Transaction.Subresource != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
							RecentSubresTransactions[Transaction.Subresource] = &Transaction;
						}
						else {
							RecentTransaction = &Transaction;
						}
					}
					// SUBRES TO SUBRES
					else if (Transaction.Subresource != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && PrevFindIter != RecentSubresTransactions.end()) {
						check(Transaction.SubresourcesToAll == false);
						FStoredBarrier Barrier = {};
						Barrier.Resource = Resource;
						Barrier.From = Transaction.From;
						Barrier.To = Transaction.To;
						Barrier.Subresource = Transaction.Subresource;
						Pass->Barriers.push_back(Barrier);

						RecentSubresTransactions[Transaction.Subresource] = &Transaction;
					}
					// SUBRES TO WHOLE
					else {
						check(Transaction.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && RecentSubresTransactions.size() && Transaction.SubresourcesToAll);

						u32 SubresourcesNum = Resource->GetSubresourcesNum();
						for (u32 SubresourceIndex = 0; SubresourceIndex < SubresourcesNum; SubresourceIndex++) {
							auto SubresFindIter = RecentSubresTransactions.find(SubresourceIndex);
							if (SubresFindIter != RecentSubresTransactions.end()) {
								if (SubresFindIter->second->To != Transaction.To) {
									FStoredBarrier Barrier = {};
									Barrier.Resource = Resource;
									Barrier.From = SubresFindIter->second->To;
									Barrier.To = Transaction.To;
									Barrier.Subresource = SubresourceIndex;
									Pass->Barriers.push_back(Barrier);
								}
							}
							else {
								if (RecentTransaction->To != Transaction.To) {
									FStoredBarrier Barrier = {};
									Barrier.Resource = Resource;
									Barrier.From = RecentTransaction->To;
									Barrier.To = Transaction.To;
									Barrier.Subresource = SubresourceIndex;
									Pass->Barriers.push_back(Barrier);
								}
							}
						}

						RecentSubresTransactions.clear();
						RecentTransaction = &Transaction;
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
	PassesList[0] = nullptr;
	PassesList.pop_back();
}
#include "CommandStream.h"
#include "Pipeline.h"
#include "Commands.h"

u64	FRenderCmdBarriersBatchFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdBarriersBatch*)DataVoidPtr;
	Data->This->ExecuteBatchedBarriers(Context, Data->BatchIndex);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdBarriersBatch);
}

u64	FRenderCmdClearRTVFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdClearRTV*)DataVoidPtr;
	Context->ClearRTV(Data->RTV, Data->Color);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdClearRTV);
}

u64	FRenderCmdClearDSVFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdClearDSV*)DataVoidPtr;
	Context->ClearDSV(Data->DSV, Data->Depth, Data->Stencil);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdClearDSV);
}

u64	FRenderCmdSetPipelineStateFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdSetPipelineState*)DataVoidPtr;
	Context->SetRoot(Data->State->ShaderState->Root);
	Context->SetPSO(Data->State);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdSetPipelineState);
}

u64	FRenderCmdSetVBFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdSetVB*)DataVoidPtr;
	Context->SetVB(Data->Location, Data->Stream);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdSetVB);
}

u64 FRenderCmdSetIBFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdSetIB*)DataVoidPtr;
	Context->SetIB(Data->Location);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdSetIB);
};

u64 FRenderCmdSetTopologyFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdSetTopology*)DataVoidPtr;
	Context->SetTopology(Data->Topology);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdSetTopology);
};

u64 FRenderCmdSetViewportFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdSetViewport*)DataVoidPtr;
	Context->SetViewport(Data->Viewport);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdSetViewport);
};

u64 FRenderCmdSetRenderTargetFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdSetRenderTarget*)DataVoidPtr;
	Context->SetRenderTarget(Data->Index, Data->RTV);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdSetRenderTarget);
};

u64 FRenderCmdSetDepthStencilFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdSetDepthStencil*)DataVoidPtr;
	Context->SetDepthStencil(Data->DSV);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdSetDepthStencil);
}

u64 FRenderCmdSetTextureFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdSetTexture*)DataVoidPtr;
	Context->SetTexture(Data->Param, Data->SRV);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdSetTexture);
};

u64 FRenderCmdSetConstantBufferFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdSetConstantBuffer*)DataVoidPtr;
	Context->SetConstantBuffer(Data->Param, Data->CBV);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdSetConstantBuffer);
};

u64 FRenderCmdSetRWTextureFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdSetRWTexture*)DataVoidPtr;
	Context->SetRWTexture(Data->Param, Data->UAV);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdSetRWTexture);
}

u64 FRenderCmdSetScissorRectFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdSetScissorRect*)DataVoidPtr;
	Context->SetScissorRect(Data->Rect);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdSetScissorRect);
};

u64 FRenderCmdDrawFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdDraw*)DataVoidPtr;
	Context->Draw(Data->VertexCount, Data->StartVertex, Data->Instances, Data->StartInstance);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdDraw);
};

u64 FRenderCmdDrawIndexedFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdDrawIndexed*)DataVoidPtr;
	Context->DrawIndexed(Data->IndexCount, Data->StartIndex, Data->BaseVertex, Data->Instances, Data->StartInstance);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdDrawIndexed);
};

u64 FRenderCmdDispatchFunc(FGPUContext * Context, void * DataVoidPtr) {
	auto Data = (FRenderCmdDispatch*)DataVoidPtr;
	Context->Dispatch(Data->X, Data->Y, Data->Z);
	return sizeof(FRenderCmdHeader) + sizeof(FRenderCmdDispatch);
}
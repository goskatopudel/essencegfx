#include "SceneRendering.h"
#include "BasePass.h"
#include "Application.h"
#include "Commands.h"
#include "VideoMemory.h"
#include "Pipeline.h"
#include "Shader.h"
#include "MathFunctions.h"

FGPUResourceRef SceneDepth;
FGPUResourceRef GBuffer0;
FGPUResourceRef GBuffer1;
FGPUResourceRef GBuffer2;
FGPUResourceRef AlbedoTexture;

FGPUResourceRef AtomicCounter;
FGPUResourceRef MotionVectorsTileListBegin;
FGPUResourceRef VectorsList;
FGPUResourceRef NodesList;

template<typename T, typename V>
u32 IntDivRoundUp(T Divident, V Divisor) {
	return (Divident + Divident - 1) / Divisor;
}

void AllocateGBuffer() {
	SceneDepth = GetTexturesAllocator()->CreateTexture(GApplication::WindowWidth, GApplication::WindowHeight, 1, DXGI_FORMAT_R24G8_TYPELESS, TextureFlags::ALLOW_DEPTH_STENCIL, L"SceneDepth");
	GBuffer0 = GetTexturesAllocator()->CreateTexture(GApplication::WindowWidth, GApplication::WindowHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, TextureFlags::ALLOW_RENDER_TARGET, L"GBuffer0");
	GBuffer1 = GetTexturesAllocator()->CreateTexture(GApplication::WindowWidth, GApplication::WindowHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM, TextureFlags::ALLOW_RENDER_TARGET, L"GBuffer1");
	GBuffer2 = GetTexturesAllocator()->CreateTexture(GApplication::WindowWidth, GApplication::WindowHeight, 1, DXGI_FORMAT_R16G16_FLOAT, TextureFlags::ALLOW_RENDER_TARGET, L"GBuffer2");

	MotionVectorsTileListBegin = GetTexturesAllocator()->CreateTexture(IntDivRoundUp(GApplication::WindowWidth, 8), IntDivRoundUp(GApplication::WindowHeight, 8), 1, DXGI_FORMAT_R32_SINT, TextureFlags::ALLOW_RENDER_TARGET | TextureFlags::ALLOW_UNORDERED_ACCESS, L"MVTiles");
	u32 Stride;
	Stride = (u32)sizeof(float) * 4;
	u64 TilesNum = MotionVectorsTileListBegin->GetDimensions().x * MotionVectorsTileListBegin->GetDimensions().y;
	VectorsList = GetBuffersAllocator()->CreateBuffer(Stride * TilesNum, 0, Stride, EBufferFlags::ShaderReadable | EBufferFlags::UnorderedAccess, L"MotionVectors");
	Stride = (u32)sizeof(u32) * 2;
	u64 MaxNodes = TilesNum * 128;
	AtomicCounter = GetBuffersAllocator()->CreateAtomicCounter();
	NodesList = GetBuffersAllocator()->CreateBuffer(Stride * MaxNodes, 0, sizeof(u32) * 2, EBufferFlags::ShaderReadable | EBufferFlags::UnorderedAccess, L"MotionVectors-Nodes", AtomicCounter);
}

void RenderScene(FCommandsStream & Commands, FScene * Scene, FCamera * Camera, FSceneRenderingFrame & PrevFrame) {
	if (AlbedoTexture == nullptr) {
		FGPUContext Context;
		Context.Open(EContextType::DIRECT);
		AlbedoTexture = LoadDDS(L"Textures/sponza/Sponza_Roof_diffuse.DDS", true, Context);
		Context.ExecuteImmediately();
	}

	Commands.SetAccess(GBuffer0, EAccessType::WRITE_RT);
	Commands.SetAccess(GBuffer1, EAccessType::WRITE_RT);
	Commands.SetAccess(GBuffer2, EAccessType::WRITE_RT);
	Commands.SetAccess(SceneDepth, EAccessType::WRITE_DEPTH);

	Commands.ClearDSV(SceneDepth->GetDSV());
	Commands.ClearRTV(GBuffer0->GetRTV(), 0.f);
	Commands.ClearRTV(GBuffer1->GetRTV(), 0.f);
	Commands.ClearRTV(GBuffer2->GetRTV(), 0.f);

	FSceneRenderingFrame Frame = {};
	Frame.FrameNum = PrevFrame.FrameNum + 1;
	Frame.Scene = Scene;
	Frame.Camera = Camera;
	Frame.Resolution = Vec2u(GApplication::WindowWidth, GApplication::WindowHeight);

	const bool bInitialFrame = PrevFrame.FrameNum == 0;
	if (bInitialFrame) {
	}
	else {
		Frame.PrevViewProjection = PrevFrame.ViewProjection;
		Frame.PrevResolution = PrevFrame.Resolution;
	}

	FGBufferRenderContext GBufferViewport;
	UpdateViewport(GBufferViewport, Camera, Frame.Resolution);
	GBufferViewport.RenderTargets.Outputs.resize(3);
	GBufferViewport.RenderTargets.Outputs[0].Resource = GBuffer0;
	GBufferViewport.RenderTargets.Outputs[0].OutputSRGB = 1;
	GBufferViewport.RenderTargets.Outputs[1].Resource = GBuffer1;
	GBufferViewport.RenderTargets.Outputs[1].OutputSRGB = 0;
	GBufferViewport.RenderTargets.Outputs[2].Resource = GBuffer2;
	GBufferViewport.RenderTargets.Outputs[2].OutputSRGB = 0;
	GBufferViewport.RenderTargets.DepthBuffer = SceneDepth;
	GBufferViewport.RenderTargets.SamplesNum = 1;
	GBufferViewport.RenderTargets.Viewport = GBuffer0->GetSizeAsViewport();
	GBufferViewport.MaterialTexture = AlbedoTexture;

	Frame.ViewProjection = GBufferViewport.ViewProjectionMatrix;

	auto & TemplateShader = GetInstance<FStaticModelShaderState_GBuffer>();
	TemplateShader.Compile();

	FFrameConstants FrameConstants;
	StoreTransposed(Load(GBufferViewport.ProjectionMatrix), &FrameConstants.ProjectionMatrix);
	StoreTransposed(Load(GBufferViewport.ViewMatrix), &FrameConstants.ViewMatrix);
	StoreTransposed(Load(GBufferViewport.ViewProjectionMatrix), &FrameConstants.ViewProjectionMatrix);
	StoreTransposed(Load(GBufferViewport.InvProjectionMatrix), &FrameConstants.InvProjectionMatrix);
	StoreTransposed(Load(GBufferViewport.InvViewMatrix), &FrameConstants.InvViewMatrix);
	StoreTransposed(Load(GBufferViewport.InvViewProjectionMatrix), &FrameConstants.InvViewProjectionMatrix);
	StoreTransposed(Load(Frame.PrevViewProjection), &FrameConstants.PrevViewProjectionMatrix);
	FrameConstants.ScreenResolution = float2(Frame.Resolution);
	Frame.FrameCBV = CreateCBVFromData(&TemplateShader.FrameCB, FrameConstants);

	Frame.ViewProjection = GBufferViewport.ViewProjectionMatrix;

	Render_GBuffer(Commands, &GBufferViewport, &Frame);

	PrevFrame = Frame;
}

class FVisualizeGBufferShaderStateBase : public FShaderState {
public:
	FTextureParam DepthTexture;
	FTextureParam GBuffer0Texture;
	FTextureParam GBuffer1Texture;
	FTextureParam GBuffer2Texture;
	FTextureParam MotionVectorsListBeginTexture;
	FTextureParam NodesBuffer;
	FTextureParam VectorsBuffer;
	FConstantBuffer Constants;

	using FShaderState::FShaderState;

	void InitParams() override {
		DepthTexture = Root->CreateTextureParam(this, "DepthBuffer");
		GBuffer0Texture = Root->CreateTextureParam(this, "GBuffer0");
		GBuffer1Texture = Root->CreateTextureParam(this, "GBuffer1");
		GBuffer2Texture = Root->CreateTextureParam(this, "GBuffer2");
		MotionVectorsListBeginTexture = Root->CreateTextureParam(this, "MotionVectorsListBegin");
		NodesBuffer = Root->CreateTextureParam(this, "Nodes");
		VectorsBuffer = Root->CreateTextureParam(this, "Vectors");
		Constants = Root->CreateConstantBuffer(this, "Frame");
	}
};

template<EGBufferView Visualize>
class FVisualizeGBufferShaderState : public FVisualizeGBufferShaderStateBase {
public:
	using FVisualizeGBufferShaderStateBase::FVisualizeGBufferShaderStateBase;

	FVisualizeGBufferShaderState() :
		FVisualizeGBufferShaderStateBase(
			GetShader("Shaders/VisualizeGBuffer.hlsl", "VertexMain", "vs_5_1", {}, 0),
			GetShader("Shaders/VisualizeGBuffer.hlsl", "PixelMain", "ps_5_1", { { "SHOW_ID", Format("%d", i32(Visualize)) } }, 0)) {}
};

struct FMotionVectorsParams {
	// for tile size of 2*N+1 this is N
	Vec2u MVecTileCenter;
	Vec2u RenderTileSize;
};

class FPreprocessMotionVectorsShaderState : public FShaderState {
public:
	FConstantBuffer FrameConstants;
	FConstantBuffer Params;
	FRWTextureParam TileListBeginTexture;
	FRWTextureParam NodesBuffer;
	FRWTextureParam VectorsBuffer;
	FTextureParam GBuffer2Texture;
	FTextureParam DepthTexture;

	FPreprocessMotionVectorsShaderState() :
		FShaderState(
			GetShader("Shaders/VisualizeMotionVectors.hlsl", "PreprocessMain", "cs_5_1", {}, 0)) {}

	void InitParams() override {
		DepthTexture = Root->CreateTextureParam(this, "DepthBuffer");
		GBuffer2Texture = Root->CreateTextureParam(this, "GBuffer2");
		FrameConstants = Root->CreateConstantBuffer(this, "Frame");
		Params = Root->CreateConstantBuffer(this, "Params");
		TileListBeginTexture = Root->CreateRWTextureParam(this, "TileListBegin");
		NodesBuffer = Root->CreateRWTextureParam(this, "Nodes");
		VectorsBuffer = Root->CreateRWTextureParam(this, "Vectors");
	}
};

Vec3u PrepareKernel(Vec3u Size, Vec3u GroupSize) {
	return (Size + GroupSize - 1u) / GroupSize;
}

void PreprocessMotionVectors(FCommandsStream & Commands, FSceneRenderingFrame * Frame) {
	Commands.SetAccess(GBuffer2, EAccessType::READ_NON_PIXEL);
	Commands.SetAccess(SceneDepth, EAccessType::READ_NON_PIXEL);
	Commands.SetAccess(MotionVectorsTileListBegin, EAccessType::WRITE_UAV);
	Commands.ClearUAV(MotionVectorsTileListBegin->GetUAV(), MotionVectorsTileListBegin, (u32)(-1));
	Commands.SetAccess(AtomicCounter, EAccessType::COPY_DEST);
	Commands.SetAccess(NodesList, EAccessType::WRITE_UAV);
	Commands.SetAccess(VectorsList, EAccessType::WRITE_UAV);
	static FPipelineCache Cache;
	static FInputLayout * InputLayout = GetInputLayout({});
	static FPipelineContext<FCommandsStream> PipelineContext;
	auto & ShaderState = GetInstance<FPreprocessMotionVectorsShaderState>();
	PipelineContext.Bind(&Commands, &Cache);
	PipelineContext.SetShaderState(&ShaderState);
	PipelineContext.ApplyState();
	Commands.SetRWTexture(&ShaderState.TileListBeginTexture, MotionVectorsTileListBegin->GetUAV());
	Commands.SetRWTexture(&ShaderState.VectorsBuffer, VectorsList->GetUAV());
	Commands.SetRWTexture(&ShaderState.NodesBuffer, NodesList->GetUAV());
	Commands.SetTexture(&ShaderState.DepthTexture, SceneDepth->GetSRV() );
	Commands.SetTexture(&ShaderState.GBuffer2Texture, GBuffer2->GetSRV() );
	Commands.SetConstantBuffer(&ShaderState.FrameConstants, Frame->FrameCBV);
	FMotionVectorsParams Params;
	Params.MVecTileCenter = Vec2u(10, 10);
	Params.RenderTileSize = Vec2u(8, 8);
	Commands.SetConstantBufferData(&ShaderState.Params, &Params, sizeof(Params));
	Commands.Dispatch(PrepareKernel(PrepareKernel(SceneDepth->GetDimensions(), Vec3u(10 * 2 + 1, 10 * 2 + 1, 1)), Vec3u(8, 8, 1)));
}

void VisualizeGBufferDebug(FCommandsStream & Commands, EGBufferView Visualize, FSceneRenderingFrame * Frame) {
	Commands.SetAccess(GBuffer0, EAccessType::READ_PIXEL);
	Commands.SetAccess(GBuffer1, EAccessType::READ_PIXEL);
	Commands.SetAccess(GBuffer2, EAccessType::READ_PIXEL);
	Commands.SetAccess(SceneDepth, EAccessType::READ_PIXEL);
	
	static FPipelineCache Cache;
	static FInputLayout * InputLayout = GetInputLayout({});
	static FPipelineContext<FCommandsStream> PipelineContext;
	PipelineContext.Bind(&Commands, &Cache);
	PipelineContext.SetInputLayout(InputLayout);
	PipelineContext.SetRenderTarget(GetBackbuffer()->GetRTV(GetBackbuffer()->GetReadFormat(true)), GetBackbuffer()->GetReadFormat(true));
	PipelineContext.SetDepthStencil({});
	
	FVisualizeGBufferShaderStateBase * BaseShaderState = nullptr;
	if (Visualize == EGBufferView::Albedo) {
		auto & ShaderState = GetInstance<FVisualizeGBufferShaderState<EGBufferView::Albedo>>();
		PipelineContext.SetShaderState(&ShaderState);
		BaseShaderState = &ShaderState;
	}
	else if (Visualize == EGBufferView::Normals) {
		auto & ShaderState = GetInstance<FVisualizeGBufferShaderState<EGBufferView::Normals>>();
		PipelineContext.SetShaderState(&ShaderState);
		BaseShaderState = &ShaderState;
	}
	else if(Visualize == EGBufferView::Depth) {
		auto & ShaderState = GetInstance<FVisualizeGBufferShaderState<EGBufferView::Depth>>();
		PipelineContext.SetShaderState(&ShaderState);
		BaseShaderState = &ShaderState;
	}
	else if (Visualize == EGBufferView::MotionVectors) {
		auto & ShaderState = GetInstance<FVisualizeGBufferShaderState<EGBufferView::MotionVectors>>();
		PipelineContext.SetShaderState(&ShaderState);
		BaseShaderState = &ShaderState;

		PreprocessMotionVectors(Commands, Frame);

		Commands.SetAccess(MotionVectorsTileListBegin, EAccessType::READ_PIXEL);
		Commands.SetAccess(VectorsList, EAccessType::READ_PIXEL);
		Commands.SetAccess(NodesList, EAccessType::READ_PIXEL);
	}

	PipelineContext.ApplyState();

	Commands.SetTexture(&BaseShaderState->DepthTexture, SceneDepth->GetSRV());
	Commands.SetTexture(&BaseShaderState->GBuffer0Texture, GBuffer0->GetSRV());
	Commands.SetTexture(&BaseShaderState->GBuffer1Texture, GBuffer1->GetSRV());
	Commands.SetTexture(&BaseShaderState->GBuffer2Texture, GBuffer2->GetSRV());
	Commands.SetTexture(&BaseShaderState->MotionVectorsListBeginTexture, MotionVectorsTileListBegin->GetSRV());
	Commands.SetTexture(&BaseShaderState->NodesBuffer, NodesList->GetSRV());
	Commands.SetTexture(&BaseShaderState->VectorsBuffer, VectorsList->GetSRV());
	Commands.SetConstantBuffer(&BaseShaderState->Constants, Frame->FrameCBV);

	Commands.SetViewport(GBuffer0->GetSizeAsViewport());
	Commands.Draw(3);
}
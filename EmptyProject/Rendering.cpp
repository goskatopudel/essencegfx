#include "Rendering.h"
#include "BasePass.h"
#include "Application.h"
#include "Commands.h"
#include "VideoMemory.h"
#include "Pipeline.h"
#include "Shader.h"

FGPUResourceRef SceneDepth;
FGPUResourceRef GBuffer0;
FGPUResourceRef GBuffer1;

void AllocateGBuffer() {
	SceneDepth = GetTexturesAllocator()->CreateTexture(GApplication::WindowWidth, GApplication::WindowHeight, 1, DXGI_FORMAT_R24G8_TYPELESS, TextureFlags::ALLOW_DEPTH_STENCIL, L"SceneDepth");
	GBuffer0 = GetTexturesAllocator()->CreateTexture(GApplication::WindowWidth, GApplication::WindowHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM, TextureFlags::ALLOW_RENDER_TARGET, L"GBuffer0");
	GBuffer1 = GetTexturesAllocator()->CreateTexture(GApplication::WindowWidth, GApplication::WindowHeight, 1, DXGI_FORMAT_R16G16_UNORM, TextureFlags::ALLOW_RENDER_TARGET, L"GBuffer1");
}

void RenderScene(FCommandsStream & Commands, FScene * Scene, FCamera * Camera, FSceneRenderingFrame * PrevFrame) {

	FSceneRenderingFrame Frame = {};
	Frame.FrameNum = PrevFrame->FrameNum + 1;
	Frame.Scene = Scene;
	Frame.Camera = Camera;
	Frame.Resolution = Vec2u(GApplication::WindowWidth, GApplication::WindowHeight);

	const bool bInitialFrame = PrevFrame->FrameNum == 0;
	if (bInitialFrame) {
	}

	FGBufferRenderContext GBufferViewport;
	UpdateViewport(GBufferViewport, Camera, Frame.Resolution);
	GBufferViewport.RenderTargets.Outputs.resize(2);
	GBufferViewport.RenderTargets.Outputs[0].Resource = GBuffer0;
	GBufferViewport.RenderTargets.Outputs[0].OutputSRGB = 0;
	GBufferViewport.RenderTargets.Outputs[1].Resource = GBuffer1;
	GBufferViewport.RenderTargets.Outputs[1].OutputSRGB = 0;
	GBufferViewport.RenderTargets.DepthBuffer = SceneDepth;

	auto & TemplateShader = GetInstance<FStaticModelShaderState_GBuffer>();
	TemplateShader.Compile();

	FFrameConstants FrameConstants;
	StoreTransposed(Load(GBufferViewport.ViewProjectionMatrix), &FrameConstants.ViewProjectionMatrix);
	FrameConstants.ScreenResolution = float2(Frame.Resolution);
	Frame.FrameCBV = CreateCBVFromData(&TemplateShader.FrameCB, FrameConstants);

	Render_GBuffer(Commands, &GBufferViewport, &Frame);

	*PrevFrame = Frame;
}

enum class EGBufferVisualize {
	Depth,
	Normals
};

template<EGBufferVisualize Visualize>
class FVisualizeGBufferShaderState : public FShaderState {
public:
	FTextureParam DepthTexture;
	FTextureParam GBuffer0Texture;
	FTextureParam GBuffer1Texture;
	FConstantBuffer Constants;

	FVisualizeGBufferShaderState() :
		FShaderState(
			GetShader("Shaders/VisualizeGBuffer.hlsl", "VertexMain", "vs_5_1", {}, 0),
			GetShader("Shaders/VisualizeGBuffer.hlsl", "PixelMain", "ps_5_1", { { "SHOW_ID", Format("%d", i32(Visualize)) } }, 0)) {}

	void InitParams() override final {
		DepthTexture = Root->CreateTextureParam(this, "DepthBuffer");
		GBuffer0Texture = Root->CreateTextureParam(this, "GBuffer0");
		GBuffer1Texture = Root->CreateTextureParam(this, "GBuffer1");
		Constants = Root->CreateConstantBuffer(this, "Frame");
	}
};

void X() {
	if (1) {
		static FVisualizeGBufferShaderState<EGBufferVisualize::Depth> DepthShaderState;

	}

}
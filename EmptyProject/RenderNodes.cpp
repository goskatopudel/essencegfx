#include "RenderNodes.h"
#include "Shader.h"
#include "Pipeline.h"

FPipelineCache PipelineCache;

class FCopyShaderState : public FShaderState {
public:
	FSRVParam SourceTexture;
	FCBVParam Constants;

	FCopyShaderState() :
		FShaderState(
			GetGlobalShader("Shaders/Utility.hlsl", "VertexMain", "vs_5_1", {}, 0),
			GetGlobalShader("Shaders/Utility.hlsl", "CopyPixelMain", "ps_5_1", {}, 0)) {}

	void InitParams() override final {
		SourceTexture = Root->CreateSRVParam(this, "SourceTexture");
		Constants = Root->CreateCBVParam(this, "Constants");
	}
};

void FCopyTexture::Process(FCommandsStream & CmdStream) {
	auto & ShaderState = GetInstance<FCopyShaderState>();

	CmdStream.SetAccess(GetInput(0)->GetResource(), EAccessType::READ_PIXEL);
	CmdStream.SetAccess(GetOutput(0)->GetResource(), EAccessType::WRITE_RT);
	FInputLayout * InputLayout = GetInputLayout({});

	FStateProxy<FCommandsStream> PipelineProxy(CmdStream, PipelineCache);
	PipelineProxy.SetShaderState(&ShaderState);
	PipelineProxy.SetInputLayout(InputLayout);
	if (OutputSrgb) {
		auto writeFmt = GetOutput(0)->GetResource()->GetWriteFormat(OutputSrgb);
		PipelineProxy.SetRenderTarget(GetOutput(0)->GetResource()->GetRTV(writeFmt), 0);
	}
	else {
		PipelineProxy.SetRenderTarget(GetOutput(0)->GetOutputDesc().RTV, 0);
	}

	PipelineProxy.ApplyState();

	CmdStream.SetTexture(&ShaderState.SourceTexture, GetInput(0)->GetResource()->GetSRV());

	D3D12_VIEWPORT Viewport;
	if (!DstRect.IsValid()) {
		DstRect = GetOutput(0)->GetOutputDesc().Rect;
	}
	Viewport.TopLeftX = (float)DstRect.X0;
	Viewport.TopLeftY = (float)DstRect.Y0;
	Viewport.Width = (float)DstRect.X1;
	Viewport.Height = (float)DstRect.Y1;

	CmdStream.SetViewport(Viewport);
	CmdStream.Draw(3);
}
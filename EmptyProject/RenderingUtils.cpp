#include "RenderingUtils.h"
#include "Resource.h"
#include "CommandStream.h"
#include "Viewport.h"
#include "Pipeline.h"
#include "Shader.h"

class FCopyShaderState : public FShaderState {
public:
	FTextureParam SourceTexture;
	FConstantBuffer Constants;

	FCopyShaderState() :
		FShaderState(
			GetShader("Shaders/Utility.hlsl", "VertexMain", "vs_5_1", {}, 0),
			GetShader("Shaders/Utility.hlsl", "CopyPixelMain", "ps_5_1", {}, 0)) {}

	void InitParams() override final {
		SourceTexture = Root->CreateTextureParam(this, "SourceTexture");
		Constants = Root->CreateConstantBuffer(this, "Constants");
	}
};

class FDownsampleDepthShaderState : public FShaderState {
public:
	FTextureParam SourceTexture;

	FDownsampleDepthShaderState() :
		FShaderState(
			GetShader("Shaders/Utility.hlsl", "VertexMain", "vs_5_1", {}, 0),
			GetShader("Shaders/Utility.hlsl", "DownsampleDepthMain", "ps_5_1", {}, 0)) {}

	void InitParams() override final {
		SourceTexture = Root->CreateTextureParam(this, "SourceTexture");
	}
};

void DrawTexture(FCommandsStream & Context, FGPUResource * Texture, float2 Location, float2 Size, ETextureFiltering Filtering, u32 Mipmap, FRenderTargetsBundle & RenderTargets) {
	Context.SetAccess(Texture, EAccessType::READ_PIXEL);

	auto & ShaderState = GetInstance<FCopyShaderState>();

	static FPipelineCache Cache;
	static FInputLayout * InputLayout = GetInputLayout({});
	static FPipelineContext<FCommandsStream> PipelineContext;
	PipelineContext.Bind(&Context, &Cache);
	PipelineContext.SetShaderState(&ShaderState);
	PipelineContext.SetInputLayout(InputLayout);
	PipelineContext.SetRenderTargetsBundle(&RenderTargets);
	PipelineContext.ApplyState();
	
	Context.SetTexture(&ShaderState.SourceTexture, Texture->GetSRV());
	Context.SetConstantBufferData(&ShaderState.Constants, &Mipmap, sizeof(Mipmap));

	D3D12_VIEWPORT Viewport = RenderTargets.Viewport;
	Viewport.TopLeftX = Location.x;
	Viewport.TopLeftY = Location.y;
	Viewport.Width = Size.x;
	Viewport.Height = Size.y;
	Context.SetViewport(Viewport);
	Context.Draw(3);
}

void GenerateMipmaps(FCommandsStream & Context, FGPUResource * Texture) {
	u32 MipmapsNum = Texture->GetMipmapsNum();

	check(Texture->IsRenderTarget() || Texture->IsDepthStencil());

	static FInputLayout * InputLayout = GetInputLayout({});

	if (Texture->IsRenderTarget()) {
		auto & ShaderState = GetInstance<FCopyShaderState>();
		static FPipelineCache Cache;
		static FPipelineContext<FCommandsStream> PipelineContext;
		PipelineContext.Bind(&Context, &Cache);
		PipelineContext.SetShaderState(&ShaderState);
		PipelineContext.SetInputLayout(InputLayout);
		PipelineContext.SetRenderTarget(Texture->GetRTV(), Texture->GetWriteFormat(), 0);
		PipelineContext.SetDepthStencil({});
		PipelineContext.ApplyState();

		auto Viewport = Texture->GetSizeAsViewport();

		for (u32 Mip = 1; Mip < MipmapsNum; ++Mip) {
			Context.SetAccess(Texture, EAccessType::READ_PIXEL, Mip - 1);
			Context.SetAccess(Texture, EAccessType::WRITE_RT, Mip);
			Viewport.Width /= 2;
			Viewport.Height /= 2;
			Context.SetTexture(&ShaderState.SourceTexture, Texture->GetSRV(Mip - 1));
			Context.SetRenderTarget(Texture->GetRTV(Mip), 0);
			Context.SetViewport(Viewport);
			Context.Draw(3);
		}
	}
	
	if (Texture->IsDepthStencil()) {
		auto & ShaderState = GetInstance<FDownsampleDepthShaderState>();
		static FPipelineCache Cache;
		static FPipelineContext<FCommandsStream> PipelineContext;
		PipelineContext.Bind(&Context, &Cache);
		PipelineContext.SetShaderState(&ShaderState);
		PipelineContext.SetInputLayout(InputLayout);
		PipelineContext.SetRenderTarget({}, DXGI_FORMAT_UNKNOWN, 0);
		D3D12_DEPTH_STENCIL_DESC Desc;
		SetD3D12StateDefaults(&Desc);
		Desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		PipelineContext.SetDepthStencilState(Desc);

		auto Viewport = Texture->GetSizeAsViewport();

		for (u32 Mip = 1; Mip < MipmapsNum; ++Mip) {
			Context.SetAccess(Texture, EAccessType::READ_PIXEL, Mip - 1);
			Context.SetAccess(Texture, EAccessType::WRITE_DEPTH, Mip);
			Viewport.Width /= 2;
			Viewport.Height /= 2;
			PipelineContext.SetDepthStencil(Texture->GetDSV(Mip), Texture->GetWriteFormat());
			PipelineContext.ApplyState();
			Context.SetTexture(&ShaderState.SourceTexture, Texture->GetSRV(Mip - 1));
			Context.SetViewport(Viewport);
			Context.Draw(3);
		}
	}
}

class FBlurShaderState : public FShaderState {
public:
	FTextureParam SourceTexture;

	FBlurShaderState() :
		FShaderState(
			GetShader("Shaders/Utility.hlsl", "VertexMain", "vs_5_1", {}, 0),
			GetShader("Shaders/Utility.hlsl", "BlurPixelMain", "ps_5_1", {}, 0)) {}

	void InitParams() override final {
		SourceTexture = Root->CreateTextureParam(this, "SourceTexture");
	}
};

void BlurTexture(FCommandsStream & Context, FGPUResource * SrcTexture, FGPUResource * OutTexture) {
	Context.SetAccess(SrcTexture, EAccessType::READ_PIXEL);
	Context.SetAccess(OutTexture, EAccessType::WRITE_RT);

	static FPipelineCache Cache;
	auto & ShaderState = GetInstance<FBlurShaderState>();
	static FInputLayout * InputLayout = GetInputLayout({});

	static FPipelineContext<FCommandsStream> PipelineContext;
	PipelineContext.Bind(&Context, &Cache);
	PipelineContext.SetShaderState(&ShaderState);
	PipelineContext.SetInputLayout(InputLayout);
	PipelineContext.SetRenderTarget(OutTexture->GetRTV(), OutTexture->GetWriteFormat(), 0);
	PipelineContext.SetDepthStencil({});
	PipelineContext.ApplyState();
	
	Context.SetTexture(&ShaderState.SourceTexture, SrcTexture->GetSRV());

	Context.Draw(3);
}
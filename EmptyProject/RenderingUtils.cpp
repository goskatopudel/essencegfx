#include "RenderingUtils.h"
#include "Resource.h"
#include "CommandStream.h"
#include "Pipeline.h"
#include "Shader.h"

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

class FDownsampleDepthShaderState : public FShaderState {
public:
	FSRVParam SourceTexture;

	FDownsampleDepthShaderState() :
		FShaderState(
			GetGlobalShader("Shaders/Utility.hlsl", "VertexMain", "vs_5_1", {}, 0),
			GetGlobalShader("Shaders/Utility.hlsl", "DownsampleDepthMain", "ps_5_1", {}, 0)) {}

	void InitParams() override final {
		SourceTexture = Root->CreateSRVParam(this, "SourceTexture");
	}
};

void DrawTexture(FCommandsStream & Context, FGPUResource * Texture, float2 Location, float2 Size, ETextureFiltering Filtering, u32 Mipmap, FRenderTargetsBundle & RenderTargets) {
	//Context.SetAccess(Texture, EAccessType::READ_PIXEL);

	//auto & ShaderState = GetInstance<FCopyShaderState>();

	//static FPipelineCache Cache;
	//static FInputLayout * InputLayout = GetInputLayout({});
	//static FPipelineContext<FCommandsStream> PipelineContext;
	//PipelineContext.Bind(&Context, &Cache);
	//PipelineContext.SetShaderState(&ShaderState);
	//PipelineContext.SetInputLayout(InputLayout);
	//PipelineContext.SetRenderTargetsBundle(&RenderTargets);
	//PipelineContext.ApplyState();
	//
	//Context.SetTexture(&ShaderState.SourceTexture, Texture->GetSRV());
	//Context.SetConstantBufferData(&ShaderState.Constants, &Mipmap, sizeof(Mipmap));

	//D3D12_VIEWPORT Viewport = RenderTargets.Viewport;
	//Viewport.TopLeftX = Location.x;
	//Viewport.TopLeftY = Location.y;
	//Viewport.Width = Size.x;
	//Viewport.Height = Size.y;
	//Context.SetViewport(Viewport);
	//Context.Draw(3);
}

void GenerateMipmaps(FCommandsStream & Context, FGPUResource * Texture) {
	u32 MipmapsNum = Texture->GetMipmapsNum();

	check(Texture->IsRenderTarget() || Texture->IsDepthStencil());

	static FInputLayout * InputLayout = GetInputLayout({});

	if (Texture->IsRenderTarget()) {
		auto & ShaderState = GetInstance<FCopyShaderState>();
		static FPipelineCache Cache;
		static FStateProxy<FCommandsStream> PipelineContext;
		PipelineContext.Bind(Context, Cache);
		PipelineContext.SetShaderState(&ShaderState);
		PipelineContext.SetInputLayout(InputLayout);
		PipelineContext.SetRenderTarget(Texture->GetRTV(), 0);
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
		static FStateProxy<FCommandsStream> PipelineContext;
		PipelineContext.Bind(Context, Cache);
		PipelineContext.SetShaderState(&ShaderState);
		PipelineContext.SetInputLayout(InputLayout);
		PipelineContext.SetRenderTarget({}, 0);
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
			PipelineContext.SetDepthStencil(Texture->GetDSV(Mip));
			PipelineContext.ApplyState();
			Context.SetTexture(&ShaderState.SourceTexture, Texture->GetSRV(Mip - 1));
			Context.SetViewport(Viewport);
			Context.Draw(3);
		}
	}
}

class FBlurShaderState : public FShaderState {
public:
	FSRVParam SourceTexture;

	FBlurShaderState() :
		FShaderState(
			GetGlobalShader("Shaders/Utility.hlsl", "VertexMain", "vs_5_1", {}, 0),
			GetGlobalShader("Shaders/Utility.hlsl", "BlurPixelMain", "ps_5_1", {}, 0)) {}

	void InitParams() override final {
		SourceTexture = Root->CreateSRVParam(this, "SourceTexture");
	}
};

void BlurTexture(FCommandsStream & Context, FGPUResource * SrcTexture, FGPUResource * OutTexture) {
	Context.SetAccess(SrcTexture, EAccessType::READ_PIXEL);
	Context.SetAccess(OutTexture, EAccessType::WRITE_RT);

	static FPipelineCache Cache;
	auto & ShaderState = GetInstance<FBlurShaderState>();
	static FInputLayout * InputLayout = GetInputLayout({});

	static FStateProxy<FCommandsStream> PipelineContext;
	PipelineContext.Bind(Context, Cache);
	PipelineContext.SetShaderState(&ShaderState);
	PipelineContext.SetInputLayout(InputLayout);
	PipelineContext.SetRenderTarget(OutTexture->GetRTV(), 0);
	PipelineContext.SetDepthStencil({});
	PipelineContext.ApplyState();
	
	Context.SetTexture(&ShaderState.SourceTexture, SrcTexture->GetSRV());

	Context.Draw(3);
}

void FromSimdT(DirectX::CXMMATRIX Matrix, float4x4 * Out) {
	DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)Out, DirectX::XMMatrixTranspose(Matrix));
}

DirectX::XMMATRIX ToSimd(float4x4 const & Matrix) {
	return DirectX::XMLoadFloat4x4((DirectX::XMFLOAT4X4 const*)&Matrix);
}


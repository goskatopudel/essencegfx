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

template<typename T>
T& GetInstance() {
	static T Instance;
	return Instance;
}

void DrawTexture(FCommandsStream & Context, FGPUResource * Texture, float2 Location, float2 Size, u32 Mipmap, FRenderTargetContext & RTContext) {
	Context.SetAccess(Texture, EAccessType::READ_PIXEL);
	Context.SetRenderTargets(&RTContext);

	auto & ShaderState = GetInstance<FCopyShaderState>();

	static FPipelineFactory Factory;
	Factory.SetShaderState(&ShaderState);
	static FInputLayout * InputLayout = GetInputLayout({});
	Factory.SetInputLayout(InputLayout);
	Factory.SetRenderTargets(&RTContext);

	Context.SetPipelineState(Factory.GetPipelineState());
	Context.SetTexture(&ShaderState.SourceTexture, Texture->GetSRV());
	Context.SetConstantBufferData(&ShaderState.Constants, &Mipmap, sizeof(Mipmap));
	D3D12_VIEWPORT Viewport = RTContext.Viewport;
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

	if (Texture->IsRenderTarget()) {
		auto & ShaderState = GetInstance<FCopyShaderState>();
		static FPipelineFactory Factory;
		Factory.SetShaderState(&ShaderState);
		static FInputLayout * InputLayout = GetInputLayout({});
		Factory.SetInputLayout(InputLayout);
		Factory.SetRenderTarget(Texture->GetFormat(), 0);
		Factory.SetDepthStencil(DXGI_FORMAT_UNKNOWN);
		Context.SetPipelineState(Factory.GetPipelineState());
		Context.SetDepthStencil({});

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
		static FPipelineFactory Factory;
		Factory.SetShaderState(&ShaderState);
		static FInputLayout * InputLayout = GetInputLayout({});
		Factory.SetInputLayout(InputLayout);
		Factory.SetRenderTarget({}, 0);
		Factory.SetDepthStencil(Texture->GetWriteFormat());
		D3D12_DEPTH_STENCIL_DESC Desc;
		SetD3D12StateDefaults(&Desc);
		Desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		Factory.SetDepthStencilState(Desc);
		Context.SetPipelineState(Factory.GetPipelineState());
		Context.SetRenderTarget({}, 0);

		auto Viewport = Texture->GetSizeAsViewport();

		for (u32 Mip = 1; Mip < MipmapsNum; ++Mip) {
			Context.SetAccess(Texture, EAccessType::READ_PIXEL, Mip - 1);
			Context.SetAccess(Texture, EAccessType::WRITE_DEPTH, Mip);
			Viewport.Width /= 2;
			Viewport.Height /= 2;
			Context.SetTexture(&ShaderState.SourceTexture, Texture->GetSRV(Mip - 1));
			Context.SetDepthStencil(Texture->GetDSV(Mip));
			Context.SetViewport(Viewport);
			Context.Draw(3);
		}
	}
}
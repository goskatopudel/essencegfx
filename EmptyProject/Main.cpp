#include "Essence.h"
#include "Application.h"
#include "Win32Application.h"
#include "ImGui\imgui.h"
#include "MathFunctions.h"
#include "Shader.h"
#include "UIUtils.h"
#include "CommandStream.h"
#include "Pipeline.h"
#include "VideoMemory.h"
#include "Model.h"

class FApplicationImpl : public FApplication {
public:
	using FApplication::FApplication;

	void AllocateScreenResources() final override;
	void Init() final override;
	void Shutdown() final override;
	bool Update() final override;
};

#include "Camera.h"

FCamera Camera;

void UpdateCamera() {
	ImGuiIO& io = ImGui::GetIO();
	static float rx = 0;
	static float ry = 0;
	if (!io.WantCaptureMouse && !io.WantCaptureKeyboard)
	{
		float activeSpeed = 10 * io.DeltaTime;

		if (io.KeysDown[VK_SHIFT]) {
			activeSpeed *= 5.f;
		}
		if (io.KeysDown[VK_CONTROL]) {
			activeSpeed *= 0.2f;
		}
		if (io.KeysDown['W']) {
			Camera.Dolly(activeSpeed);
		}
		if (io.KeysDown['S']) {
			Camera.Dolly(-activeSpeed);
		}
		if (io.KeysDown['A']) {
			Camera.Strafe(-activeSpeed);
		}
		if (io.KeysDown['D']) {
			Camera.Strafe(activeSpeed);
		}
		if (io.KeysDown['Q']) {
			Camera.Roll(-io.DeltaTime * DirectX::XM_PI * 0.66f);
		}
		if (io.KeysDown['E']) {
			Camera.Roll(io.DeltaTime * DirectX::XM_PI * 0.66f);
		}
		if (io.KeysDown[VK_SPACE]) {
			Camera.Climb(activeSpeed);
		}
	}

	float dx = (float)io.MouseDelta.x / (float)GApplication::WindowWidth;
	float dy = (float)io.MouseDelta.y / (float)GApplication::WindowHeight;
	if (io.MouseDown[1]) {
		Camera.Rotate(dy, dx);
	}
}

#include "Scene.h"

class FStaticModelShaderState_Debug : public FShaderState {
public:
	FConstantBuffer ConstantBuffer;
	FTextureParam UVTexture;
	FTextureParam ShadowmapTexture;

	struct FConstantBufferData {
		float4x4 ViewProj;
		float4x4 World;
		float4x4 InvView;
		float2 Resolution;
		float2 Padding;
		float4x4 WorldToShadow;
		float3 L;
	};

	FStaticModelShaderState_Debug() :
		FShaderState(
			GetShader("Shaders/DebugModel.hlsl", "VShader", "vs_5_1", {}, 0),
			GetShader("Shaders/DebugModel.hlsl", "PSDebug", "ps_5_1", {}, 0)) {}

	void InitParams() override final {
		ConstantBuffer = Root->CreateConstantBuffer(this, "Constants");
		UVTexture = Root->CreateTextureParam(this, "UVTexture");
		ShadowmapTexture = Root->CreateTextureParam(this, "ShadowmapTexture");
	}
};

constexpr FShader * SKIP_SHADER = nullptr;

class FStaticModelShaderState_Depth : public FShaderState {
public:
	FConstantBuffer ConstantBuffer;
	
	struct FConstantBufferData {
		float4x4 ViewProj;
		float4x4 World;
		float4x4 InvView;
	};

	FStaticModelShaderState_Depth() :
		FShaderState(
			GetShader("Shaders/DebugModel.hlsl", "VShader", "vs_5_1", {}, 0), 
			SKIP_SHADER) {}

	void InitParams() override final {
		ConstantBuffer = Root->CreateConstantBuffer(this, "Constants");
	}
};

#include "Viewport.h"

FScene Scene;
FGPUResourceRef DepthBuffer;
FGPUResourceRef Shadowmap;

FGPUResourceRef Texture;

struct FShadowRenderingParams {
	u32	Resolution = 1024;
	float2 LightAngles = float2(0, 0);

	float3 LightDirection;
} ShadowRenderingParams;

void ShowShadowmapOptions() {
	ImGui::Begin("Shadowmap");
	
	const char* items[] = { "512", "1024", "2048", "4096" };
	const u32 values[] = { 512, 1024, 2048, 4096 };
	static int item = 1;
	ImGui::Combo("Resolution", &item, items, _ARRAYSIZE(items)); 
	ImGui::End();

	ImGui::SliderAngle("Azimuthal angle", &ShadowRenderingParams.LightAngles.x, 0.f, 360.f);
	ImGui::SliderAngle("Polar angle", &ShadowRenderingParams.LightAngles.y, 0.f, 180.f);

	ShadowRenderingParams.LightDirection = float3(
		sinf(ShadowRenderingParams.LightAngles.y) * cosf(ShadowRenderingParams.LightAngles.x),
		cosf(ShadowRenderingParams.LightAngles.y),
		sinf(ShadowRenderingParams.LightAngles.y) * sinf(ShadowRenderingParams.LightAngles.x)
		);

	ShadowRenderingParams.Resolution = values[item];

	if (Shadowmap->GetDimensions().x != ShadowRenderingParams.Resolution) {
		Shadowmap = GetTexturesAllocator()->CreateTexture(ShadowRenderingParams.Resolution, ShadowRenderingParams.Resolution, 1, DXGI_FORMAT_R32_TYPELESS, TextureFlags::ALLOW_DEPTH_STENCIL, L"Shadowmap");
	}
}

struct FSceneRenderContext_Shadowmap : public FRT0Context {
	FScene * Scene;
	FCamera * Camera;
};

void PreRender_Shadowmap(FCommandsStream & Commands, FSceneRenderContext_Shadowmap & SceneContext) {
	Commands.SetAccess(SceneContext.DepthBuffer, EAccessType::WRITE_DEPTH);
	Commands.ClearDSV(SceneContext.DepthBuffer->GetDSV());
	if (SceneContext.DepthBuffer) {
		Commands.SetDepthStencil(SceneContext.DepthBuffer->GetDSV());
	}
	Commands.SetViewport(SceneContext.DepthBuffer->GetSizeAsViewport());
}

void RenderModel_Shadowmap(FCommandsStream & Commands, FSceneRenderContext_Shadowmap & SceneContext, FSceneStaticMesh * StaticMesh) {
	// 
	static FStaticModelShaderState_Depth ShaderState;
	static FPipelineState * PipelineState;

	static FInputLayout * StaticMeshInputLayout = GetInputLayout({
		CreateInputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("TANGENT", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("BITANGENT", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 0, 0),
		CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 1, 0),
		CreateInputElement("COLOR", DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0),
	});

	static FPipelineFactory Factory;
	Factory.SetInputLayout(StaticMeshInputLayout);
	Factory.SetDepthStencil(SceneContext.DepthBuffer->GetWriteFormat());
	Factory.SetShaderState(&ShaderState);
	Factory.SetTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	PipelineState = Factory.GetPipelineState();
	Commands.SetPipelineState(PipelineState);

	FStaticModelShaderState_Debug::FConstantBufferData Constants;
	Constants.ViewProj = SceneContext.Viewport.TViewProjectionMatrix;
	Constants.InvView = SceneContext.Viewport.TInvViewMatrix;
	CreateWorldMatrixT(StaticMesh->Position, 1, Constants.World);
	Commands.SetConstantBuffer(&ShaderState.ConstantBuffer, CreateCBVFromData(&ShaderState.ConstantBuffer, Constants));

	Commands.SetVB(StaticMesh->Model->VertexBuffer, 0);
	Commands.SetIB(StaticMesh->Model->IndexBuffer);
	for (auto & Mesh : StaticMesh->Model->Meshes) {
		Commands.DrawIndexed(Mesh.IndicesNum, Mesh.StartIndex, Mesh.BaseVertex);
	}
}

void Render_Shadowmap(FCommandsStream & Stream, FSceneRenderContext_Shadowmap SceneContext) {

	PreRender_Shadowmap(Stream, SceneContext);
	for (auto StaticMeshPtr : SceneContext.Scene->StaticMeshes) {
		RenderModel_Shadowmap(Stream, SceneContext, StaticMeshPtr);
	}
}

struct FSceneRenderContext_Forward : public FRT1Context {
	FScene * Scene;
	FCamera * Camera;
	float4x4 WorldToShadowMatrix;
	float3 L;
};

void PreRender_Forward(FCommandsStream & Commands, FSceneRenderContext_Forward & SceneContext) {
	Commands.SetAccess(SceneContext.RenderTargets[0].Resource, EAccessType::WRITE_RT);
	if (SceneContext.DepthBuffer) {
		Commands.SetAccess(SceneContext.DepthBuffer, EAccessType::WRITE_DEPTH);
	}
	Commands.SetAccess(Shadowmap, EAccessType::READ_PIXEL);
	Commands.ClearDSV(SceneContext.DepthBuffer->GetDSV());

	Commands.SetRenderTarget(0, SceneContext.RenderTargets[0].GetRTV());
	if (SceneContext.DepthBuffer) {
		Commands.SetDepthStencil(SceneContext.DepthBuffer->GetDSV());
	}
	Commands.SetViewport(SceneContext.RenderTargets[0].Resource->GetSizeAsViewport());
}

void RenderModel_Forward(FCommandsStream & Commands, FSceneRenderContext_Forward & SceneContext, FSceneStaticMesh * StaticMesh) {
	// 
	static FStaticModelShaderState_Debug ShaderState;
	static FPipelineState * PipelineState;

	static FInputLayout * StaticMeshInputLayout = GetInputLayout({
		CreateInputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("TANGENT", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("BITANGENT", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 0, 0),
		CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 1, 0),
		CreateInputElement("COLOR", DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0),
	});

	static FPipelineFactory Factory;
	Factory.SetInputLayout(StaticMeshInputLayout);
	if (SceneContext.DepthBuffer) {
		Factory.SetDepthStencil(SceneContext.DepthBuffer->GetWriteFormat());
	}
	Factory.SetRenderTarget(SceneContext.RenderTargets[0].GetFormat(), 0);
	Factory.SetShaderState(&ShaderState);
	Factory.SetTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	PipelineState = Factory.GetPipelineState();
	Commands.SetPipelineState(PipelineState);

	FStaticModelShaderState_Debug::FConstantBufferData Constants;
	Constants.ViewProj = SceneContext.Viewport.TViewProjectionMatrix;
	Constants.InvView = SceneContext.Viewport.TInvViewMatrix;
	Constants.Resolution = float2(SceneContext.Viewport.Resolution);
	Constants.WorldToShadow = SceneContext.WorldToShadowMatrix;
	Constants.L = SceneContext.L;
	CreateWorldMatrixT(StaticMesh->Position, 1, Constants.World);
	Commands.SetConstantBuffer(&ShaderState.ConstantBuffer, CreateCBVFromData(&ShaderState.ConstantBuffer, Constants));
	Commands.SetTexture(&ShaderState.UVTexture, Texture->GetSRV());
	Commands.SetTexture(&ShaderState.ShadowmapTexture, Shadowmap->GetSRV());

	Commands.SetVB(StaticMesh->Model->VertexBuffer, 0);
	Commands.SetIB(StaticMesh->Model->IndexBuffer);
	for (auto & Mesh : StaticMesh->Model->Meshes) {
		Commands.DrawIndexed(Mesh.IndicesNum, Mesh.StartIndex, Mesh.BaseVertex);
	}
}

void Render_Forward(FCommandsStream & Stream, FSceneRenderContext_Forward SceneContext) {

	PreRender_Forward(Stream, SceneContext);
	for (auto StaticMeshPtr : SceneContext.Scene->StaticMeshes) {
		RenderModel_Forward(Stream, SceneContext, StaticMeshPtr);
	}
}

void FApplicationImpl::Init() {
	Camera.Position = float3(0, 0.f, -10.f);
	Camera.Up = float3(0, 1.f, 0);
	Camera.Direction = normalize(float3(0) - Camera.Position);

	Scene.AddStaticMesh(L"Tree", GetModel(L"Models/tree.obj"));

	FGPUContext Context;
	Context.Open(EContextType::DIRECT);
	Scene.Prepare(Context);
	Texture = LoadDDS(L"textures/uvchecker.DDS", true, Context);
	Context.Close();
	Context.ExecuteImmediately();

	Shadowmap = GetTexturesAllocator()->CreateTexture(1024, 1024, 1, DXGI_FORMAT_R32_TYPELESS, TextureFlags::ALLOW_DEPTH_STENCIL, L"Shadowmap");
}

void FApplicationImpl::AllocateScreenResources() {
	DepthBuffer = GetTexturesAllocator()->CreateTexture(GApplication::WindowWidth, GApplication::WindowHeight, 1, DXGI_FORMAT_R24G8_TYPELESS, TextureFlags::ALLOW_DEPTH_STENCIL, L"DepthBuffer");
}

void FApplicationImpl::Shutdown() {

}

class FCopyShaderState : public FShaderState {
public:
	FTextureParam SourceTexture;

	FCopyShaderState() :
		FShaderState(
			GetShader("Shaders/Utility.hlsl", "VertexMain", "vs_5_1", {}, 0),
			GetShader("Shaders/Utility.hlsl", "CopyPixelMain", "ps_5_1", {}, 0)) {}

	void InitParams() override final {
		SourceTexture = Root->CreateTextureParam(this, "SourceTexture");
	}
};

void DrawTexture(FCommandsStream & Context, FGPUResource * Texture, float2 Location, float2 Size, FRT1Context & RTContext) {
	Context.SetAccess(Texture, EAccessType::READ_PIXEL);
	Context.SetAccess(RTContext.RenderTargets[0].Resource, EAccessType::WRITE_RT);

	static FCopyShaderState ShaderState;
	static FPipelineFactory Factory;
	Factory.SetTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Factory.SetRenderTarget(RTContext.RenderTargets[0].GetFormat(), 0);
	Factory.SetShaderState(&ShaderState);
	static FInputLayout * InputLayout = GetInputLayout({});
	Factory.SetInputLayout(InputLayout);
	
	Context.SetRenderTarget(0, RTContext.RenderTargets[0].GetRTV());
	Context.SetDepthStencil({});
	Context.SetPipelineState(Factory.GetPipelineState());
	Context.SetTexture(&ShaderState.SourceTexture, Texture->GetSRV());
	D3D12_VIEWPORT Viewport = RTContext.RenderTargets[0].Resource->GetSizeAsViewport();
	Viewport.TopLeftX = Location.x;
	Viewport.TopLeftY = Location.y;
	Viewport.Width = Size.x;
	Viewport.Height = Size.y;
	Context.SetViewport(Viewport);
	Context.Draw(3);
}

#include "DebugPrimitivesRendering.h"

bool FApplicationImpl::Update() {
	ImGuiIO& io = ImGui::GetIO();

	UpdateCamera();

	if (io.KeyCtrl && io.KeysDown['R'] && io.KeysDownDuration['R'] == 0.f) {
		GetDirectQueue()->WaitForCompletion();

		RecompileChangedShaders();
		RecompileChangedPipelines();
	}

	ShowAppStats();
	ShowShadowmapOptions();

	ImGui::ShowTestWindow();

	using namespace DirectX;

	static FCommandsStream Stream;
	Stream.Reset();
	Stream.SetAccess(GetBackbuffer(), EAccessType::WRITE_RT);
	Stream.ClearRTV(GetBackbuffer()->GetRTV(), 0.f);
	Stream.SetViewport(GetBackbuffer()->GetSizeAsViewport());

	FGPUContext Context;
	Context.Open(EContextType::DIRECT);

	FSceneRenderContext_Shadowmap ShadowmapContext;
	ShadowmapContext.DepthBuffer = Shadowmap;
	ShadowmapContext.Scene = &Scene;
	GenerateShadowmapViewport(ShadowmapContext.Viewport, Vec2i(Shadowmap->GetDimensions().x, Shadowmap->GetDimensions().y), ShadowRenderingParams.LightDirection * -1.f);

	Render_Shadowmap(Stream, ShadowmapContext);

	FSceneRenderContext_Forward SceneContext;
	SceneContext.RenderTargets[0].Resource = GetBackbuffer();
	SceneContext.RenderTargets[0].OutputSRGB = 1;
	SceneContext.DepthBuffer = DepthBuffer;
	GenerateViewport(SceneContext.Viewport, &Camera, Vec2i(GApplication::WindowWidth, GApplication::WindowHeight));
	SceneContext.Scene = &Scene;
	SceneContext.Camera = &Camera;
	SceneContext.WorldToShadowMatrix = ShadowmapContext.Viewport.TViewProjectionMatrix;
	SceneContext.L = ShadowRenderingParams.LightDirection;
	Render_Forward(Stream, SceneContext);

	DrawTexture(Stream, Shadowmap, 0.f, float2(128, 128), SceneContext);

	Stream.Close();
	Playback(Context, &Stream);

	static FDebugPrimitivesAccumulator DebugAcc;
	DebugAcc.AddLine(float3(0), float3(1, 0, 0), Color4b(255, 0, 0, 255));
	DebugAcc.AddLine(float3(0), float3(0, 1, 0), Color4b(0, 255, 0, 255));
	DebugAcc.AddLine(float3(0), float3(0, 0, 1), Color4b(0, 0, 255, 255));

	u16 GridBarsPerDimNum = 11;
	float GridSpan = 20.f;
	float GridInc = GridSpan / (GridBarsPerDimNum - 1);
	float3 GridZero = float3(0.f, -0.1f, 0.f);

	for (u16 I = 0; I < GridBarsPerDimNum; ++I) {
		Color4b Color = ToColor4b((I % 2) ? 0.2f : 0.1f, true);
		float3 A = float3(-GridSpan * 0.5f, 0, -GridSpan * 0.5f + I * GridInc) + GridZero;
		DebugAcc.AddLine(A, A + float3(GridSpan, 0, 0), Color);
		float3 B = float3(-GridSpan * 0.5f + I * GridInc, 0, -GridSpan * 0.5f) + GridZero;
		DebugAcc.AddLine(B, B + float3(0, 0, GridSpan), Color);
	}

	DebugAcc.FlushToViewport(Context, SceneContext);

	Context.Execute();

	return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	FApplicationImpl SampleApp(L"Essence2", 1024, 768);
	return Win32::Run(&SampleApp, hInstance, nCmdShow);
}
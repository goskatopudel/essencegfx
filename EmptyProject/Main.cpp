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

	float dx = (float)io.MouseDelta.x / (float)GApplication::WndWidth;
	float dy = (float)io.MouseDelta.y / (float)GApplication::WndHeight;
	if (io.MouseDown[1]) {
		Camera.Rotate(dy, dx);
	}
}

void FApplicationImpl::Init() {
	Camera.Position = float3(0, 0.f, -10.f);
	Camera.Up = float3(0, 1.f, 0);
	Camera.Direction = normalize(float3(0) - Camera.Position);
}

void FApplicationImpl::AllocateScreenResources() {

}

void FApplicationImpl::Shutdown() {

}

struct FPatch {
	FOwnedResource	VertexBuffer;
	FOwnedResource	IndexBuffer;
	u32 VBSize;
	u32 IBSize;
	u32 IndicesNum;
};

struct FPatchVertex {
	float3 Position;
};

class FPatchShaderState : public FShaderState {
public:
	FConstantBuffer			ConstantBuffer;

	struct FConstantBufferData {
		float4x4			ViewProjectionMatrix;
	};

	FPatchShaderState() :
		FShaderState(
			GetShader("Shaders/Tesselate.hlsl", "VShader", "vs_5_0", {}, 0),
			GetShader("Shaders/Tesselate.hlsl", "PShader", "ps_5_0", {}, 0)) {}

	void InitParams() override final {
		ConstantBuffer = Root->CreateConstantBuffer(this, "FrameConstants");
	}
};

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

	static FPatch Patch;
	static bool bInit = true;
	if (bInit) {
		const u32 VerticesNum = 4;
		const FPatchVertex VData[VerticesNum] = {
			{ float3(-0.5f,-0.5f, 0) },
			{ float3( 0.5f,-0.5f, 0) },
			{ float3( 0.5f, 0.5f, 0) },
			{ float3(-0.5f, 0.5f, 0) },
		};

		const u16 IData[] = { 0, 1, 2, 0, 2, 3 };

		Patch.VBSize = sizeof(VData);
		Patch.IBSize = sizeof(IData);
		Patch.IndicesNum = _countof(IData);

		Patch.VertexBuffer = GetBuffersAllocator()->CreateSimpleBuffer(sizeof(VData), 4, L"Patch VB");
		Patch.IndexBuffer = GetBuffersAllocator()->CreateSimpleBuffer(sizeof(IData), 4, L"Patch IB");
		
		FGPUContext Context;
		Context.Open(EContextType::DIRECT);
		Context.CopyToBuffer(Patch.VertexBuffer, VData, sizeof(VData));
		Context.CopyToBuffer(Patch.IndexBuffer, IData, sizeof(IData));
		Context.Barrier(Patch.VertexBuffer, ALL_SUBRESOURCES, EAccessType::COPY_DEST, EAccessType::READ_VB_CB);
		Context.Barrier(Patch.IndexBuffer, ALL_SUBRESOURCES, EAccessType::COPY_DEST, EAccessType::READ_IB);
		Context.ExecuteImmediately();

		bInit = false;
	}

	static FPatchShaderState ShaderState;
	static FPipelineState * PipelineState;
	static FCommandsStream Stream;
	Stream.Reset();

	DXGI_FORMAT OutFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	if (!PipelineState) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineDesc = GetDefaultPipelineStateDesc();
		PipelineDesc.RTVFormats[0] = OutFormat;
		PipelineDesc.NumRenderTargets = 1;
		PipelineDesc.DepthStencilState.DepthEnable = false;
		PipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		PipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;

		PipelineState = GetGraphicsPipelineState(&ShaderState,
			&PipelineDesc,
			GetInputLayout({
				CreateInputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT)
			})
		);
	}

	FBufferLocation VB;
	VB.Address = Patch.VertexBuffer->GetGPUAddress();
	VB.Size = Patch.VBSize;
	VB.Stride = sizeof(FPatchVertex);
	FBufferLocation IB;
	IB.Address = Patch.IndexBuffer->GetGPUAddress();
	IB.Size = Patch.IBSize;
	IB.Stride = sizeof(u16);

	using namespace DirectX;

	Vec2i Resolution = Vec2i(GApplication::WndWidth, GApplication::WndHeight);

	auto ProjectionMatrix = XMMatrixPerspectiveFovLH(
		3.14f * 0.25f,
		(float)Resolution.x / (float)Resolution.y,
		0.01f, 1000.f);

	auto ViewMatrix = XMMatrixLookToLH(
		ToSIMD(Camera.Position),
		ToSIMD(Camera.Direction),
		ToSIMD(Camera.Up));

	XMVECTOR Determinant;
	auto InvViewMatrixT = XMMatrixTranspose(XMMatrixInverse(&Determinant, ViewMatrix));
	auto ViewProjMatrixT = XMMatrixTranspose(ViewMatrix * ProjectionMatrix);

	Stream.SetAccess(GetBackbuffer(), 0, EAccessType::WRITE_RT);
	Stream.ClearRTV(GetBackbuffer()->GetRTV(), 0.f);
	
	Stream.SetVB(VB, 0);
	Stream.SetIB(IB);
	Stream.SetPipelineState(PipelineState);
	Stream.SetConstantBuffer(&ShaderState.ConstantBuffer, CreateCBVFromData(&ShaderState.ConstantBuffer, ViewProjMatrixT));
	Stream.SetRenderTarget(0, GetBackbuffer()->GetRTV(OutFormat));
	Stream.SetViewport(GetBackbuffer()->GetSizeAsViewport());
	Stream.DrawIndexed(Patch.IndicesNum);

	Stream.Close();

	FGPUContext Context;
	Context.Open(EContextType::DIRECT);
	Playback(Context, &Stream);

	FRenderViewport Viewport;
	Viewport.Camera = &Camera;
	Viewport.DepthBuffer = nullptr;
	Viewport.OutputSRGB = true;
	Viewport.RenderTarget = GetBackbuffer();
	Viewport.Resolution = Resolution;
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.TViewProjectionMatrix, ViewProjMatrixT);

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

	DebugAcc.FlushToViewport(Context, Viewport);

	Context.Execute();

	return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	FApplicationImpl SampleApp(L"Essence2", 1024, 768);
	return Win32::Run(&SampleApp, hInstance, nCmdShow);
}
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

#include "BasePass.h"
#include "DepthPass.h"
#include "Viewport.h"

FScene Scene;
FGPUResourceRef DepthBuffer;
FGPUResourceRef Shadowmap;
FGPUResourceRef ShadowmapM2;
FGPUResourceRef PingPong;

FGPUResourceRef Texture;

struct FShadowRenderingParams {
	u32	Resolution = 1024;
	float2 LightAngles = float2(0, 0);
	bool ShowTextures = false;
	int ShowMipmap = 0;
	bool Blur = false;

	float3 LightDirection;
} ShadowRenderingParams;

void AllocateShadowmap() {
	Shadowmap = GetTexturesAllocator()->CreateTexture(ShadowRenderingParams.Resolution, ShadowRenderingParams.Resolution, 1, DXGI_FORMAT_R32_TYPELESS, TextureFlags::ALLOW_DEPTH_STENCIL | TextureFlags::TEXTURE_MIPMAPPED, L"Shadowmap");
	ShadowmapM2 = GetTexturesAllocator()->CreateTexture(ShadowRenderingParams.Resolution, ShadowRenderingParams.Resolution, 1, DXGI_FORMAT_R32_FLOAT, TextureFlags::ALLOW_RENDER_TARGET | TextureFlags::TEXTURE_MIPMAPPED, L"ShadowmapM2");
	PingPong = GetTexturesAllocator()->CreateTexture(ShadowRenderingParams.Resolution, ShadowRenderingParams.Resolution, 1, DXGI_FORMAT_R32_FLOAT, TextureFlags::ALLOW_RENDER_TARGET | TextureFlags::TEXTURE_MIPMAPPED, L"PingPong");
}

void ShowShadowmapOptions() {
	ImGui::Begin("Shadowmap");
	
	const char* items[] = { "512", "1024", "2048", "4096" };
	const u32 values[] = { 512, 1024, 2048, 4096 };
	static int item = 1;
	ImGui::Combo("Resolution", &item, items, _ARRAYSIZE(items)); 

	ImGui::SliderAngle("Azimuthal angle", &ShadowRenderingParams.LightAngles.x, 0.f, 360.f);
	ImGui::SliderAngle("Polar angle", &ShadowRenderingParams.LightAngles.y, 0.f, 180.f);

	ImGui::Checkbox("Visualize", &ShadowRenderingParams.ShowTextures);
	ImGui::SliderInt("Mipmap", &ShadowRenderingParams.ShowMipmap, 0, 5);
	ImGui::Checkbox("Blur", &ShadowRenderingParams.Blur);

	ImGui::End();

	ShadowRenderingParams.LightDirection = float3(
		sinf(ShadowRenderingParams.LightAngles.y) * cosf(ShadowRenderingParams.LightAngles.x),
		cosf(ShadowRenderingParams.LightAngles.y),
		sinf(ShadowRenderingParams.LightAngles.y) * sinf(ShadowRenderingParams.LightAngles.x)
		);

	ShadowRenderingParams.Resolution = values[item];

	if (Shadowmap->GetDimensions().x != ShadowRenderingParams.Resolution) {
		AllocateShadowmap();
	}
}

void FApplicationImpl::Init() {
	Camera.Position = float3(0, 0.f, -10.f);
	Camera.Up = float3(0, 1.f, 0);
	Camera.Direction = normalize(float3(0) - Camera.Position);

	Scene.AddStaticMesh(L"Tree", GetModel(L"Models/mitsuba.obj"));

	FGPUContext Context;
	Context.Open(EContextType::DIRECT);
	Scene.Prepare(Context);
	Texture = LoadDDS(L"textures/uvchecker.DDS", true, Context);
	Context.Close();
	Context.ExecuteImmediately();

	AllocateShadowmap();
}

void FApplicationImpl::AllocateScreenResources() {
	DepthBuffer = GetTexturesAllocator()->CreateTexture(GApplication::WindowWidth, GApplication::WindowHeight, 1, DXGI_FORMAT_R24G8_TYPELESS, TextureFlags::ALLOW_DEPTH_STENCIL, L"DepthBuffer");
}

void FApplicationImpl::Shutdown() {

}

#include "RenderingUtils.h"
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

	FDepthRenderContext ShadowmapContext;
	ShadowmapContext.OutputVSM = true;
	ShadowmapContext.RenderTargets.DepthBuffer = Shadowmap;
	ShadowmapContext.RenderTargets.Outputs.resize(1);
	ShadowmapContext.RenderTargets.Outputs[0].OutputSRGB = false;
	ShadowmapContext.RenderTargets.Outputs[0].Resource = ShadowmapM2;
	ShadowmapContext.RenderTargets.Viewport = Shadowmap->GetSizeAsViewport();
	UpdateShadowmapViewport(ShadowmapContext, Vec2i(Shadowmap->GetDimensions().x, Shadowmap->GetDimensions().y), ShadowRenderingParams.LightDirection * -1.f);

	Render_Depth(Stream, &ShadowmapContext, &Scene);

	if(ShadowRenderingParams.Blur) {
		BlurTexture(Stream, Shadowmap, PingPong);
		Stream.SetAccess(Shadowmap, EAccessType::COPY_DEST, 0);
		Stream.SetAccess(PingPong, EAccessType::COPY_SRC, 0);
		Stream.BatchBarriers();
		Stream.CopyTextureRegion(Shadowmap, 0, PingPong, 0);

		BlurTexture(Stream, ShadowmapM2, PingPong);
		Stream.SetAccess(ShadowmapM2, EAccessType::COPY_DEST, 0);
		Stream.SetAccess(PingPong, EAccessType::COPY_SRC, 0);
		Stream.BatchBarriers();
		Stream.CopyTextureRegion(ShadowmapM2, 0, PingPong, 0);
	}

	GenerateMipmaps(Stream, Shadowmap);
	GenerateMipmaps(Stream, ShadowmapM2);

	FForwardRenderContext SceneContext;
	UpdateViewport(SceneContext, &Camera, Vec2i(GApplication::WindowWidth, GApplication::WindowHeight));
	SceneContext.RenderTargets.Outputs.resize(1);
	SceneContext.RenderTargets.Outputs[0].Resource = GetBackbuffer();
	SceneContext.RenderTargets.Outputs[0].OutputSRGB = 1;
	SceneContext.RenderTargets.DepthBuffer = DepthBuffer;
	SceneContext.Camera = &Camera;
	SceneContext.WorldToShadowmap = ShadowmapContext.ViewProjectionMatrix;
	SceneContext.Shadowmap = Shadowmap;
	SceneContext.ShadowmapM2 = ShadowmapM2;
	SceneContext.RenderTargets.Viewport = DepthBuffer->GetSizeAsViewport();
	Render_Forward(Stream, &SceneContext, &Scene);

	FRenderTargetContext RenderTargets = SceneContext.RenderTargets;

	if(ShadowRenderingParams.ShowTextures) {
		RenderTargets.DepthBuffer = nullptr;
		DrawTexture(Stream, Shadowmap, 0.f, float2(512, 512), ETextureFiltering::Point, ShadowRenderingParams.ShowMipmap, RenderTargets);
		DrawTexture(Stream, ShadowmapM2, float2(0, 512), float2(512, 512), ETextureFiltering::Point, ShadowRenderingParams.ShowMipmap, RenderTargets);
	}

	RenderTargets.DepthBuffer = DepthBuffer;

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

	DebugAcc.FlushToViewport(Context, RenderTargets, &SceneContext.ViewProjectionMatrix);

	Context.Execute();

	return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	FApplicationImpl SampleApp(L"Essence2", 1024, 768);
	return Win32::Run(&SampleApp, hInstance, nCmdShow);
}
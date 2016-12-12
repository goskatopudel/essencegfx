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


void FApplicationImpl::Init() {
	Camera.Position = float3(0, 0.f, -10.f);
	Camera.Up = float3(0, 1.f, 0);
	Camera.Direction = normalize(float3(0) - Camera.Position);

}

void FApplicationImpl::AllocateScreenResources() {

}

void FApplicationImpl::Shutdown() {

}

#include "RenderMaterial.h"
#include "RenderModel.h"

#include "RenderGraph.h"
#include "RenderNodes.h"

#include "Scene.h"

FScene Scene;
FSceneActorRef Actor;
FSceneRenderContext SceneRenderContext;

void InitScene() {
	//Actor = Scene.SpawnActor(GetModel(L"sibenik.obj", L"models/sibenik/", L"models/sibenik/"), float3(0, 0, 0));
	Actor = Scene.SpawnActor(GetModel(L"tree.obj", L"models/", L"models/"), float3(0, 0, 0));
}

FGPUResourceRef RenderSceneToTexture(FCommandsStream & CmdStream, FSceneRenderContext * SceneRenderContext);

FGPUResourceRef Texture;

void InitGraph() {
	FGPUContext Context;
	Context.Open(EContextType::DIRECT);
	Texture = LoadDdsTexture(L"Textures/checker.dds", true, Context);
	Context.ExecuteImmediately();

	InitScene();
}

void TestGraph() {
	static FCommandsStream CmdStream;
	CmdStream.Reset();

	FRenderGraphNodeRef FinalOutput = CreateDataNode(GetBackbuffer());

	// which passess will be executed and in what order
	SceneRenderContext.RenderPasses.clear();
	SceneRenderContext.RenderPasses.push_back(Scene.ForwardPassActors.SceneRenderPass.get());

	Scene.AdvanceToNextFrame();

	SceneRenderContext.SetupNextFrameRendering(
		&Scene, 
		Vec2u(GApplication::WindowHeight, GApplication::WindowWidth), 
		&Camera);

	auto Color = RenderSceneToTexture(CmdStream, &SceneRenderContext);

	auto CopyTexture = eastl::make_shared<FCopyTexture>();
	CopyTexture->LinkInput(0, CreateDataNode(Color));
	CopyTexture->LinkOutput(0, FinalOutput);
	CopyTexture->OutputSrgb = true;

	ProcessGraph(CmdStream, FinalOutput.get());

	FGPUContext Context;
	Context.Open(EContextType::DIRECT);

	CmdStream.Close();
	Playback(Context, &CmdStream);
	Context.Execute();
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

	ImGui::ShowTestWindow();

	using namespace DirectX;
	
	static bool Init;
	if (!Init) {
		Init = true;

		InitGraph();
	}

	TestGraph();

	return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	FApplicationImpl SampleApp(L"Essence2", 1024, 768);
	return Win32::Run(&SampleApp, hInstance, nCmdShow);
}

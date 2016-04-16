#include "Application.h"
#include "Device.h"
#include "Resource.h"
#include "ImGui\imgui.h"
#include "Win32Application.h"
#include "SwapChain.h"
#include "Win32Application.h"
#include <DirectXMath.h>
#include "UIUtils.h"
#include "MathFunctions.h"
#include "CommandStream.h"

#include "Shader.h"
#include "Pipeline.h"
#include "Model.h"
#include "Camera.h"

#include "VideoMemory.h"

FOwnedResource RenderTargetRed;
FOwnedResource RenderTarget;
FOwnedResource UATarget;
FOwnedResource UITexture;
FOwnedResource DepthBuffer;
FOwnedResource MipmappedRT;

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

	float dx = (float)io.MouseDelta.x / (float)1024.f;
	float dy = (float)io.MouseDelta.y / (float)768.f;
	if (io.MouseDown[1]) {
		Camera.Rotate(dy, dx);
	}
}

bool ProcessWinMessage(Win32::Message const& Message) {
	ImGuiIO& io = ImGui::GetIO();

	switch (Message.message) {
	case WM_LBUTTONDOWN:
		io.MouseDown[0] = true;
		return true;
	case WM_LBUTTONUP:
		io.MouseDown[0] = false;
		return true;
	case WM_RBUTTONDOWN:
		io.MouseDown[1] = true;
		return true;
	case WM_RBUTTONUP:
		io.MouseDown[1] = false;
		return true;
	case WM_MBUTTONDOWN:
		io.MouseDown[2] = true;
		return true;
	case WM_MBUTTONUP:
		io.MouseDown[2] = false;
		return true;
	case WM_MOUSEWHEEL:
		io.MouseWheel += GET_WHEEL_DELTA_WPARAM(Message.wParam) > 0 ? +1.0f : -1.0f;
		return true;
	case WM_MOUSEMOVE:
		io.MousePos.x = (signed short)(Message.lParam);
		io.MousePos.y = (signed short)(Message.lParam >> 16);
		return true;
	case WM_KEYDOWN:
		if (Message.wParam < 256)
			io.KeysDown[Message.wParam] = 1;
		return true;
	case WM_KEYUP:
		if (Message.wParam < 256)
			io.KeysDown[Message.wParam] = 0;
		return true;
	case WM_CHAR:
		// You can also use ToAscii()+GetKeyboardState() to retrieve characters.
		if (Message.wParam > 0 && Message.wParam < 0x10000)
			io.AddInputCharacter((unsigned short)Message.wParam);
		return true;
	}

	return false;
}

class FCopyShaderState : public FShaderState {
public:
	FTextureParam			SourceTexture;

	FCopyShaderState() :
		FShaderState(
			GetShader("Shaders/Utility.hlsl", "VShader", "vs_5_0", {}, 0),
			GetShader("Shaders/Utility.hlsl", "CopyPS", "ps_5_0", {}, 0)) {}

	void InitParams() override final {
		SourceTexture = Root->CreateTextureParam("Image");
	}
};

class FUIShaderState : public FShaderState {
public:
	FTextureParam			AtlasTexture;
	FConstantBuffer			ConstantBuffer;

	struct FConstantBufferData {
		float4x4			ProjectionMatrix;
	};

	FUIShaderState() : 
		FShaderState(
			GetShader("Shaders/Ui.hlsl", "VShader", "vs_5_0", {}, 0), 
			GetShader("Shaders/Ui.hlsl", "PShader", "ps_5_0", {}, 0)) {}

	void InitParams() override final {
		AtlasTexture = Root->CreateTextureParam("Image");
		ConstantBuffer = Root->CreateConstantBuffer("Constants");
	}
};

void RenderImDrawLists(ImDrawData *draw_data) {
	static FUIShaderState UIShaderState;
	static FPipelineState * UIPipelineState;
	static FCommandsStream Stream;

	if (!UIPipelineState) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineDesc = GetDefaultPipelineStateDesc();
		PipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		PipelineDesc.NumRenderTargets = 1;
		PipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		PipelineDesc.RasterizerState.DepthClipEnable = true;
		PipelineDesc.BlendState.RenderTarget[0].BlendEnable = true;
		PipelineDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		PipelineDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		PipelineDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		PipelineDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		PipelineDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		PipelineDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		PipelineDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		UIPipelineState = GetGraphicsPipelineState(&UIShaderState,
			&PipelineDesc,
			GetInputLayout({
				CreateInputElement("POSITION", DXGI_FORMAT_R32G32_FLOAT),
				CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT),
				CreateInputElement("COLOR", DXGI_FORMAT_R8G8B8A8_UNORM)
			})
			);
	}

	Stream.Reset();

	u32 vtxBytesize = 0;
	u32 idxBytesize = 0;
	for (int n = 0; n < draw_data->CmdListsCount; n++) {
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		vtxBytesize += cmd_list->VtxBuffer.size() * sizeof(ImDrawVert);
		idxBytesize += cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx);
	}

	FOwnedResource VertexBuffer = GetUploadAllocator()->CreateBuffer(vtxBytesize, 8);
	FOwnedResource IndexBuffer = GetUploadAllocator()->CreateBuffer(idxBytesize, 8);
	auto vtxDst = (ImDrawVert*)VertexBuffer->GetMappedPtr();
	auto idxDst = (ImDrawIdx*)IndexBuffer->GetMappedPtr();

	for (int n = 0; n < draw_data->CmdListsCount; n++) {
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		memcpy(vtxDst, &cmd_list->VtxBuffer[0], cmd_list->VtxBuffer.size() * sizeof(ImDrawVert));
		memcpy(idxDst, &cmd_list->IdxBuffer[0], cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx));
		vtxDst += cmd_list->VtxBuffer.size();
		idxDst += cmd_list->IdxBuffer.size();
	}

	FBufferLocation VB;
	VB.Address = VertexBuffer->GetGPUAddress();
	VB.Size = vtxBytesize;
	VB.Stride = sizeof(ImDrawVert);
	Stream.SetVB(VB, 0);

	FBufferLocation IB;
	IB.Address = IndexBuffer->GetGPUAddress();
	IB.Size = idxBytesize;
	IB.Stride = sizeof(u16);
	Stream.SetIB(IB);

	using namespace DirectX;

	auto matrix = XMMatrixTranspose(
		XMMatrixOrthographicOffCenterLH(
			0, (float)1024, (float)768, 0, 0, 1));

	Stream.SetAccess(GetBackbuffer(), 0, EAccessType::WRITE_RT);

	Stream.SetPipelineState(UIPipelineState);
	Stream.SetConstantBuffer(&UIShaderState.ConstantBuffer, CreateCBVFromData(&UIShaderState.ConstantBuffer, matrix));
	Stream.SetRenderTarget(0, GetBackbuffer()->GetRTV(DXGI_FORMAT_R8G8B8A8_UNORM));
	Stream.SetViewport(GetBackbuffer()->GetSizeAsViewport());

	u32 vtxOffset = 0;
	i32 idxOffset = 0;

	for (int n = 0; n < draw_data->CmdListsCount; n++) {
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++) {
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

			if (pcmd->UserCallback) {
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else {
				D3D12_RECT scissor = { (LONG)pcmd->ClipRect.x, (LONG)pcmd->ClipRect.y, (LONG)pcmd->ClipRect.z, (LONG)pcmd->ClipRect.w };

				if (pcmd->TextureId) {
					Stream.SetTexture(&UIShaderState.AtlasTexture, ((FGPUResource*)pcmd->TextureId)->GetSRV());
				}
				else {
					Stream.SetTexture(&UIShaderState.AtlasTexture, NULL_TEXTURE2D_VIEW);
				}

				Stream.SetScissorRect(scissor);
				Stream.DrawIndexed(pcmd->ElemCount, idxOffset, vtxOffset);
			}
			idxOffset += pcmd->ElemCount;
		}
		vtxOffset += cmd_list->VtxBuffer.size();
	}

	FGPUContext Context;
	Context.Open(EContextType::DIRECT);
	Playback(Context, &Stream);
	Context.Execute();
}


FOwnedResource ColorTexture;
//
//struct GPUSceneRender {
//	FPipelineState*			PSO;
//	FGraphicsRootLayout*	Root;
//
//	FConstantBuffer			FrameConstantsBuffer;
//	/*FConstantParam			ViewProjectionMatrixParam;
//	FConstantParam			InvViewMatrixParam;
//	FConstantParam			LightFromDirection;
//*/
//	FConstantBuffer			ObjectConstantsBuffer;
//	/*FConstantParam			WorldMatrixParam;
//
//	FConstantBufferParam	FrameConstants;
//	FConstantBufferParam	ObjectConstants;
//*/
//	FTextureParam			BaseColorTextureParam;
//	FTextureParam			MetallicTextureParam;
//	FTextureParam			NormalmapTextureParam;
//	FTextureParam			RoughnessTextureParam;
//
//};
//
//GPUSceneRender SceneRender;
//
//void RenderScene(FGPUContext & Context, FModel * model) {
//	static bool initialized = false;
//	if (!initialized) {
//		initialized = true;
//
//		D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineDesc = GetDefaultPipelineStateDesc();
//		PipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
//		PipelineDesc.NumRenderTargets = 1;
//		PipelineDesc.DSVFormat = DepthBuffer->FatData->Desc.Format;
//		PipelineDesc.DepthStencilState.DepthEnable = true;
//
//		auto VS = GetShader("Shaders/Model.hlsl", "VShader", "vs_5_0", {}, 0);
//		auto PS = GetShader("Shaders/Model.hlsl", "PShader", "ps_5_0", {}, 0);
//		SceneRender.Root = GetRootLayout(VS, PS);
//		/*SceneRender.PSO = GetGraphicsPipelineState(&PipelineDesc, GetRootSignature(SceneRender.Root), VS, PS,
//			GetInputLayout({
//				CreateInputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
//				CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 0, 0),
//				CreateInputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, 0, 1),
//			})
//		);
//*/
//		/*SceneRender.Root->CreateConstantBuffer("FrameConstants", SceneRender.FrameConstantsBuffer);
//
//		SceneRender.FrameConstantsBuffer.CreateConstantParam("ViewProj", SceneRender.ViewProjectionMatrixParam);
//		SceneRender.FrameConstantsBuffer.CreateConstantParam("InvView", SceneRender.InvViewMatrixParam);
//		SceneRender.FrameConstantsBuffer.CreateConstantParam("LightFromDirection", SceneRender.LightFromDirection);
//		SceneRender.FrameConstantsBuffer.CreateConstantBufferVersion(SceneRender.FrameConstants);
//
//		SceneRender.Root->CreateConstantBuffer("ObjectConstants", SceneRender.ObjectConstantsBuffer);
//		
//		SceneRender.ObjectConstantsBuffer.CreateConstantParam("World", SceneRender.WorldMatrixParam);
//		SceneRender.ObjectConstantsBuffer.CreateConstantBufferVersion(SceneRender.ObjectConstants);
//
//		SceneRender.Root->CreateTextureParam("BaseColorTexture", SceneRender.BaseColorTextureParam);
//		SceneRender.Root->CreateTextureParam("MetallicTexture", SceneRender.MetallicTextureParam);
//		SceneRender.Root->CreateTextureParam("NormalmapTexture", SceneRender.NormalmapTextureParam);
//		SceneRender.Root->CreateTextureParam("RoughnessTexture", SceneRender.RoughnessTextureParam);*/
//	}
//
//	Context.SetIB(GetIB());
//	Context.SetVB(GetVB(0), 0);
//	Context.SetVB(GetVB(1), 1);
//
//	using namespace DirectX;
//
//	auto ProjectionMatrix = XMMatrixPerspectiveFovLH(
//		3.14f * 0.25f, 
//		(float)1024.f / 768.f, 
//		0.01f, 1000.f);
//
//	auto ViewMatrix = XMMatrixLookToLH(
//		ToSIMD(Camera.Position),
//		ToSIMD(Camera.Direction),
//		ToSIMD(Camera.Up));
//
//	auto WorldTMatrix = XMMatrixTranspose(
//		XMMatrixScaling(0.05f, 0.05f, 0.05f));
//
//	XMVECTOR Determinant;
//	auto InvViewTMatrix = XMMatrixTranspose(XMMatrixInverse(&Determinant, ViewMatrix));
//	auto ViewProjTMatrix = XMMatrixTranspose(ViewMatrix * ProjectionMatrix);
//
//	/*SceneRender.FrameConstants.Set(&SceneRender.ViewProjectionMatrixParam, ViewProjTMatrix);
//	SceneRender.FrameConstants.Set(&SceneRender.InvViewMatrixParam, InvViewTMatrix);
//	SceneRender.FrameConstants.Serialize();
//	SceneRender.ObjectConstants.Set(&SceneRender.WorldMatrixParam, WorldTMatrix);
//	SceneRender.ObjectConstants.Serialize();
//
//	Context.SetPSO(SceneRender.PSO);
//	Context.SetRoot(SceneRender.Root);
//	Context.SetConstantBuffer(&SceneRender.FrameConstants);
//	Context.SetConstantBuffer(&SceneRender.ObjectConstants);*/
//	Context.SetRenderTarget(0, GetBackbuffer()->GetRTV());
//	Context.SetDepthStencil(DepthBuffer->GetDSV());
//	Context.SetViewport(GetBackbuffer()->GetSizeAsViewport());
//
//	u64 MeshesNum = model->Meshes.size();
//	for (u64 MeshIndex = 0; MeshIndex < MeshesNum; MeshIndex++) {
//		auto const & mesh = model->Meshes[MeshIndex];
//		if (model->FatData->FatMeshes[MeshIndex].Material) {
//			Context.SetTexture(&SceneRender.BaseColorTextureParam, model->FatData->FatMeshes[MeshIndex].Material->FatData->BaseColorTexture->GetSRV());
//			if (model->FatData->FatMeshes[MeshIndex].Material->FatData->MetallicTexture.IsValid()) {
//				Context.SetTexture(&SceneRender.MetallicTextureParam, model->FatData->FatMeshes[MeshIndex].Material->FatData->MetallicTexture->GetSRV());
//			}
//			else {
//				Context.SetTexture(&SceneRender.MetallicTextureParam, ColorTexture->GetSRV());
//			}
//			if (model->FatData->FatMeshes[MeshIndex].Material->FatData->NormalMapTexture.IsValid()) {
//				Context.SetTexture(&SceneRender.NormalmapTextureParam, model->FatData->FatMeshes[MeshIndex].Material->FatData->NormalMapTexture->GetSRV());
//			}
//			else {
//				Context.SetTexture(&SceneRender.NormalmapTextureParam, ColorTexture->GetSRV());
//			}
//			if (model->FatData->FatMeshes[MeshIndex].Material->FatData->RoughnessTexture.IsValid()) {
//				Context.SetTexture(&SceneRender.RoughnessTextureParam, model->FatData->FatMeshes[MeshIndex].Material->FatData->RoughnessTexture->GetSRV());
//			}
//			else {
//				Context.SetTexture(&SceneRender.RoughnessTextureParam, ColorTexture->GetSRV());
//			}
//		}
//		else {
//			Context.SetTexture(&SceneRender.BaseColorTextureParam, ColorTexture->GetSRV());
//			Context.SetTexture(&SceneRender.MetallicTextureParam, ColorTexture->GetSRV());
//			Context.SetTexture(&SceneRender.NormalmapTextureParam, ColorTexture->GetSRV());
//			Context.SetTexture(&SceneRender.RoughnessTextureParam, ColorTexture->GetSRV());
//		}
//
//		Context.DrawIndexed(mesh.IndexCount, mesh.StartIndex, mesh.BaseVertex);
//	}
//}

FModel* DrawModel;

void FApplication::Init() {
	ListAdapters();
	InitDevices(0, true);

	Win32::SetCustomMessageFunc(ProcessWinMessage);

	QueryPerformanceFrequency((LARGE_INTEGER *)&CpuFrequency);

	ImGuiIO& io = ImGui::GetIO();
	io.KeyMap[ImGuiKey_Tab] = VK_TAB;                     
	io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
	io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
	io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
	io.KeyMap[ImGuiKey_Home] = VK_HOME;
	io.KeyMap[ImGuiKey_End] = VK_END;
	io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
	io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
	io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
	io.KeyMap[ImGuiKey_A] = 'A';
	io.KeyMap[ImGuiKey_C] = 'C';
	io.KeyMap[ImGuiKey_V] = 'V';
	io.KeyMap[ImGuiKey_X] = 'X';
	io.KeyMap[ImGuiKey_Y] = 'Y';
	io.KeyMap[ImGuiKey_Z] = 'Z';

	io.RenderDrawListsFn = RenderImDrawLists;
	io.ImeWindowHandle = Win32::GetWndHandle();

	Camera.Position = float3(0, 5.f, -50.f);
	Camera.Up = float3(0, 1.f, 0);
	Camera.Direction = normalize(float3(0) - Camera.Position);

	RenderTargetRed = GetTexturesAllocator()->CreateTexture(1024, 768, 1, DXGI_FORMAT_R8G8B8A8_UNORM, ALLOW_RENDER_TARGET, L"A", DXGI_FORMAT_R8G8B8A8_UNORM, float4(1,0,0,0));
	RenderTarget = GetTexturesAllocator()->CreateTexture(1024, 768, 1, DXGI_FORMAT_R8G8B8A8_UNORM, ALLOW_RENDER_TARGET, L"A", DXGI_FORMAT_R8G8B8A8_UNORM);
	UATarget = GetTexturesAllocator()->CreateTexture(512, 512, 1, DXGI_FORMAT_R8G8B8A8_UNORM, ALLOW_UNORDERED_ACCESS, L"B");
	DepthBuffer = GetTexturesAllocator()->CreateTexture(1024, 768, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, ALLOW_DEPTH_STENCIL, L"DepthStencil", DXGI_FORMAT_D24_UNORM_S8_UINT);
	
	io.Fonts->AddFontDefault();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	io.Fonts->TexID = UITexture = GetTexturesAllocator()->CreateTexture(width, height, 1, DXGI_FORMAT_R8G8B8A8_UNORM, TEXTURE_NO_FLAGS, L"ImGui default font");

	//DrawModel = LoadModelFromOBJ(L"Models/cube.obj");
	//ConvertObjToBinary(L"Models/sponza.obj", L"Models/sponza.bin");
	DrawModel = LoadModelFromBinary(L"Models/sponza.bin");

	FGPUContext Context;
	Context.Open(EContextType::DIRECT);
	Context.CopyDataToSubresource(UITexture, 0, pixels, sizeof(u32) * width, sizeof(u32) * width * height);

	Context.Barrier(UITexture, ALL_SUBRESOURCES, EAccessType::COPY_DEST, EAccessType::READ_PIXEL);

	LoadModelTextures(Context, DrawModel);
	UpdateGeometryBuffers(Context);

	ColorTexture = LoadDDSImage(L"Textures/uvchecker.DDS", true, Context);

	Context.ExecuteImmediately();
}

extern u32		GIgnoreRelease;

void FApplication::Shutdown() {
	auto finalSyncPoint = GetDirectQueue()->GenerateSyncPoint();
	GetDirectQueue()->WaitForCompletion();
	EndFrame();
	ImGui::Shutdown();
	GIgnoreRelease = true;
}

void ShowSceneWindow() {
	ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	if (ImGui::CollapsingHeader("Directional Light")) {
		static float AzimuthAngle;
		static float HorizontalAngle;
		static float3 Color = 1.f;
		static float Intensity;
		ImGui::SliderFloat("Azimuth angle", &AzimuthAngle, 0, DirectX::XM_2PI);
		ImGui::SliderFloat("Horizontal angle", &HorizontalAngle, 0, DirectX::XM_PI);
		float3 Direction = float3(cosf(AzimuthAngle) * sinf(HorizontalAngle), cosf(HorizontalAngle), sinf(AzimuthAngle) * sinf(HorizontalAngle));
		ImGui::ColorEdit3("Color", &Color.x);
		ImGui::SliderFloat("Intensity", &Intensity, 0.f, 1000000.f, "%.3f", 10.f);
	}

	ImGui::End();
}

void ShowAppStats() {
	ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	if (ImGui::CollapsingHeader("Shaders")) {
		extern u64 GShadersCompilationVersion;

		ImGui::Text("Shaders:\nPSOs:\nCurrent shaders version:"); ImGui::SameLine();
		ImGui::Text("%u\n%u\n%u", GetShadersNum(), GetPSOsNum(), (u32)GShadersCompilationVersion);
	}
	if (ImGui::CollapsingHeader("Memory")) {
		ShowMemoryInfo();
	}
	ImGui::End();
}

bool FApplication::Update() {
	ImGuiIO& io = ImGui::GetIO();

	// Setup display size (every frame to accommodate for window resizing)
	RECT rect;
	GetClientRect(Win32::GetWndHandle(), &rect);
	io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

	i64 CurrentTime;
	QueryPerformanceCounter((LARGE_INTEGER *)&CurrentTime);
	io.DeltaTime = (float)(CurrentTime - Time) / CpuFrequency;
	Time = CurrentTime;

	// Read keyboard modifiers inputs
	io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
	io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;

	SetCursor(io.MouseDrawCursor ? NULL : LoadCursor(NULL, IDC_ARROW));
	UpdateCamera();

	if (io.KeyCtrl && io.KeysDown['R'] && io.KeysDownDuration['R'] == 0.f) {
		GetDirectQueue()->WaitForCompletion();

		RecompileChangedShaders();
		RecompileChangedPipelines();
	}

	ImGui::NewFrame();

	ShowSceneWindow();
	ShowAppStats();

	ImGui::ShowTestWindow();

	static FCommandsStream Stream;
	Stream.Reset();
	Stream.SetAccess(GetBackbuffer(), EAccessType::WRITE_RT);
	Stream.ClearRTV(GetBackbuffer()->GetRTV(), 0.f);
	Stream.Close();

	FGPUContext Context;
	Context.Open(EContextType::DIRECT);
	Playback(Context, &Stream);
	Context.Execute();

	ImGui::Render();
	
	Context.Open(EContextType::DIRECT);
	Stream.Reset();
	Stream.SetAccess(GetBackbuffer(), EAccessType::COMMON);
	Stream.Close();
	Playback(Context, &Stream);
	Context.Execute();
	GetDirectQueue()->Flush();
	GetSwapChain()->Present();

	EndFrame();

	return true;
}
#include "Application.h"
#include "Device.h"
#include "Resource.h"
#include "ImGui\imgui.h"
#include "Win32Application.h"
#include "SwapChain.h"
#include "Win32Application.h"
#include <DirectXMath.h>
#include "UtilWidgets.h"
#include "MathFunctions.h"

#include "Shader.h"
#include "Pipeline.h"
#include "Model.h"
#include "Camera.h"

#include "VideoMemory.h"
#include "Barriers.h"

FOwnedResource RenderTargetRed;
FOwnedResource RenderTarget;
FOwnedResource UATarget;
FOwnedResource UITexture;
FOwnedResource DepthBuffer;

FRenderPass DepthPrePass;
FRenderPass MainPass;
FRenderPass FinalizePass;

void TestBarriers() {
	DepthPrePass.SetName(L"Prepass");
	DepthPrePass.SetAccess(DepthBuffer, EAccessType::WRITE_DEPTH, 0);
	MainPass.SetName(L"MainPass");
	MainPass.SetAccess(DepthBuffer, EAccessType::READ_DEPTH, 0);
	MainPass.SetAccess(RenderTarget, EAccessType::WRITE_RT);
	FinalizePass.SetName(L"Finalize");
	FinalizePass.SetAccess(RenderTarget, EAccessType::READ_PIXEL);

	FRenderPassSequence PassSequence = { &DepthPrePass, &MainPass, &FinalizePass };

}

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

struct GPUCopyTexture {
	FPipelineState*			PSO;
	FGraphicsRootLayout*	Root;

	FShaderParam			InputImage;
	FConstantBuffer			GlobalsBuffer;
	FConstantParam			WriteColor;

	FConstantBufferParam	FrameGlobals;

	void Init(D3D12_GRAPHICS_PIPELINE_STATE_DESC const& Desc) {
		auto VS = GetShader("Shaders/Test.hlsl", "VShader", "vs_5_1", {}, 0);
		auto PS = GetShader("Shaders/Test.hlsl", "TestPS", "ps_5_1", {}, 0);
		Root = GetRootLayout(VS, PS);
		PSO = GetGraphicsPipelineState(&Desc, GetRootSignature(Root), VS, PS, GetInputLayout({}));

		Root->CreateTextureParam("Image", InputImage);
		Root->CreateConstantBuffer("$Globals", GlobalsBuffer);

		GlobalsBuffer.CreateConstantParam("WriteColor", WriteColor);
		GlobalsBuffer.CreateConstantBufferVersion(FrameGlobals);
	}
};

GPUCopyTexture CopyTexture;

inline D3D12_INPUT_ELEMENT_DESC CreateInputElement(const char* SemanticName, DXGI_FORMAT Format, u32 SemanticIndex = 0, u32 InputSlot = 0) {
	D3D12_INPUT_ELEMENT_DESC Element;
	Element.SemanticName = SemanticName;
	Element.SemanticIndex = SemanticIndex;
	Element.Format = Format;
	Element.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	Element.InputSlot = InputSlot;
	Element.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	Element.InstanceDataStepRate = 0;
	return Element;
}

struct GPURenderUI {
	FPipelineState*			PSO;
	FGraphicsRootLayout*	Root;

	FShaderParam			InputImage;
	FConstantBuffer			GlobalsBuffer;
	FConstantParam			ProjectionMatrix;

	FConstantBufferParam	FrameGlobals;

	void Init(D3D12_GRAPHICS_PIPELINE_STATE_DESC const& Desc) {
		auto VS = GetShader("Shaders/Ui.hlsl", "VShader", "vs_5_0", {}, 0);
		auto PS = GetShader("Shaders/Ui.hlsl", "PShader", "ps_5_0", {}, 0);
		Root = GetRootLayout(VS, PS);
		PSO = GetGraphicsPipelineState(&Desc, GetRootSignature(Root), VS, PS, 
			GetInputLayout({ 
				CreateInputElement("POSITION", DXGI_FORMAT_R32G32_FLOAT),
				CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT),
				CreateInputElement("COLOR", DXGI_FORMAT_R8G8B8A8_UNORM),
			}));

		Root->CreateTextureParam("Image", InputImage);
		Root->CreateConstantBuffer("ConstantBuffer", GlobalsBuffer);

		GlobalsBuffer.CreateConstantParam("Projection", ProjectionMatrix);
		GlobalsBuffer.CreateConstantBufferVersion(FrameGlobals);
	}
};

GPURenderUI RenderUI;
FOwnedResource ColorTexture;

struct GPUSceneRender {
	FPipelineState*			PSO;
	FGraphicsRootLayout*	Root;

	FConstantBuffer			FrameConstantsBuffer;
	FConstantBuffer			ObjectConstantsBuffer;

	FConstantParam			ViewProjectionMatrixParam;
	FConstantParam			WorldMatrixParam;

	FConstantBufferParam	FrameConstants;
	FConstantBufferParam	ObjectConstants;

	FShaderParam			BaseColorTextureParam;
};

GPUSceneRender SceneRender;

void RenderScene(GPUGraphicsContext & Context, FModel * model) {
	static bool initialized = false;
	if (!initialized) {
		initialized = true;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineDesc = GetDefaultPipelineStateDesc();
		PipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		PipelineDesc.NumRenderTargets = 1;
		PipelineDesc.DSVFormat = DepthBuffer->FatData->Desc.Format;
		PipelineDesc.DepthStencilState.DepthEnable = true;

		auto VS = GetShader("Shaders/Model.hlsl", "VShader", "vs_5_0", {}, 0);
		auto PS = GetShader("Shaders/Model.hlsl", "PShader", "ps_5_0", {}, 0);
		SceneRender.Root = GetRootLayout(VS, PS);
		SceneRender.PSO = GetGraphicsPipelineState(&PipelineDesc, GetRootSignature(SceneRender.Root), VS, PS,
			GetInputLayout({
				CreateInputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
				CreateInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 0, 0),
				CreateInputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, 0, 1),
			})
		);

		SceneRender.Root->CreateConstantBuffer("FrameConstants", SceneRender.FrameConstantsBuffer);

		SceneRender.FrameConstantsBuffer.CreateConstantParam("ViewProj", SceneRender.ViewProjectionMatrixParam);
		SceneRender.FrameConstantsBuffer.CreateConstantBufferVersion(SceneRender.FrameConstants);

		SceneRender.Root->CreateConstantBuffer("ObjectConstants", SceneRender.ObjectConstantsBuffer);
		
		SceneRender.ObjectConstantsBuffer.CreateConstantParam("World", SceneRender.WorldMatrixParam);
		SceneRender.ObjectConstantsBuffer.CreateConstantBufferVersion(SceneRender.ObjectConstants);

		SceneRender.Root->CreateTextureParam("BaseColorTexture", SceneRender.BaseColorTextureParam);
	}

	Context.SetIB(GetIB());
	Context.SetVB(GetVB(0), 0);
	Context.SetVB(GetVB(1), 1);

	using namespace DirectX;

	auto ProjectionMatrix = XMMatrixPerspectiveFovLH(
		3.14f * 0.25f, 
		(float)1024.f / 768.f, 
		0.01f, 1000.f);

	auto ViewMatrix = XMMatrixLookToLH(
		ToSIMD(Camera.Position),
		ToSIMD(Camera.Direction),
		ToSIMD(Camera.Up));

	auto WorldTMatrix = XMMatrixTranspose(
		XMMatrixScaling(0.05f, 0.05f, 0.05f));

	auto ViewProjTMatrix = XMMatrixTranspose(ViewMatrix * ProjectionMatrix);

	SceneRender.FrameConstants.Set(&SceneRender.ViewProjectionMatrixParam, ViewProjTMatrix);
	SceneRender.FrameConstants.Serialize();
	SceneRender.ObjectConstants.Set(&SceneRender.WorldMatrixParam, WorldTMatrix);
	SceneRender.ObjectConstants.Serialize();

	Context.SetPSO(SceneRender.PSO);
	Context.SetRoot(SceneRender.Root);
	Context.SetConstantBuffer(&SceneRender.FrameConstants);
	Context.SetConstantBuffer(&SceneRender.ObjectConstants);
	Context.SetRenderTarget(0, GetBackbuffer()->GetRTV());
	Context.SetDepthStencil(DepthBuffer->GetDSV());
	Context.SetViewport(GetBackbuffer()->GetSizeAsViewport());

	u64 MeshesNum = model->Meshes.size();
	for (u64 MeshIndex = 0; MeshIndex < MeshesNum; MeshIndex++) {
		auto const & mesh = model->Meshes[MeshIndex];
		if (model->FatData->FatMeshes[MeshIndex].Material) {
			Context.SetTexture(&SceneRender.BaseColorTextureParam, model->FatData->FatMeshes[MeshIndex].Material->FatData->BaseColorTexture->GetSRV());
		}
		else {
			Context.SetTexture(&SceneRender.BaseColorTextureParam, ColorTexture->GetSRV());
		}

		Context.DrawIndexed(mesh.IndexCount, mesh.StartIndex, mesh.BaseVertex);
	}
}

void RenderImDrawLists(ImDrawData *draw_data) {

	static bool initialized = false;
	if (!initialized) {
		initialized = true;

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

		RenderUI.Init(PipelineDesc);
	}
	
	GPUGraphicsContext Context;
	Context.Open();

	u32 vtxBytesize = 0;
	u32 idxBytesize = 0;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		vtxBytesize += cmd_list->VtxBuffer.size() * sizeof(ImDrawVert);
		idxBytesize += cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx);
	}

	FOwnedResource VertexBuffer = GetUploadAllocator()->CreateBuffer(vtxBytesize, 8);
	FOwnedResource IndexBuffer = GetUploadAllocator()->CreateBuffer(idxBytesize, 8);
	auto vtxDst = (ImDrawVert*)VertexBuffer->GetMappedPtr();
	auto idxDst = (ImDrawIdx*)IndexBuffer->GetMappedPtr();

	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
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
	Context.SetVB(VB, 0);

	FBufferLocation IB;
	IB.Address = IndexBuffer->GetGPUAddress();
	IB.Size = idxBytesize;
	IB.Stride = sizeof(u16);
	Context.SetIB(IB);

	using namespace DirectX;

	auto matrix = XMMatrixTranspose(
		XMMatrixOrthographicOffCenterLH(
			0, (float)1024, (float)768, 0, 0, 1));

	RenderUI.FrameGlobals.Set(&RenderUI.ProjectionMatrix, matrix);
	RenderUI.FrameGlobals.Serialize();

	Context.SetPSO(RenderUI.PSO);
	Context.SetRoot(RenderUI.Root);
	Context.SetConstantBuffer(&RenderUI.FrameGlobals);
	Context.SetRenderTarget(0, GetBackbuffer()->GetRTV(DXGI_FORMAT_R8G8B8A8_UNORM));
	Context.SetViewport(GetBackbuffer()->GetSizeAsViewport());

	u32 vtxOffset = 0;
	i32 idxOffset = 0;

	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++) {
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

			if (pcmd->UserCallback) {
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else {
				D3D12_RECT scissor = { (LONG)pcmd->ClipRect.x, (LONG)pcmd->ClipRect.y, (LONG)pcmd->ClipRect.z, (LONG)pcmd->ClipRect.w };

				if (pcmd->TextureId) {
					Context.SetTexture(&CopyTexture.InputImage, ((FGPUResource*)pcmd->TextureId)->GetSRV());
				}
				else {
					Context.SetTexture(&CopyTexture.InputImage, NULL_TEXTURE2D_VIEW);
				}

				Context.SetScissorRect(scissor);
				Context.DrawIndexed(pcmd->ElemCount, idxOffset, vtxOffset);
			}
			idxOffset += pcmd->ElemCount;
		}
		vtxOffset += cmd_list->VtxBuffer.size();
	}

	Context.Execute();
}

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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineDesc = GetDefaultPipelineStateDesc();
	PipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	PipelineDesc.NumRenderTargets = 1;
	CopyTexture.Init(PipelineDesc);

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

	GPUGraphicsContext Context;
	Context.Open();
	{
		FBarrierScope copyScope(Context, UITexture, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		Context.CopyDataToSubresource(UITexture, 0, pixels, sizeof(u32) * width, sizeof(u32) * width * height);
	}

	LoadModelTextures(Context, DrawModel);
	UpdateGeometryBuffers(Context);

	ColorTexture = LoadDDSImage(L"Textures/uvchecker.DDS", true, Context);

	Context.Barrier(RenderTargetRed, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Context.Barrier(DepthBuffer, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	Context.ExecuteImmediately();

	TestBarriers();
}

extern u32		GIgnoreRelease;

void FApplication::Shutdown() {
	auto finalSyncPoint = GetDirectQueue()->GenerateSyncPoint();
	GetDirectQueue()->WaitForCompletion();
	EndFrame();
	ImGui::Shutdown();
	GIgnoreRelease = true;
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

	ImGui::NewFrame();

	ShowMemoryWidget();

	GPUGraphicsContext Context;
	Context.Open();

	float4 Val = 1.f;
	CopyTexture.FrameGlobals.Set(&CopyTexture.WriteColor, Val);
	CopyTexture.FrameGlobals.Serialize();

	{
		FBarrierScope scope(Context, GetBackbuffer(), 0, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Context.FlushBarriers();
		Context.ClearRTV(GetBackbuffer()->GetRTV(), 0.f);
		Context.ClearDSV(DepthBuffer->GetDSV(), 1.f, 0);

		GPUComputeContext ComputeContext;
		ComputeContext.Open();
		{
			FBarrierScope scope(ComputeContext, UATarget, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}
		ComputeContext.ExecuteImmediately();

		Context.ClearRTV(RenderTargetRed->GetRTV(), float4(1,0,0,0));

		{
			FBarrierScope scope(Context, RenderTargetRed, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			FBarrierScope scope1(Context, RenderTarget, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
			Context.ClearRTV(RenderTarget->GetRTV(), 0.f);

			Context.SetPSO(CopyTexture.PSO);
			Context.SetRoot(CopyTexture.Root);
			Context.SetConstantBuffer(&CopyTexture.FrameGlobals);
			Context.SetTexture(&CopyTexture.InputImage, UITexture->GetSRV());
			Context.SetRenderTarget(0, RenderTarget->GetRTV());
			Context.SetViewport(RenderTarget->GetSizeAsViewport());
			Context.Draw(3);
		}
		{
			FBarrierScope backbufferScope(Context, GetBackbuffer(), 0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
			Context.CopyResource(GetBackbuffer(), RenderTarget);
			Context.Barrier(RenderTarget, 0, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
		}
		RenderScene(Context, DrawModel);
		Context.Execute();
		ImGui::Render();
		Context.Open();
	}
	Context.Execute();

	GetDirectQueue()->Flush();
	GetSwapChain()->Present();

	EndFrame();

	return true;
}
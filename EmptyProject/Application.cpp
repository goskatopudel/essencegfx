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
#include "VideoMemory.h"

namespace GApplication {
bool			WindowSizeChanged;
u32				WindowWidth;
u32				WindowHeight;
const wchar_t*	WndTitle;
i64				Time;
i64				CpuFrequency;
}

FGPUResourceRef UITexture;

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

class FUIShaderState : public FShaderState {
public:
	FSRVParam			AtlasTexture;
	FCBVParam			ConstantBuffer;

	struct FConstantBufferData {
		float4x4			ProjectionMatrix;
	};

	FUIShaderState() : 
		FShaderState(
			GetGlobalShader("Shaders/Ui.hlsl", "VShader", "vs_5_0", {}, 0),
			GetGlobalShader("Shaders/Ui.hlsl", "PShader", "ps_5_0", {}, 0)) {}

	void InitParams() override final {
		AtlasTexture = Root->CreateSRVParam(this, "Image");
		ConstantBuffer = Root->CreateCBVParam(this, "Constants");
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

	FGPUResourceRef VertexBuffer = GetUploadAllocator()->CreateBuffer(vtxBytesize, 8);
	FGPUResourceRef IndexBuffer = GetUploadAllocator()->CreateBuffer(idxBytesize, 8);
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
			0, (float)GApplication::WindowWidth, (float)GApplication::WindowHeight, 0, 0, 1));

	Stream.SetAccess(GetBackbuffer(), EAccessType::WRITE_RT);

	Stream.SetPipelineState(UIPipelineState);
	Stream.SetConstantBuffer(&UIShaderState.ConstantBuffer, CreateCBVFromData(&UIShaderState.ConstantBuffer, matrix));
	Stream.SetRenderTarget(GetBackbuffer()->GetRTV(DXGI_FORMAT_R8G8B8A8_UNORM));
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

	Stream.Close();

	FGPUContext Context;
	Context.Open(EContextType::DIRECT);
	Playback(Context, &Stream);
	Context.Execute();
}

void FApplication::CoreInit()
{
	ListAdapters();
	InitDevices(0, EDebugMode::GpuValidation);

	Win32::SetCustomMessageFunc(ProcessWinMessage);

	QueryPerformanceFrequency((LARGE_INTEGER *)&GApplication::CpuFrequency);

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

	io.Fonts->AddFontDefault();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	io.Fonts->TexID = UITexture = GetTexturesAllocator()->CreateTexture(width, height, 1, DXGI_FORMAT_R8G8B8A8_UNORM, TEXTURE_NO_FLAGS, L"ImGui default font");

	FGPUContext Context;
	Context.Open(EContextType::DIRECT);
	Context.CopyDataToSubresource(UITexture, 0, pixels, sizeof(u32) * width, sizeof(u32) * width * height);
	Context.Barrier(UITexture, ALL_SUBRESOURCES, EAccessType::COPY_DEST, EAccessType::READ_PIXEL);
	Context.ExecuteImmediately();

	AllocateScreenResources();

	Init();
}

void FApplication::CoreShutdown() {
	Shutdown();

	auto finalFGPUSyncPoint = GetDirectQueue()->GenerateGPUSyncPoint();
	GetDirectQueue()->WaitForCompletion();
	EndFrame();
	ImGui::Shutdown();

	FreeAllocators();
	SetIgnoreRelease();
}

bool FApplication::CoreUpdate() {
	if (GApplication::WindowSizeChanged) {
		GetDirectQueue()->WaitForCompletion();
		GetSwapChain()->Resize(GApplication::WindowWidth, GApplication::WindowHeight);
		GApplication::WindowSizeChanged = 0;

		AllocateScreenResources();
	}

	ImGuiIO& io = ImGui::GetIO();

	// Setup display size (every frame to accommodate for window resizing)
	RECT rect;
	GetClientRect(Win32::GetWndHandle(), &rect);
	io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

	i64 CurrentTime;
	QueryPerformanceCounter((LARGE_INTEGER *)&CurrentTime);
	io.DeltaTime = (float)(CurrentTime - GApplication::Time) / GApplication::CpuFrequency;
	GApplication::Time = CurrentTime;

	// Read keyboard modifiers inputs
	io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
	io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;

	SetCursor(io.MouseDrawCursor ? NULL : LoadCursor(NULL, IDC_ARROW));

	ImGui::NewFrame();

	bool UpdateResult = Update();

	ImGui::Render();

	FGPUContext Context;
	static FCommandsStream Stream;

	Context.Open(EContextType::DIRECT);
	Stream.Reset();
	Stream.SetAccess(GetBackbuffer(), EAccessType::COMMON);
	Stream.Close();
	Playback(Context, &Stream);
	Context.Execute();
	GetDirectQueue()->Flush();
	GetSwapChain()->Present();

	EndFrame();

	return UpdateResult;
}
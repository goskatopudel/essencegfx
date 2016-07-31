#include "Viewport.h"
#include <DirectXMath.h>
#include "MathFunctions.h"
#include "Camera.h"
using namespace DirectX;

void GenerateViewport(FRenderViewport &Viewport, FCamera * Camera, Vec2i Resolution, float FovY, float NearPlane, float FarPlane) {
	Viewport.Resolution = Resolution;

	auto ProjMatrix = XMMatrixPerspectiveFovLH(
		FovY,
		(float)Resolution.x / (float)Resolution.y,
		NearPlane, FarPlane);

	auto ViewMatrix = XMMatrixLookToLH(
		ToSIMD(Camera->Position),
		ToSIMD(Camera->Direction),
		ToSIMD(Camera->Up));

	XMVECTOR Determinant;

	auto InvViewMatrix = XMMatrixInverse(&Determinant, ViewMatrix);
	auto InvProjMatrix = XMMatrixInverse(&Determinant, ProjMatrix);

	auto ViewProjMatrix = ViewMatrix * ProjMatrix;
	auto InvViewProjMatrix = XMMatrixInverse(&Determinant, ViewProjMatrix);

	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.ViewMatrix, ViewMatrix);
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.InvViewMatrix, InvViewMatrix);
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.ProjectionMatrix, ProjMatrix);
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.InvProjectionMatrix, InvProjMatrix);
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.ViewProjectionMatrix, ViewProjMatrix);
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.InvViewProjectionMatrix, InvViewProjMatrix);

	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.TViewMatrix, XMMatrixTranspose(ViewMatrix));
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.TInvViewMatrix, XMMatrixTranspose(InvViewMatrix));
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.TProjectionMatrix, XMMatrixTranspose(ProjMatrix));
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.TInvProjectionMatrix, XMMatrixTranspose(InvProjMatrix));
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.TViewProjectionMatrix, XMMatrixTranspose(ViewProjMatrix));
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.TInvViewProjectionMatrix, XMMatrixTranspose(InvViewProjMatrix));
}

void GenerateShadowmapViewport(FRenderViewport &Viewport, Vec2i Resolution, float3 Direction) {
	Viewport.Resolution = Resolution;

	auto ProjMatrix = XMMatrixOrthographicLH(50.f, 50.f, 0.1f, 100.f);

	auto ViewMatrix = XMMatrixLookToLH(
		ToSIMD(Direction * -50.f),
		ToSIMD(Direction),
		ToSIMD(float3(0,0,1)));

	XMVECTOR Determinant;

	auto InvViewMatrix = XMMatrixInverse(&Determinant, ViewMatrix);
	auto InvProjMatrix = XMMatrixInverse(&Determinant, ProjMatrix);

	auto ViewProjMatrix = ViewMatrix * ProjMatrix;
	auto InvViewProjMatrix = XMMatrixInverse(&Determinant, ViewProjMatrix);

	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.ViewMatrix, ViewMatrix);
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.InvViewMatrix, InvViewMatrix);
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.ProjectionMatrix, ProjMatrix);
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.InvProjectionMatrix, InvProjMatrix);
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.ViewProjectionMatrix, ViewProjMatrix);
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.InvViewProjectionMatrix, InvViewProjMatrix);

	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.TViewMatrix, XMMatrixTranspose(ViewMatrix));
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.TInvViewMatrix, XMMatrixTranspose(InvViewMatrix));
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.TProjectionMatrix, XMMatrixTranspose(ProjMatrix));
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.TInvProjectionMatrix, XMMatrixTranspose(InvProjMatrix));
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.TViewProjectionMatrix, XMMatrixTranspose(ViewProjMatrix));
	DirectX::XMStoreFloat4x4((XMFLOAT4X4*)&Viewport.TInvViewProjectionMatrix, XMMatrixTranspose(InvViewProjMatrix));
}

#include "Resource.h"

DXGI_FORMAT FRenderTargetDesc::GetFormat() const {
	return Resource->GetWriteFormat(IsSRGB > 0);
}

D3D12_CPU_DESCRIPTOR_HANDLE FRenderTargetDesc::GetRTV() const {
	return Resource->GetRTV(GetFormat());
}
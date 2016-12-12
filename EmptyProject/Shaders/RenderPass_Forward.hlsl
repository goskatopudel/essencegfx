#include "ShaderCommon.inl"

ConstantBuffer<FFrameConstants> Frame : register(b0);
struct FObjectConstants
{      
    float4x4 WorldMatrix;
    float4x4 PrevWorldMatrix;
    float4x4 Dummy;
};
ConstantBuffer<FObjectConstants> Object : register(b2);

#include "VertexInterface.hlsli"
#include [VERTEX]
#include "MaterialSurfaceInterface.hlsli"
#include [MATERIAL]

void VertexMain(VertexIn Vertex, FPixelInterpolated Output) {
	FVertexInterface VertexInterface;
	FVertexInterface_construct(VertexInterface);
	LoadVertex(Vertex, VertexInterface);
	MaterialVertexMain(VertexInterface, Output);
}

void PixelMain(FPixelInterpolated Interpolated, out float4 OutColor : SV_Target0) {
	FMaterialSurfaceInterface MatSurface;
	FMaterialSurfaceInterface_construct(MatSurface);
	MaterialPixelMain(Interpolated, MatSurface);
	OutColor = float4(MatSurface.Albedo, 1.f);
}
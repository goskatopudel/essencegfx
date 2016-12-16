#include "BasicMaterial_Data.h"

ConstantBuffer<FFrameConstants> Frame : register(b0, space0);
ConstantBuffer<FTestMaterial_Object> Object : register(b0, space2);

Texture2D<float4> 	AlbedoTexture : register(t0, space1);

struct VIn 
{
	float3 	Position : POSITION;
	float3 	Normal : NORMAL;
	float3 	Tangent : TANGENT;
	float3 	Bitangent : BITANGENT;
	float2 	Texcoord0 : TEXCOORD0;
	float2 	Texcoord1 : TEXCOORD1;
	float3 	Color : COLOR;
};

struct VOut
{
	float4 SvPosition : SV_POSITION;
	float4 PrevClipPosition : PREV_CLIP_POSITION;
	float3 Normal : NORMAL;
	float2 Texcoord0 : TEXCOORD0;
	float3 WorldPosition : WORLD_POSITION;
};

VOut VertexMain(VIn Input, uint VertexId : SV_VertexID)
{
	VOut Output;

	float4 position = mul(float4(Input.Position, 1), Object.WorldMatrix);
	Output.WorldPosition = position.xyz;
	Output.Normal = mul(Input.Normal, (float3x3)Object.WorldMatrix);
	Output.SvPosition = mul(position, Frame.ViewProjectionMatrix);
	Output.Texcoord0 = Input.Texcoord0;

	float4 prevPosition = mul(float4(Input.Position, 1), Object.PrevWorldMatrix);
	Output.PrevClipPosition = mul(prevPosition, Frame.PrevViewProjectionMatrix);

	return Output;
}

struct ForwardOutput {
	float4 Color : SV_TARGET0;
};

void PixelMain(VOut Interpolated, out ForwardOutput Output)
{
	float2 ScreenClipspace = Interpolated.SvPosition.xy / (float2) Frame.ScreenResolution * float2(2, -2) - 0.5f;
	float3 Albedo = AlbedoTexture.Sample(TextureSampler, Interpolated.Texcoord0).rgb;
	float3 N = normalize(Interpolated.Normal);

	float4 prevPosition = Interpolated.PrevClipPosition;
	prevPosition /= prevPosition.w;

	float2 currentNdcPosition = Interpolated.SvPosition.xy / (float2)Frame.ScreenResolution;
	float2 prevNdcPosition = prevPosition.xy * float2(0.5f,-0.5f) + 0.5f;

	Output.Color = float4(Albedo, 1.f);
}
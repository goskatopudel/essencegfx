cbuffer FrameConstants : register(b0)
{      
    float4x4 ViewProjectionMatrix;
	float4x4 ViewMatrix;
	float4x4 ProjectionMatrix;
	float4x4 InvViewProjectionMatrix;
	float4x4 InvViewMatrix;
	float4x4 InvProjectionMatrix;
	float2 ScreenResolution;
}

cbuffer ObjectConstants : register(b2)
{      
    float4x4 WorldMatrix;
    float4x4 ShadowmapMatrix;
}

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

#define TEXTURE_ANISO_SAMPLER s0, space0
#define TEXTURE_BILINEAR_WRAP_SAMPLER s0, space1
#define TEXTURE_POINT_WRAP_SAMPLER s0, space2
#define TEXTURE_BILINEAR_CLAMP_SAMPLER s0, space3
#define TEXTURE_POINT_CLAMP_SAMPLER s0, space4

Texture2D<float> ShadowmapTexture	: register(t0);
Texture2D<float> ShadowmapM2Texture	: register(t1);
SamplerState ShadowmapSampler : register(TEXTURE_ANISO_SAMPLER);

struct VOut
{
	float4 SvPosition : SV_POSITION;
	float3 WorldPosition : WORLD_POSITION;
};

VOut VertexMain(VIn Input, uint VertexId : SV_VertexID)
{
	VOut Output;

	float4 position = mul(float4(Input.Position, 1), WorldMatrix);
	Output.WorldPosition = position.xyz;
	Output.SvPosition = mul(position, ViewProjectionMatrix);

	return Output;
} 

float ChebyshevUpperBound(float2 Moments, float t)  
{  
	// One-tailed inequality valid if t > Moments.x  
	float p = (t <= Moments.x);  
	float Variance = Moments.y - Moments.x * Moments.x;
	const float MinVariance = 0.00001f;
	Variance = max(Variance, MinVariance);  
	// Compute probabilistic upper bound.  
	float d = t - Moments.x;
	float p_max = Variance / (Variance + d*d);  
	return max(p, p_max);  
} 

void PixelMain(VOut Interpolated, out float4 OutColor : SV_TARGET0)
{
	float4 shadowPosition = mul(float4(Interpolated.WorldPosition, 1), ShadowmapMatrix);
	shadowPosition /= shadowPosition.w;

	shadowPosition.xy = shadowPosition.xy * float2(0.5f, -0.5f) + 0.5f;

	float2 Moments;
	Moments.x = ShadowmapTexture.Sample(ShadowmapSampler, shadowPosition.xy);
	Moments.y = ShadowmapM2Texture.Sample(ShadowmapSampler, shadowPosition.xy);

	float Light = ChebyshevUpperBound(Moments, shadowPosition.z);

	OutColor = Light;
}


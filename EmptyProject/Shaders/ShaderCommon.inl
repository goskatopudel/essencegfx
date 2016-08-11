#ifndef __cplusplus
#define __HLSL
#endif

#ifdef __HLSL
#define TEXTURE_ANISO_SAMPLER s0, space0
#define TEXTURE_BILINEAR_WRAP_SAMPLER s0, space1
#define TEXTURE_POINT_WRAP_SAMPLER s0, space2
#define TEXTURE_BILINEAR_CLAMP_SAMPLER s0, space3
#define TEXTURE_POINT_CLAMP_SAMPLER s0, space4
#endif

struct FFrameConstants
{      
    float4x4 ViewProjectionMatrix;
	float4x4 ViewMatrix;
	float4x4 ProjectionMatrix;
	float4x4 InvViewProjectionMatrix;
	float4x4 InvViewMatrix;
	float4x4 InvProjectionMatrix;
	float4x4 PrevViewProjectionMatrix;
	float2 ScreenResolution;
};
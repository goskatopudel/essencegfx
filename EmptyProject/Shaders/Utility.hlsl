#ifndef MIPMAP
#define MIPMAP 0
#endif

cbuffer Constants : register(b0)
{      
    uint Mipmap;
}

struct VOut
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

VOut VertexMain(uint vertexId : SV_VertexID)
{
	VOut output;
	output.position.x = (float)(vertexId / 2) * 4 - 1;
	output.position.y = (float)(vertexId % 2) * 4 - 1;
	output.position.z = 1;
	output.position.w = 1;

	output.texcoord.x = (float)(vertexId / 2) * 2;
	output.texcoord.y= 1 - (float)(vertexId % 2) * 2;

	return output;
}

#define TEXTURE_ANISO_SAMPLER s0, space0
#define TEXTURE_BILINEAR_WRAP_SAMPLER s0, space1
#define TEXTURE_POINT_WRAP_SAMPLER s0, space2
#define TEXTURE_BILINEAR_CLAMP_SAMPLER s0, space3
#define TEXTURE_POINT_CLAMP_SAMPLER s0, space4

Texture2D<float4> 	SourceTexture : register(t0);
SamplerState    	Sampler : register(TEXTURE_BILINEAR_CLAMP_SAMPLER);
SamplerState    	PointSampler : register(TEXTURE_POINT_CLAMP_SAMPLER);

float4 				WriteColor;
float 				WriteDepth;

float4 CopyPixelMain(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
	return SourceTexture.SampleLevel(Sampler, texcoord, Mipmap);
}

#define DEPTH_BILINEAR 0
#define DEPTH_MIN 1

#ifndef DEPTH_DOWNSAMPLE
#define DEPTH_DOWNSAMPLE DEPTH_BILINEAR
#endif

float DownsampleDepthMain(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_Depth
{
#if DEPTH_DOWNSAMPLE == DEPTH_MIN
	uint2 Texel = position.xy;
	float Depth = SourceTexture[Texel * 2].r;
	Depth = min(Depth, SourceTexture[Texel * 2 + uint2(1, 0)].r);
	Depth = min(Depth, SourceTexture[Texel * 2 + uint2(1, 1)].r);
	Depth = min(Depth, SourceTexture[Texel * 2 + uint2(0, 1)].r);
	return Depth;
#else
	return SourceTexture.SampleLevel(Sampler, texcoord, 0).r;
#endif
}

float4 ColorPS(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
	return WriteColor;
}

float DepthPS(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_Depth
{
	return WriteDepth;
}

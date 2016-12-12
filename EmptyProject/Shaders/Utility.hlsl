#ifndef MIPMAP
#define MIPMAP 0
#endif

cbuffer Constants : register(b0)
{      
    uint Mipmap;
}

struct VOut
{
	float4 Position : SV_POSITION;
	float2 Texcoord : TEXCOORD;
};

VOut VertexMain(uint VertexId : SV_VertexID)
{
	VOut Output;
	Output.Position.x = (float)(VertexId / 2) * 4 - 1;
	Output.Position.y = (float)(VertexId % 2) * 4 - 1;
	Output.Position.z = 1;
	Output.Position.w = 1;

	Output.Texcoord.x = (float)(VertexId / 2) * 2;
	Output.Texcoord.y= 1 - (float)(VertexId % 2) * 2;

	return Output;
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

float4 CopyPixelMain(float4 SvPosition : SV_POSITION, float2 Texcoord : TEXCOORD) : SV_TARGET
{
	return SourceTexture.SampleLevel(Sampler, Texcoord, Mipmap);
}

#define DEPTH_BILINEAR 0
#define DEPTH_MIN 1

#ifndef DEPTH_DOWNSAMPLE
#define DEPTH_DOWNSAMPLE DEPTH_BILINEAR
#endif

float DownsampleDepthMain(float4 Position : SV_POSITION, float2 Texcoord : TEXCOORD) : SV_Depth
{
#if DEPTH_DOWNSAMPLE == DEPTH_MIN
	uint2 Texel = Position.xy;
	float Depth = SourceTexture[Texel * 2].r;
	Depth = min(Depth, SourceTexture[Texel * 2 + uint2(1, 0)].r);
	Depth = min(Depth, SourceTexture[Texel * 2 + uint2(1, 1)].r);
	Depth = min(Depth, SourceTexture[Texel * 2 + uint2(0, 1)].r);
	return Depth;
#else
	return SourceTexture.SampleLevel(Sampler, Texcoord, 0).r;
#endif
}

float4 ColorPS(float4 Position : SV_POSITION, float2 Texcoord : TEXCOORD) : SV_TARGET
{
	return WriteColor;
}

float DepthPS(float4 Position : SV_POSITION, float2 Texcoord : TEXCOORD) : SV_Depth
{
	return WriteDepth;
}

float4 BlurPixelMain(float4 SvPosition : SV_POSITION, float2 Texcoord : TEXCOORD) : SV_TARGET
{
	float4 Sum = 0;
	int2 Texel = SvPosition.xy;

	// 3x3 box
	[unroll]
	for(int x = -1; x <= 1; ++x) {
		[unroll]
		for(int y = -1; y <= 1; ++y) {
			Sum += SourceTexture[Texel + int2(x, y)];
		}
	}
	return Sum / 9.f;
}

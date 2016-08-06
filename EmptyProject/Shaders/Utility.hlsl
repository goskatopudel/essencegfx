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

Texture2D<float4> 	SourceTexture : register(t0);
SamplerState    	Sampler : register(s0, space4); // static bilinear

float4 				WriteColor;
float 				WriteDepth;

float4 CopyPixelMain(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
	return SourceTexture.Sample(Sampler, texcoord);
}

float4 ColorPS(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
	return WriteColor;
}

float DepthPS(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_Depth
{
	return WriteDepth;
}

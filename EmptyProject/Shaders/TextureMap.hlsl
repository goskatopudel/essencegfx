SamplerState TextureSampler : register(s0);

struct VIn 
{
	float3 	position : POSITION;
	float3 	normal : NORMAL;
	float3 	tangent : TANGENT;
	float3 	bitangent : BITANGENT;
	float2 	texcoord0 : TEXCOORD0;
	float2 	texcoord1 : TEXCOORD1;
	float3 	color : COLOR;
};

struct VOut
{
	float4 	position : SV_POSITION;
	float3 	wposition : POSITION;
	float3 	normal : NORMAL;
};

VOut VShader(VIn input, uint vertexId : SV_VertexID)
{
	VOut output;
	output.position = float4(input.texcoord1 * float2(2, -2) + float2(-1, 1), 0, 1);
	output.wposition = input.position.xyz;
	output.normal = input.normal;
	return output;
}

void PShader(VOut interpolated, out float4 OutPosition : SV_TARGET0, out float4 OutNormal : SV_TARGET1)
{
	float3 N = normalize(interpolated.normal);
	OutPosition = float4(interpolated.wposition, 1);
	OutNormal = float4(N, 1);
}


cbuffer Constants : register(b0)
{      
    matrix 	WorldViewProj;
    float2  DepthRange;
}

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
	float 	depth : LINEAR_DEPTH;
};

VOut VShader(VIn input, uint vertexId : SV_VertexID)
{
	VOut output;

	matrix objectMatrix = World;

	float4 position = float4(input.position, 1);
	position = mul(position, objectMatrix);
	output.wposition = position.xyz;
	position = mul(position, ViewProj);
	output.position = position;
	output.normal = normalize(mul(input.normal, (float3x3) objectMatrix));
	output.texcoord0 = input.texcoord0;
	output.texcoord1 = input.texcoord1;
	output.color = input.color;
	return output;
}

void PSDebug(VOut interpolated, out float4 OutColor : SV_TARGET0)
{
	float3 N = normalize(interpolated.normal);
	float3 V = normalize(InvView[3].xyz - interpolated.wposition);
	float NoV = saturate(dot(N, V));

	uint2 ScreenCoord = interpolated.position.xy;
	float Dither = BayerMatrix8[ScreenCoord.x % 8][ScreenCoord.y % 8] / 64.f;

	float4 sample = UVTexture.Sample(TextureSampler, interpolated.texcoord0 );

	float4 shadowmapUV = mul(float4(interpolated.wposition, 1), WorldToShadow);
	shadowmapUV /= shadowmapUV.w;
	shadowmapUV.xy = shadowmapUV.xy * float2(0.5f, -0.5f) + 0.5f;
	float shadowmapSample = ShadowmapTexture.Sample(PointSampler, shadowmapUV.xy);

	// float lit = 0;
	// for(uint Index = 0; Index < 4; ++Index) {
	// 	lit += shadowmapSample > shadowmapUV.z - ddif * BayerMatrix8[(ScreenCoord.x * 2 + Index % 2) % 8][(ScreenCoord.y * 2 + Index / 2) % 8] / 64.f;
	// }
	// lit /= 4.f;

	float lit = shadowmapSample > shadowmapUV.z - 0.0001f;

	OutColor = saturate(dot(N, L)) * lit;
}


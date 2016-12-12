Texture2D<float4> 	AlbedoTexture : register(t0);

struct FPixelInterpolated
{
	float4 SvPosition : SV_POSITION;
	float4 PrevClipPosition : PREV_CLIP_POSITION;
	float3 Normal : NORMAL;
	float2 Texcoord0 : TEXCOORD0;
	float3 WorldPosition : WORLD_POSITION;
};

void MaterialVertexMain(FVertexInterface VertexInterface, out FPixelInterpolated Output)
{
	float4 position = mul(float4(VertexInterface.Position, 1), Object.WorldMatrix);
	Output.WorldPosition = position.xyz;
	Output.Normal = mul(VertexInterface.Normal, (float3x3)Object.WorldMatrix);
	Output.SvPosition = mul(position, Frame.ViewProjectionMatrix);
	Output.Texcoord0 = VertexInterface.Texcoord0;

	float4 prevPosition = mul(float4(VertexInterface.Position, 1), Object.PrevWorldMatrix);
	Output.PrevClipPosition = mul(prevPosition, Frame.PrevViewProjectionMatrix);
}

void MaterialPixelMain(FPixelInterpolated Interpolated, inout FMaterialSurfaceInterface MatSurfaceInterface)
{
	float2 ScreenClipspace = Interpolated.SvPosition.xy / (float2) Frame.ScreenResolution * float2(2, -2) - 0.5f;
	float3 Albedo = AlbedoTexture.Sample(TextureSampler, Interpolated.Texcoord0).rgb;
	MatSurfaceInterface.Albedo = Albedo;

	float3 N = normalize(Interpolated.Normal);
	MatSurfaceInterface.WorldNormal = N;

	float4 prevPosition = Interpolated.PrevClipPosition;
	prevPosition /= prevPosition.w;

	float2 currentNdcPosition = Interpolated.SvPosition.xy / (float2)Frame.ScreenResolution;
	float2 prevNdcPosition = prevPosition.xy * float2(0.5f,-0.5f) + 0.5f;
	MatSurfaceInterface.MotionVector = currentNdcPosition - prevNdcPosition;	
}
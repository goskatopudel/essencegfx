Texture2D PositionsTexture : register(t0);
Texture2D NormalsTexture : register(t1);

struct BVHNode {
    float3 	VMin;
    float3 	VMax;
};

StructuredBuffer<BVHNode>	BVH : register(t2);
RWTexture2D BakedSignal : register(u0);

[numthreads(8, 8, 1)]
void BakeAO(uint3 DTid : SV_DispatchThreadID) {
	float3 Position = PositionsTexture[DTid.xy].xyz;
	float3 Normal = NormalsTexture[DTid.xy].xyz;

	if(length(Normal) > 0.f) {
		BakedSignal[DTid.xy] = 1;
	}
	else {
		BakedSignal[DTid.xy] = 0;
	}
}
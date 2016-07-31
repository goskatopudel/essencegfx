cbuffer FrameConstants : register(b0) {      
    matrix ViewProjectionMatrix;
}

float3 SG(float mu, float lambda, float3 P, float3 V) {
	return mu * exp(lambda * (dot(V, P) - 1));
}

struct VS_In {
	float3 	Position : POSITION;
};

struct VS_Out {
	float3 	Position : POSITION;
};

VS_Out VertexMain(VS_In Input) {
	VS_Out Output;
	Output.Position = Input.Position;
	return Output;
}

struct HS_PER_PATCH_OUTPUT {
	float EdgeTessalation[3] : SV_TessFactor;
	float InsideTessalation[1] : SV_InsideTessFactor;
};

HS_PER_PATCH_OUTPUT HullPatchFunc(InputPatch<VS_Out, 3> InPatch, uint PatchID : SV_PrimitiveID) {
	HS_PER_PATCH_OUTPUT Output;

	Output.EdgeTessalation[0] = Output.EdgeTessalation[1] = Output.EdgeTessalation[2] = 1.f;
	Output.InsideTessalation[0] = 1.f;

	return Output;
}

struct HS_OUTPUT {
	float3 	Position : Position;
};

[domain("tri")]
[partitioning("pow2")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("HullPatchFunc")]
HS_OUTPUT HullMain(InputPatch<VS_Out, 3> InVertices, uint ControlPointID : SV_OutputControlPointID) {
	HS_OUTPUT Output;

	Output.Position = InVertices[ControlPointID].Position;

	return Output;
}

struct DS_OUTPUT {
	float4 Position : SV_Position;
	float3 Normal : NORMAL;
};

[domain("tri")]
DS_OUTPUT DomainMain(HS_PER_PATCH_OUTPUT Patch, const OutputPatch<HS_OUTPUT, 3> PatchPoints, float3 DomainCoord : SV_DomainLocation) {
	DS_OUTPUT Output;

	float3 P = normalize(PatchPoints[0].Position * DomainCoord.x + PatchPoints[1].Position * DomainCoord.y + PatchPoints[2].Position * DomainCoord.z);

	float4 Position = mul(float4(P, 1), ViewProjectionMatrix);
	Output.Position = Position;

	Output.Normal = P;

	return Output;
}

struct PS_In {
	float4 	Position : SV_POSITION;
};

void PixelMain(DS_OUTPUT Interpolated, out float4 OutColor : SV_TARGET0) {
	OutColor = float4(normalize(Interpolated.Normal) * 0.5 + 0.5, 1);
}
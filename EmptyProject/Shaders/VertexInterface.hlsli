struct FVertexInterface
{
	float3 	Position;
	float3 	Normal;
	float3 	Tangent;
	float3 	Bitangent;
	float2 	Texcoord0;
	float2 	Texcoord1;
	float3 	Color;
};

void FVertexInterface_construct(inout FVertexInterface VertexInterface) {
	VertexInterface.Position = 0.f;
	VertexInterface.Normal = 0.f;
	VertexInterface.Tangent = 0.f;
	VertexInterface.Bitangent = 0.f;
	VertexInterface.Texcoord0 = 0.f;
	VertexInterface.Texcoord1 = 0.f;
	VertexInterface.Color = 0.f;
}
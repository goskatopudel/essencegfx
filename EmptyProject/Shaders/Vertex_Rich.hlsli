struct VertexIn
{
	float3 	Position : POSITION;
	float3 	Normal : NORMAL;
	float3 	Tangent : TANGENT;
	float3 	Bitangent : BITANGENT;
	float2 	Texcoord0 : TEXCOORD0;
	float2 	Texcoord1 : TEXCOORD1;
	float3 	Color : COLOR;
};

void LoadVertex(VertexIn Vertex, inout FVertexInterface VertexInterface) {
	VertexInterface.Position = Vertex.Position;
	VertexInterface.Normal = Vertex.Normal;
	VertexInterface.Tangent = Vertex.Tangent;
	VertexInterface.Bitangent = Vertex.Bitangent;
	VertexInterface.Texcoord0 = Vertex.Texcoord0;
	VertexInterface.Texcoord1 = Vertex.Texcoord1;
	VertexInterface.Color = Vertex.Color;
}
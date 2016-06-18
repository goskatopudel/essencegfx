#include "EditorModelFormat.h"
#include "ModelHelpers.h"

constexpr u32 CURRENT_VERSION = 1;

struct FEditorModelFileHeader {
	u8		Header[26]; // "BINARY_EDITOR_MODEL_HEADER"
	u8		AssetName[128];
	u32		Version;
	u32		MeshesNum;
	u32		MaterialsNum;
};

struct FEditorMeshHeader {
	u8		Name[128];
	u32		IndicesNum;
	u32		VerticesNum;
	u32		MaterialIndex;
	u32		HasNormals : 1;
	u32		HasTangentSpace : 1;
	u32		HasTexcoords0 : 1;
	u32		HasTexcoords1 : 1;
	u32		HasColors : 1;
	u32		FlagsReserved : 27;

	union {
		struct { u32 Reserved[256]; };
		struct {
			u32		UVAtlasResolutionX;
			u32		UVAtlasResolutionY;
		};
	};
};

class FInStream {
public:
	FILE * File = nullptr;

	~FInStream() {
		if (File) {
			fclose(File);
			File = nullptr;
		}
	}

	void OpenForRead(const wchar_t * filename) {
		verify(_wfopen_s(&File, filename, L"rb") == 0);
	}

	void Read(void* Dst, u64 Bytesize) {
		if (Bytesize) {
			verify(fread(Dst, Bytesize, 1, File) == 1);
		}
	}

	template<typename T>
	void Read(T & TypedRef) {
		Read(&TypedRef, sizeof(T));
	}

	template<typename T>
	void Read(eastl::vector<T> & Vector) {
		u64 size;
		Read(size);
		Vector.resize(size);
		Read(Vector.data(), Vector.size() * sizeof(T));
	}

	template<>
	void Read(eastl::string & Str) {
		u64 size;
		Read(size);
		Str.resize(size);
		Read((void*)Str.data(), size + 1);
	}
};

class FOutStream {
public:
	FILE * File = nullptr;

	~FOutStream() {
		if (File) {
			fclose(File);
			File = nullptr;
		}
	}

	void OpenForWrite(const wchar_t * filename) {
		verify(_wfopen_s(&File, filename, L"wb") == 0);
	}

	void Write(const void* Src, u64 Bytesize) {
		fwrite(Src, Bytesize, 1, File);
	}

	template<typename T>
	void Write(T const& TypedValue) {
		Write(&TypedValue, sizeof(T));
	}

	template<typename T>
	void Write(eastl::vector<T> const& Vector) {
		u64 size = Vector.size();
		Write(size);
		Write(Vector.data(), Vector.size() * sizeof(T));
	}

	template<>
	void Write(eastl::string const& Str) {
		u64 Len = Str.size();
		Write(Len);
		Write(Str.data(), Len + 1);
	}
};

constexpr const char * HEADER_STRING = "BINARY_EDITOR_MODEL_HEADER";
constexpr u32 HEADER_STRING_LEN = 26;

void SaveEditorModel(FEditorModel const * Model, const wchar_t * Filename) {
	FOutStream OutStream;
	OutStream.OpenForWrite(Filename);

	FEditorModelFileHeader Header = {};
	memcpy(Header.Header, HEADER_STRING, sizeof(Header.Header));
	static_assert(HEADER_STRING_LEN == sizeof(Header.Header), "Model header string doesn't fit header space");
	Header.MaterialsNum = 0;
	Header.MeshesNum = (u32)Model->Meshes.size();
	Header.Version = CURRENT_VERSION;
	OutStream.Write(Header);

	// Meshes
	for (u32 MeshIndex = 0; MeshIndex < Header.MeshesNum; MeshIndex++) {
		auto & Mesh = Model->Meshes[MeshIndex];

		FEditorMeshHeader MeshHeader = {};
		MeshHeader.VerticesNum = Mesh.GetVerticesNum();
		MeshHeader.IndicesNum = Mesh.GetIndicesNum();
		MeshHeader.HasNormals = Mesh.Normals.size() > 0;
		MeshHeader.HasColors = Mesh.Colors.size() > 0;
		MeshHeader.HasTangentSpace = Mesh.Tangents.size() > 0 && Mesh.Bitangents.size() > 0;
		MeshHeader.HasTexcoords0 = Mesh.Texcoords0.size() > 0;
		MeshHeader.HasTexcoords1 = Mesh.Texcoords1.size() > 0;

		if (MeshHeader.HasTexcoords1) {
			MeshHeader.UVAtlasResolutionX = Mesh.AtlasInfo.ResolutionX;
			MeshHeader.UVAtlasResolutionY = Mesh.AtlasInfo.ResolutionY;
		}

		OutStream.Write(MeshHeader);

		OutStream.Write(Mesh.Indices);
		OutStream.Write(Mesh.Positions);
		if (MeshHeader.HasNormals) {
			OutStream.Write(Mesh.Normals);
		}
		if (MeshHeader.HasTangentSpace) {
			OutStream.Write(Mesh.Tangents);
			OutStream.Write(Mesh.Bitangents);
		}
		if (MeshHeader.HasColors) {
			OutStream.Write(Mesh.Colors);
		}
		if (MeshHeader.HasTexcoords0) {
			OutStream.Write(Mesh.Texcoords0);
		}
		if (MeshHeader.HasTexcoords1) {
			OutStream.Write(Mesh.Texcoords1);
		}
	}

	// Materials
	// TODO
}

#include "Print.h"

void LoadEditorModelInternal(FEditorModel * Model, const wchar_t * Filename) {
	Model->Clear();

	FInStream InStream;
	InStream.OpenForRead(Filename);
	FEditorModelFileHeader Header;
	InStream.Read(Header);

	check(Header.Version == 1);
	check(strncmp((const char *)Header.Header, HEADER_STRING, HEADER_STRING_LEN) == 0);

	for (u32 MeshIndex = 0; MeshIndex < Header.MeshesNum; MeshIndex++) {
		FEditorMeshHeader MeshHeader;
		InStream.Read(MeshHeader);

		FEditorMesh Mesh;
		u32 VerticesNum = MeshHeader.VerticesNum;
		u32 IndicesNum = MeshHeader.IndicesNum;

		InStream.Read(Mesh.Indices);
		InStream.Read(Mesh.Positions);
		if (MeshHeader.HasNormals) {
			InStream.Read(Mesh.Normals);
		}
		if (MeshHeader.HasTangentSpace) {
			InStream.Read(Mesh.Tangents);
			InStream.Read(Mesh.Bitangents);
		}
		if (MeshHeader.HasColors) {
			InStream.Read(Mesh.Colors);
		}
		if (MeshHeader.HasTexcoords0) {
			InStream.Read(Mesh.Texcoords0);
		}
		if (MeshHeader.HasTexcoords1) {
			InStream.Read(Mesh.Texcoords1);
		}

		Model->AddMesh(std::move(Mesh));

		BuildBVH(&Model->Meshes.back(), &Model->Meshes.back().BVH);
	}
}

void LoadEditorModel(FEditorModel * Model, const wchar_t * Filename) {
	LoadEditorModelInternal(Model, Filename);

	if (Model == nullptr) {
		PrintFormated(L"Failed to load %s\n");
	}
}

#include "Shader.h"

#include <EASTL\unique_ptr.h>
#include <EASTL\hash_map.h>
#include <EASTL\string.h>
#include <EASTL\initializer_list.h>
#include <EASTL\sort.h>
#include <EASTL\vector.h>
#include "Print.h"
#include "FileIO.h"

#include <D3Dcompiler.h>
#pragma comment(lib,"d3dcompiler.lib")

#include "Hash.h"
#include "Device.h"

class FCompiledShader {
public:
	eastl::unique_ptr<u8[]>			Bytecode;
	u64								Size;
	u64								Hash;

	FCompiledShader() = default;
	FCompiledShader(u8* code, u64 size, u64 hash) : Size(size), Hash(hash) {
		Bytecode = eastl::make_unique<u8[]>(size);
		memcpy(Bytecode.get(), code, Size);
	}
};

u64 GetShaderHash(FShader const* shader) {
	return shader->LocationHash;
}

struct ShaderHash {
	u64 hash;
};

bool operator == (ShaderHash a, ShaderHash b) { return a.hash == b.hash; };
bool operator != (ShaderHash a, ShaderHash b) { return a.hash != b.hash; };

template<>
struct eastl::hash<ShaderHash> { u64 operator()(ShaderHash h) const { return h.hash; } };
			
eastl::hash_map<ShaderHash, eastl::shared_ptr<FShader>>			ShadersLocationLookup;
eastl::hash_map<ShaderHash, eastl::shared_ptr<FCompiledShader>>	ShadersCodeLookup;

u64			GShadersCompilationVersion = 0;

void FShader::Compile() {
	auto ShaderCode = ReadEntireFile(File.c_str());

	unique_com_ptr<ID3DBlob> CodeBlob;
	unique_com_ptr<ID3DBlob> ErrorsBlob;
	HRESULT preprocessResult = D3DPreprocess(ShaderCode.Data, ShaderCode.Bytesize, File.c_str(), Macros.get(), D3D_COMPILE_STANDARD_FILE_INCLUDE, CodeBlob.get_init(), ErrorsBlob.get_init());

	ShaderHash hashLookup = { MurmurHash2_64(CodeBlob->GetBufferPointer(), CodeBlob->GetBufferSize(), LocationHash) };
	auto codeCacheFind = ShadersCodeLookup.find(hashLookup);
	if (codeCacheFind != ShadersCodeLookup.end()) {
		if (Bytecode.get() != codeCacheFind->second.get()) {
			Bytecode = codeCacheFind->second;
			LastChangedVersion = GShadersCompilationVersion;
		}
	}
	else if (CodeBlob.get() && CodeBlob->GetBufferPointer()) {
		HRESULT hr = D3DCompile2(ShaderCode.Data, ShaderCode.Bytesize, File.c_str(), Macros.get(), D3D_COMPILE_STANDARD_FILE_INCLUDE, Func.c_str(), Target, Flags, 0, 0, nullptr, 0, CodeBlob.get_init(), ErrorsBlob.get_init());

		if (ErrorsBlob.get() && ErrorsBlob->GetBufferPointer()) {
			PrintFormated(L"Compilation errors: %s\n", ConvertToWString((const char*)ErrorsBlob->GetBufferPointer(), ErrorsBlob->GetBufferSize()).c_str());
		}

		if (CodeBlob.get() && CodeBlob->GetBufferPointer()) {
			Bytecode = ShadersCodeLookup[hashLookup] = eastl::make_shared<FCompiledShader>((u8*)CodeBlob->GetBufferPointer(), CodeBlob->GetBufferSize(), hashLookup.hash);
		}

		LastChangedVersion = GShadersCompilationVersion;
	}
}

eastl::wstring	FShader::GetDebugName() const {
	eastl::wstring w;
	w.sprintf(L"%s_%s_%s", ConvertToWString(File).c_str(), ConvertToWString(Func).c_str(), ConvertToWString(Target).c_str());
	return std::move(w);
}

u64 GetBytecodeHash(FShader const* shader) {
	return shader->Bytecode->Hash;
}

D3D12_SHADER_BYTECODE GetBytecode(FShader const* shader) {
	D3D12_SHADER_BYTECODE d3d12bytecode;
	d3d12bytecode.pShaderBytecode = shader->Bytecode->Bytecode.get();
	d3d12bytecode.BytecodeLength = shader->Bytecode->Size;
	return d3d12bytecode;
}

FShader* GetShader(eastl::string file, eastl::string func, const char* target, std::initializer_list<ShaderMacroPair> macros, u32 flags) {
	u64 shaderLocationHash = flags;
	shaderLocationHash = MurmurHash2_64(file.data(), file.size() * sizeof(file[0]), shaderLocationHash);
	shaderLocationHash = MurmurHash2_64(func.data(), func.size() * sizeof(func[0]), shaderLocationHash);
	shaderLocationHash = MurmurHash2_64(target, strlen(target) * sizeof(target[0]), shaderLocationHash);
	if (macros.size()) {
		shaderLocationHash = MurmurHash2_64(macros.begin(), macros.size() * sizeof(macros.begin()[0]), shaderLocationHash);
	}

	ShaderHash hashLookup = { shaderLocationHash };
	auto cacheFind = ShadersLocationLookup.find(hashLookup);
	if (cacheFind != ShadersLocationLookup.end()) {
		return cacheFind->second.get();
	}

	auto & Ref = ShadersLocationLookup[hashLookup] = eastl::make_shared<FShader>();
	FShader* shader = Ref.get();

	eastl::unique_ptr<D3D_SHADER_MACRO[]> d3dmacros = eastl::make_unique<D3D_SHADER_MACRO[]>(macros.size() + 1);
	for (auto & Macro : macros) {
		shader->MacrosRaw.push_back(Macro);
	}
	for (u32 Index = 0; Index < macros.size(); ++Index) {
		d3dmacros[Index].Name = shader->MacrosRaw[Index].first.c_str();
		d3dmacros[Index].Definition = shader->MacrosRaw[Index].second.c_str();
	}
	d3dmacros[macros.size()] = {};

	shader->File = file;
	shader->Func = func;
	shader->Target = target;
	shader->Macros = std::move(d3dmacros);
	shader->Flags = flags;
	shader->LocationHash = hashLookup.hash;

	shader->Compile();

	return shader;
}

void RecompileChangedShaders() {
	GShadersCompilationVersion++;

	for (auto & ShaderEntry : ShadersLocationLookup) {
		ShaderEntry.second->Compile();
	}
}

u32 GetShadersNum() {
	return (u32)ShadersCodeLookup.size();
}
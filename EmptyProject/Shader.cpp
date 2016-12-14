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

void FShaderCompilationEnvironment::SetDefine(eastl::string A, eastl::string B) {
	Macros.push_back(eastl::make_pair(std::move(A), std::move(B)));
}

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

struct ShaderHash {
	u64 hash;
};

bool operator == (ShaderHash a, ShaderHash b) { return a.hash == b.hash; };
bool operator != (ShaderHash a, ShaderHash b) { return a.hash != b.hash; };

template<>
struct eastl::hash<ShaderHash> { u64 operator()(ShaderHash h) const { return h.hash; } };
			
eastl::hash_map<ShaderHash, FShaderRef> GlobalShadersLookup;
eastl::hash_map<ShaderHash, eastl::shared_ptr<FCompiledShader>>	ShadersCodeLookup;

u64 GShadersCompilationVersion = 0;

class FGlobalShader : public FShader {
public:
	eastl::string							File;
	eastl::string							Func;
	const char*								Target;
	eastl::unique_ptr<D3D_SHADER_MACRO[]>	Macros;
	eastl::vector<ShaderMacroPair>			MacrosRaw;
	u32										Flags;

	void Compile() override;
	eastl::wstring	GetDebugName() const override;
	~FGlobalShader() {}
};

void FGlobalShader::Compile() {
	auto ShaderCode = ReadEntireFile(File.c_str());

	unique_com_ptr<ID3DBlob> CodeBlob;
	unique_com_ptr<ID3DBlob> ErrorsBlob;
	HRESULT preprocessResult = D3DPreprocess(ShaderCode.Data, ShaderCode.Bytesize, File.c_str(), Macros.get(), D3D_COMPILE_STANDARD_FILE_INCLUDE, CodeBlob.get_init(), ErrorsBlob.get_init());

	ShaderHash hashLookup = {};

	bool bFound = false;
	if(CodeBlob.get()) {
		hashLookup.hash = MurmurHash2_64(CodeBlob->GetBufferPointer(), CodeBlob->GetBufferSize(), PersistentHash);
		auto codeCacheFind = ShadersCodeLookup.find(hashLookup);
		if (codeCacheFind != ShadersCodeLookup.end()) {
			if (Bytecode.get() != codeCacheFind->second.get()) {
				Bytecode = codeCacheFind->second;
				LastChangedVersion = GShadersCompilationVersion;
				bFound = true;
			}
		}
	}
	if (!bFound && CodeBlob.get() && CodeBlob->GetBufferPointer()) {
		HRESULT hr = D3DCompile2(ShaderCode.Data, ShaderCode.Bytesize, File.c_str(), Macros.get(), D3D_COMPILE_STANDARD_FILE_INCLUDE, Func.c_str(), Target, Flags, 0, 0, nullptr, 0, CodeBlob.get_init(), ErrorsBlob.get_init());

		if (ErrorsBlob.get() && ErrorsBlob->GetBufferPointer()) {
			PrintFormated(L"Compilation errors: %s\n", ConvertToWString((const char*)ErrorsBlob->GetBufferPointer(), ErrorsBlob->GetBufferSize()).c_str());
		}

		if (CodeBlob.get() && CodeBlob->GetBufferPointer()) {
			Bytecode = ShadersCodeLookup[hashLookup] = eastl::make_shared<FCompiledShader>((u8*)CodeBlob->GetBufferPointer(), CodeBlob->GetBufferSize(), hashLookup.hash);
		}

		LastChangedVersion = GShadersCompilationVersion;
	}
	else if(ErrorsBlob.get() && ErrorsBlob->GetBufferPointer()) {
		PrintFormated(L"Compilation errors: %s\n", ConvertToWString((const char*)ErrorsBlob->GetBufferPointer(), ErrorsBlob->GetBufferSize()).c_str());
	}
}

eastl::wstring	FGlobalShader::GetDebugName() const {
	eastl::wstring w;
	w.sprintf(L"%s_%s_%s", ConvertToWString(File).c_str(), ConvertToWString(Func).c_str(), ConvertToWString(Target).c_str());
	return std::move(w);
}

FShaderBytecode FShader::GetShaderBytecode() const {
	FShaderBytecode Output;
	Output.Bytecode.BytecodeLength = Bytecode->Size;
	Output.Bytecode.pShaderBytecode = Bytecode->Bytecode.get();
	Output.BytecodeHash = Bytecode->Hash;
	return Output;
}

void RecompileChangedShaders() {
	GShadersCompilationVersion++;

	for (auto & ShaderEntry : GlobalShadersLookup) {
		ShaderEntry.second->Compile();
	}
}

u32 GetShadersNum() {
	return (u32)ShadersCodeLookup.size();
}

FShaderRef GetNullShader() {
	static FShaderRef NullShader;
	return NullShader;
}

FShaderRef GetGlobalShader(eastl::string file, eastl::string func, const char* target, u32 flags) {
	return GetGlobalShader(std::move(file), std::move(func), target, FShaderCompilationEnvironment(), flags);
}

FShaderRef GetGlobalShader(eastl::string file, eastl::string func, const char* target, FShaderCompilationEnvironment & environment, u32 flags) {
	u64 shaderLocationHash = flags;
	shaderLocationHash = MurmurHash2_64(file.data(), file.size() * sizeof(file[0]), shaderLocationHash);
	shaderLocationHash = MurmurHash2_64(func.data(), func.size() * sizeof(func[0]), shaderLocationHash);
	shaderLocationHash = MurmurHash2_64(target, strlen(target) * sizeof(target[0]), shaderLocationHash);
	if (environment.Macros.size()) {
		shaderLocationHash = MurmurHash2_64(environment.Macros.begin(), environment.Macros.size() * sizeof(environment.Macros.begin()[0]), shaderLocationHash);
	}

	ShaderHash hashLookup = { shaderLocationHash };
	auto cacheFind = GlobalShadersLookup.find(hashLookup);
	if (cacheFind != GlobalShadersLookup.end()) {
		return cacheFind->second;
	}

	auto & Ref = GlobalShadersLookup[hashLookup] = eastl::make_shared<FGlobalShader>();
	auto shader = static_cast<FGlobalShader*>(Ref.get());

	eastl::unique_ptr<D3D_SHADER_MACRO[]> d3dmacros = eastl::make_unique<D3D_SHADER_MACRO[]>(environment.Macros.size() + 1);
	for (auto & Macro : environment.Macros) {
		shader->MacrosRaw.push_back(Macro);
	}
	for (u32 Index = 0; Index < environment.Macros.size(); ++Index) {
		d3dmacros[Index].Name = shader->MacrosRaw[Index].first.c_str();
		d3dmacros[Index].Definition = shader->MacrosRaw[Index].second.c_str();
	}
	d3dmacros[environment.Macros.size()] = {};

	shader->File = file;
	shader->Func = func;
	shader->Target = target;
	shader->Macros = std::move(d3dmacros);
	shader->Flags = flags;
	shader->PersistentHash = hashLookup.hash;

	shader->Compile();

	return Ref;
}

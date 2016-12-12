#pragma once
#include "Essence.h"

#include <EASTL\string.h>
#include <EASTL\vector.h>
#include <EASTL\shared_ptr.h>

#include <d3d12.h>
#include "Ref.h"

class FShader;
class FCompiledShader;
typedef eastl::pair<eastl::string, eastl::string> ShaderMacroPair;

struct FShaderBytecode {
	D3D12_SHADER_BYTECODE Bytecode;
	u64 BytecodeHash;
};

class FShader {
public:
	eastl::shared_ptr<FCompiledShader> Bytecode;
	u64 PersistentHash;
	u64 LastChangedVersion;
	FShaderBytecode GetShaderBytecode() const;

	virtual void Compile() {}
	virtual eastl::wstring	GetDebugName() const { return L""; }
	virtual ~FShader() {}
};
DECORATE_CLASS_REF(FShader);

inline u64 GetShaderHash(FShaderRefParam Shader) {
	return Shader->PersistentHash;
}

void RecompileChangedShaders();
u32 GetShadersNum();

FShaderRef GetNullShader();
FShaderRef GetGlobalShader(eastl::string file, eastl::string func, const char* target, std::initializer_list<ShaderMacroPair> macros, u32 flags);
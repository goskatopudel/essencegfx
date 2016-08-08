#pragma once
#include "Essence.h"

#include <EASTL\string.h>
#include <EASTL\vector.h>
#include <EASTL\shared_ptr.h>

#include <d3d12.h>

class FShader;
class FCompiledShader;
typedef eastl::pair<eastl::string, eastl::string> ShaderMacroPair;

class FShader {
public:
	u64										LastChangedVersion;
	eastl::shared_ptr<FCompiledShader>		Bytecode;
	u64										LocationHash;
	eastl::string							File;
	eastl::string							Func;
	const char*								Target;
	eastl::unique_ptr<D3D_SHADER_MACRO[]>	Macros;
	eastl::vector<ShaderMacroPair>			MacrosRaw;
	u32										Flags;

	void Compile();
	eastl::wstring	GetDebugName() const;
};

FShader* GetShader(eastl::string file, eastl::string func, const char* target, std::initializer_list<ShaderMacroPair> macros, u32 flags);
// used to identify source of shader, not it's content (persistent between code changes)
u64 GetShaderHash(FShader const* shader);

D3D12_SHADER_BYTECODE GetBytecode(FShader const*);
// used to dientify shader content
u64 GetBytecodeHash(FShader const* shader);

void RecompileChangedShaders();
u32 GetShadersNum();

constexpr FShader * NO_SHADER = nullptr;
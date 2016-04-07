#pragma once
#include "Essence.h"

#include <EASTL\string.h>
#include <EASTL\vector.h>

#include <d3d12.h>

class FShader;
typedef eastl::pair<eastl::wstring, eastl::wstring> ShaderMacroPair;

FShader* GetShader(eastl::string file, eastl::string func, const char* target, eastl::vector<ShaderMacroPair> macros, u32 flags);
u64 GetShaderHash(FShader const* shader);

D3D12_SHADER_BYTECODE GetBytecode(FShader const*);
u64 GetBytecodeHash(FShader const* shader);
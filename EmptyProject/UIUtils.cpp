#include "UIUtils.h"
#include "imgui\imgui.h"
#include <Psapi.h>
#include "PointerMath.h"
#include "Device.h"
#include "Shader.h"
#include "Pipeline.h"

void ShowMemoryInfo() {
	auto localMemory = GetLocalMemoryInfo();
	auto nonLocalMemory = GetNonLocalMemoryInfo();

	ImGui::BulletText("Device memory");
	ImGui::Indent();
	ImGui::Text("Local memory");
	ImGui::Text("Budget:\nCurrent usage:"); ImGui::SameLine();
	ImGui::Text("%llu Mb\n%llu Mb", Megabytes(localMemory.Budget), Megabytes(localMemory.CurrentUsage));
	ImGui::Text("Non-Local memory");
	ImGui::Text("Budget:\nCurrent usage:"); ImGui::SameLine();
	ImGui::Text("%llu Mb\n%llu Mb", Megabytes(nonLocalMemory.Budget), Megabytes(nonLocalMemory.CurrentUsage));
	ImGui::Unindent();

	ImGui::Separator();

	ImGui::BulletText("Process memory");
	PROCESS_MEMORY_COUNTERS pmc;
	verify(GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)));
	ImGui::Indent();
	ImGui::Text("Working set:\nPagefile:"); ImGui::SameLine();
	ImGui::Text("%llu Mb\n%llu Mb", Megabytes(pmc.WorkingSetSize), Megabytes(pmc.PagefileUsage));
	ImGui::Unindent();

	ImGui::Separator();

	ImGui::BulletText("System memory");
	PERFORMANCE_INFORMATION perfInfo;
	verify(GetPerformanceInfo(&perfInfo, sizeof(perfInfo)));
	ImGui::Indent();
	ImGui::Text("Commited total:\nPhysical total:\nPhysical available:"); ImGui::SameLine();
	ImGui::Text("%llu Mb\n%llu Mb\n%llu Mb"
		, Megabytes(perfInfo.CommitTotal * perfInfo.PageSize)
		, Megabytes(perfInfo.PhysicalTotal * perfInfo.PageSize)
		, Megabytes(perfInfo.PhysicalAvailable * perfInfo.PageSize));
	ImGui::Unindent();
}

void ShowAppStats() {
	ImGui::Begin("Stats");
	if (ImGui::CollapsingHeader("Shaders")) {
		extern u64 GShadersCompilationVersion;

		ImGui::Text("Shaders:\nPSOs:\nCurrent shaders version:"); ImGui::SameLine();
		ImGui::Text("%u\n%u\n%u", GetShadersNum(), GetPSOsNum(), (u32)GShadersCompilationVersion);
	}
	if (ImGui::CollapsingHeader("Memory")) {
		ShowMemoryInfo();
	}
	ImGui::End();
}

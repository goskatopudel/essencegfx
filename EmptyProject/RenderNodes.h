#pragma once
#include "RenderGraph.h"


class FCopyTexture : public FProcessingNode {
public:
	FViewportRect DstRect;
	bool OutputSrgb = true;

	void Process(FCommandsStream & CmdStream) override;
};
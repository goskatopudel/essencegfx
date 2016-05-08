#pragma once
#include "Essence.h"

class FEditorMesh;
class FEditorModel;

void SaveEditorModel(FEditorModel const * Model, const wchar_t * Filename);
void LoadEditorModel(FEditorModel * Model, const wchar_t * Filename);
#pragma once
#include <imgui.h>

namespace kizuri::editor::icons {

void Torii(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);

void Hierarchy(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);

void Viewport(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);

void Inspector(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);

void Console(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);

void Folder(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);

void Play(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);
void Stop(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);
void Move(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);
void Rotate(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);
void Scale(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);
void Maximize(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);
void Settings(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);

using IconFn = void(*)(ImDrawList*, ImVec2, float, ImU32);

void PanelHeader(const char* label, IconFn icon);

}

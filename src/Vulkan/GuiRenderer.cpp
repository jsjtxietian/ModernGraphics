#include "GuiRenderer.h"

void imguiTextureWindow(const char *Title, uint32_t texId)
{
    // The function creates a default window and fills the entire window with the texture's
    // content. We get the minimum and maximum boundaries:
    ImGui::Begin(Title, nullptr);

    ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    ImVec2 vMax = ImGui::GetWindowContentRegionMax();

    // creates a rectangular texture item, which is added to the draw list:
    ImGui::Image((void *)(intptr_t)texId, ImVec2(vMax.x - vMin.x, vMax.y - vMin.y));

    ImGui::End();
}

// If we need to display the contents of a color buffer, we use the call with a texture index.
// If the texture contains depth data, we set the higher 16 bits to the texture index:
// imguiTextureWindow("Some title", textureID);
// imguiTextureWindow("Some depth buffer",
//                    textureID | 0xFFFF);
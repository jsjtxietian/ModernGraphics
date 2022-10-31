#if 0
// Screen Space Ambient Occlusion (SSAO) is an image-based technique to roughly
// approximate global illumination in real time. Ambient occlusion itself is a very crude
// approximation of global illumination. It can be thought of as the amount of open "sky"
// visible from a point on a surface and not occluded by any local adjacent geometry.
// In its simplest form, we can estimate this amount by sampling several points in the
// neighborhood of our point of interest and checking their visibility from the central point.

// you may have noticed that the SSAO effect behaves somewhat
// weirdly on transparent surfaces. That is quite understandable since our transparency
// rendering is done via punch-through transparency whereby a part of transparent surface
// pixels is discarded proportionally to the transparency value. These holes expose the depth
// values beneath the transparent surface, hence our SSAO implementation works partially.
// In a real-world rendering engine, you might want to calculate the SSAO effect after the
// opaque objects have been fully rendered and before any transparent objects influence the
// depth buffer.

// We can now add the SSAO effect to different Vulkan demos just by moving the instance of
// SSAOProcessor. While this might be pretty neat for learning and demonstration, it is far
// from the most performant solution and can be difficult to synchronize in postprocessing
// pipelines that include tens of different effects.

#include "Framework/VulkanApp.h"
#include "Framework/GuiRenderer.h"
#include "Framework/MultiRenderer.h"
#include "Framework/VKQuadRenderer.h"

const char *envMapFile = "data/piazza_bologni_1k.hdr";
const char *irrMapFile = "data/piazza_bologni_1k_irradiance.hdr";

#include "Effects/SSAOProcessor.h"

#include <imgui/imgui_internal.h>

struct MyApp : public CameraApp
{
    MyApp() : CameraApp(-95, -95),
              colorTex(ctx_.resources.addColorTexture()),
              depthTex(ctx_.resources.addDepthTexture()),
              finalTex(ctx_.resources.addColorTexture()),

              sceneData(ctx_, "data/meshes/test.meshes", "data/meshes/test.scene", "data/meshes/test.materials",
                        ctx_.resources.loadCubeMap(envMapFile),
                        ctx_.resources.loadCubeMap(irrMapFile)),

              multiRenderer(ctx_, sceneData, "data/shaders/07/VK01.vert", "data/shaders/07/VK01.frag", {colorTex, depthTex},
                            ctx_.resources.addRenderPass({colorTex, depthTex}, RenderPassCreateInfo{
                                                                                   .clearColor_ = true, .clearDepth_ = true, .flags_ = eRenderPassBit_First | eRenderPassBit_Offscreen})),

              SSAO(ctx_, colorTex, depthTex, finalTex),

              quads(ctx_, {colorTex, finalTex}), imgui(ctx_, {colorTex, SSAO.getBlurY()})
    {
        positioner = CameraPositioner_FirstPerson(glm::vec3(-10.0f, -3.0f, 3.0f), glm::vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, 1.0f, 0.0f));

        onScreenRenderers_.emplace_back(multiRenderer);
        onScreenRenderers_.emplace_back(SSAO);

        onScreenRenderers_.emplace_back(quads, false);
        onScreenRenderers_.emplace_back(imgui, false);
    }

    bool enableSSAO = true;

    void drawUI() override
    {
        ImGui::Begin("Control", nullptr);
        ImGui::Checkbox("Enable SSAO", &enableSSAO);
        // https://github.com/ocornut/imgui/issues/1889#issuecomment-398681105
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !enableSSAO);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * enableSSAO ? 1.0f : 0.2f);

        ImGui::SliderFloat("SSAO scale", &SSAO.params->scale_, 0.0f, 2.0f);
        ImGui::SliderFloat("SSAO bias", &SSAO.params->bias_, 0.0f, 0.3f);
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
        ImGui::Separator();
        ImGui::SliderFloat("SSAO radius", &SSAO.params->radius, 0.05f, 0.5f);
        ImGui::SliderFloat("SSAO attenuation scale", &SSAO.params->attScale, 0.5f, 1.5f);
        ImGui::SliderFloat("SSAO distance scale", &SSAO.params->distScale, 0.0f, 1.0f);
        ImGui::End();

        if (enableSSAO)
        {
            imguiTextureWindow("SSAO", 2);
        }
    }

    void draw3D() override
    {
        multiRenderer.setMatrices(getDefaultProjection(), camera.getViewMatrix());

        quads.clear();
        quads.quad(-1.0f, -1.0f, 1.0f, 1.0f, enableSSAO ? 1 : 0);
    }

private:
    VulkanTexture colorTex, depthTex, finalTex;

    VKSceneData sceneData;

    MultiRenderer multiRenderer;

    SSAOProcessor SSAO;

    QuadRenderer quads;

    GuiRenderer imgui;
};

int main()
{
    MyApp app;
    app.mainLoop();
    return 0;
}

#endif
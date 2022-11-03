#if 0

// https://en.wikipedia.org/wiki/Bloom_(shader_effect)

// Strictly speaking, applying a tone-mapping operator directly to RGB channel values is very
// crude. The more correct model would be to tone-map the luminance and then apply it back
// to RGB values.

// This demo uses an asynchronous texture-loadingapproach. If the calls to the asynchronous
// texture loader are removed, the demo just loads the textures upfront. As a side note,
// Vulkan allows the use of dedicated transfer queues for asynchronous uploading of the
// textures into the GPU, but this requires some more tweaking in the Vulkan device and
// queue-initialization code. We leave this as an exercise to our readers.

#include "Framework/VulkanApp.h"
#include "Framework/GuiRenderer.h"
#include "Framework/MultiRenderer.h"
#include "Framework/VKQuadRenderer.h"
#include "Framework/CubeRenderer.h"
#include "Effects/HDRProcessor.h"

#include "Framework/Barriers.h"

const uint32_t TEX_RGB = (0x2 << 16);

struct MyApp : public CameraApp
{
    MyApp() : CameraApp(-95, -95),
              // https://hdrihaven.com/hdri/?h=immenstadter_horn
              // generated using FilterEnvmap tool
              cubemap(ctx_.resources.loadCubeMap("data/immenstadter_horn_2k.hdr")),
              cubemapIrr(ctx_.resources.loadCubeMap("data/immenstadter_horn_2k_irradiance.hdr")),
              // MultiRenderer requires two input textures, one for color and one for the
              // depth buffer. A 32-bit RGBA output texture for the HDR postprocessor is allocated:
              HDRDepth(ctx_.resources.addDepthTexture()),
              HDRLuminance(ctx_.resources.addColorTexture(0, 0, LuminosityFormat)),
              luminanceResult(ctx_.resources.addColorTexture(1, 1, LuminosityFormat)),

              hdrTex(ctx_.resources.addColorTexture()),

              sceneData(ctx_, "data/meshes/test.meshes", "data/meshes/test.scene", "data/meshes/test.materials",
                        cubemap,
                        cubemapIrr,
                        true),

              cubeRenderer(ctx_,
                           cubemap,
                           {HDRLuminance, HDRDepth},
                           ctx_.resources.addRenderPass({HDRLuminance, HDRDepth}, RenderPassCreateInfo{
                                                                                      .clearColor_ = true, .clearDepth_ = true, .flags_ = eRenderPassBit_First | eRenderPassBit_Offscreen})),
              // The only MultiRenderer instance in this application outputs to previously
              // allocated buffers using the same shaders as in 07
              // A custom offscreen rendering pass compatible with our HDR framebuffers is a
              // required parameter. The constructor body creates a rendering sequence. In order
              // to see anything on the screen, we use the QuadRenderer instance. The upper half
              // of the screen shows an intermediate HDR buffer, and the lower part displays the final image:
              multiRenderer(ctx_, sceneData, "data/shaders/07/VK01.vert", "data/shaders/08/VK03_scene_IBL.frag", {HDRLuminance, HDRDepth}, ctx_.resources.addRenderPass({HDRLuminance, HDRDepth}, RenderPassCreateInfo{.clearColor_ = false, .clearDepth_ = false, .flags_ = eRenderPassBit_Offscreen})),

              // tone mapping (gamma correction / exposure)
              luminance(ctx_, HDRLuminance, luminanceResult),
              // Temporarily we switch between luminances [coming from PingPong light adaptation calculator]
              hdr(ctx_, HDRLuminance, luminanceResult, mappedUniformBufferAttachment(ctx_.resources, &hdrUniforms, VK_SHADER_STAGE_FRAGMENT_BIT)),

              displayedTextureList({hdrTex, HDRLuminance, luminance.getResult64(), luminance.getResult32(), luminance.getResult16(), luminance.getResult08(), luminance.getResult04(), luminance.getResult02(), luminance.getResult01(), // 2 - 9
                                    hdr.getBloom1(), hdr.getBloom2(), hdr.getBrightness(), hdr.getResult(),                                                                                                                              // 10 - 13
                                    HDRLuminance, hdr.getStreaks1(), hdr.getStreaks2(),                                                                                                                                                  // 14 - 16
                                    hdr.getAdaptatedLum1(), hdr.getAdaptatedLum2()}),                                                                                                                                                    // 17 - 18

              quads(ctx_, displayedTextureList), imgui(ctx_, displayedTextureList),

              toDepth(ctx_, HDRDepth), toShader(ctx_, HDRDepth),

              lumToColor(ctx_, HDRLuminance), lumToShader(ctx_, HDRLuminance),

              lumWait(ctx_, HDRLuminance)
    {
        positioner = CameraPositioner_FirstPerson(vec3(-15.81f, -5.18f, -5.81f), vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, 1.0f, 0.0f));

        hdrUniforms->bloomStrength = 1.1f;
        hdrUniforms->maxWhite = 1.17f;
        hdrUniforms->exposure = 0.9f;
        hdrUniforms->adaptationSpeed = 0.1f;

        setVkImageName(ctx_.vkDev, HDRDepth.image.image, "HDRDepth");
        setVkImageName(ctx_.vkDev, HDRLuminance.image.image, "HDRLuminance");
        setVkImageName(ctx_.vkDev, hdrTex.image.image, "hdrTex");
        setVkImageName(ctx_.vkDev, luminanceResult.image.image, "lumRes");

        setVkImageName(ctx_.vkDev, hdr.getBloom1().image.image, "bloom1");
        setVkImageName(ctx_.vkDev, hdr.getBloom2().image.image, "bloom2");
        setVkImageName(ctx_.vkDev, hdr.getBrightness().image.image, "bloomBright");
        setVkImageName(ctx_.vkDev, hdr.getResult().image.image, "bloomResult");
        setVkImageName(ctx_.vkDev, hdr.getStreaks1().image.image, "bloomStreaks1");
        setVkImageName(ctx_.vkDev, hdr.getStreaks2().image.image, "bloomStreaks2");

        onScreenRenderers_.emplace_back(cubeRenderer);

        onScreenRenderers_.emplace_back(toDepth, false);
        onScreenRenderers_.emplace_back(lumToColor, false);

        onScreenRenderers_.emplace_back(multiRenderer);

        onScreenRenderers_.emplace_back(lumWait, false);

        onScreenRenderers_.emplace_back(luminance, false);

        onScreenRenderers_.emplace_back(hdr, false);

        onScreenRenderers_.emplace_back(quads, false);
        onScreenRenderers_.emplace_back(imgui, false);
    }

    bool showPyramid = true;
    bool showDebug = true;
    bool enableToneMapping = true;

    void drawUI() override
    {
        ImGui::Begin("Settings", nullptr);
        ImGui::Text("FPS: %.2f", getFPS());

        ImGui::Checkbox("Enable ToneMapping", &enableToneMapping);
        ImGui::Checkbox("ShowPyramid", &showPyramid);
        ImGui::Checkbox("ShowDebug", &showDebug);

        ImGui::SliderFloat("BloomStrength: ", &hdrUniforms->bloomStrength, 0.1f, 2.0f);
        ImGui::SliderFloat("MaxWhite: ", &hdrUniforms->maxWhite, 0.1f, 2.0f);
        ImGui::SliderFloat("Exposure: ", &hdrUniforms->exposure, 0.1f, 10.0f);
        ImGui::SliderFloat("Adaptation speed: ", &hdrUniforms->adaptationSpeed, 0.01f, 2.0f);
        ImGui::End();

        if (showPyramid)
        {
            ImGui::Begin("Pyramid", nullptr);

            ImGui::Text("HDRColor");
            ImGui::Image((void *)(intptr_t)(2 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::Text("Lum64");
            ImGui::Image((void *)(intptr_t)(3 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::Text("Lum32");
            ImGui::Image((void *)(intptr_t)(4 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::Text("Lum16");
            ImGui::Image((void *)(intptr_t)(5 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::Text("Lum08");
            ImGui::Image((void *)(intptr_t)(6 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::Text("Lum04");
            ImGui::Image((void *)(intptr_t)(7 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::Text("Lum02");
            ImGui::Image((void *)(intptr_t)(8 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::Text("Lum01");
            ImGui::Image((void *)(intptr_t)(9 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::End();
        }

        if (showDebug)
        {
            ImGui::Begin("Adaptation", nullptr);
            ImGui::Text("Adapt1");
            ImGui::Image((void *)(intptr_t)(17 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::Text("Adapt2");
            ImGui::Image((void *)(intptr_t)(18 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::End();

            ImGui::Begin("Debug", nullptr);
            ImGui::Text("Bloom1");
            ImGui::Image((void *)(intptr_t)(10 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::Text("Bloom2");
            ImGui::Image((void *)(intptr_t)(11 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::Text("Bright");
            ImGui::Image((void *)(intptr_t)(12 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::Text("Result");
            ImGui::Image((void *)(intptr_t)(13 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

            ImGui::Text("Streaks1");
            ImGui::Image((void *)(intptr_t)(14 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::Text("Streaks2");
            ImGui::Image((void *)(intptr_t)(15 | TEX_RGB), ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::End();
        }
    }

    void draw3D() override
    {
        const mat4 p = getDefaultProjection();
        const mat4 view = camera.getViewMatrix();
        cubeRenderer.setMatrices(p, view);
        multiRenderer.setMatrices(p, view);

        multiRenderer.checkLoadedTextures();

        quads.clear();
        quads.quad(-1.0f, 1.0f, 1.0f, -1.0f, 12);
    }

private:
    HDRUniformBuffer *hdrUniforms;

    VulkanTexture cubemap;
    VulkanTexture cubemapIrr;

    VulkanTexture HDRDepth;
    VulkanTexture HDRLuminance;
    VulkanTexture luminanceResult;
    VulkanTexture hdrTex;

    VKSceneData sceneData;
    CubemapRenderer cubeRenderer;
    MultiRenderer multiRenderer;

    LuminanceCalculator luminance;
    HDRProcessor hdr;

    std::vector<VulkanTexture> displayedTextureList;

    GuiRenderer imgui;
    QuadRenderer quads;

    ShaderOptimalToDepthBarrier toDepth;
    DepthToShaderOptimalBarrier toShader;

    ShaderOptimalToColorBarrier lumToColor;
    ColorToShaderOptimalBarrier lumToShader;

    ColorWaitBarrier lumWait;
};

int main()
{
    MyApp app;
    app.mainLoop();
    return 0;
}

#endif
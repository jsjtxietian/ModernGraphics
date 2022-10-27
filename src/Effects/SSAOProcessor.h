#pragma once
#include "Framework/CompositeRenderer.h"
#include "Framework/ShaderProcessor.h"
#include "Framework/Barriers.h"

const int SSAOWidth = 0;  // smaller SSAO buffer can be used 512
const int SSAOHeight = 0; // 512;

struct SSAOProcessor : public CompositeRenderer
{
    SSAOProcessor(VulkanRenderContext &ctx,
                  VulkanTexture colorTex,
                  VulkanTexture depthTex,
                  VulkanTexture outputTex) : CompositeRenderer(ctx),

                                             rotateTex(ctx.resources.loadTexture2D("data/rot_texture.bmp")),
                                             SSAOTex(ctx.resources.addColorTexture(SSAOWidth, SSAOHeight)),
                                             SSAOBlurXTex(ctx.resources.addColorTexture(SSAOWidth, SSAOHeight)),
                                             SSAOBlurYTex(ctx.resources.addColorTexture(SSAOWidth, SSAOHeight)),

                                             SSAOParamBuffer(mappedUniformBufferAttachment(ctx.resources, &params, VK_SHADER_STAGE_FRAGMENT_BIT)),

                                             SSAO(ctx, {.buffers = {SSAOParamBuffer}, .textures = {fsTextureAttachment(depthTex), fsTextureAttachment(rotateTex)}},
                                                  {SSAOTex}, "data/shaders/08/VK02_SSAO.frag"),
                                             BlurX(ctx, {.textures = {fsTextureAttachment(SSAOTex)}},
                                                   {SSAOBlurXTex}, "data/shaders/08/VK02_SSAOBlurX.frag"),
                                             BlurY(ctx, {.textures = {fsTextureAttachment(SSAOBlurXTex)}},
                                                   {SSAOBlurYTex}, "data/shaders/08/VK02_SSAOBlurY.frag"),
                                             SSAOFinal(ctx, {.buffers = {SSAOParamBuffer}, .textures = {fsTextureAttachment(colorTex), fsTextureAttachment(SSAOBlurYTex)}},
                                                       {outputTex}, "data/shaders/08/VK02_SSAOFinal.frag"),

                                             ssaoColorToShader(ctx_, SSAOTex),
                                             ssaoShaderToColor(ctx_, SSAOTex),

                                             blurXColorToShader(ctx_, SSAOBlurXTex),
                                             blurXShaderToColor(ctx_, SSAOBlurXTex),

                                             blurYColorToShader(ctx_, SSAOBlurYTex),
                                             blurYShaderToColor(ctx_, SSAOBlurYTex),

                                             finalColorToShader(ctx_, outputTex),
                                             finalShaderToColor(ctx_, outputTex)
    {
        setVkImageName(ctx_.vkDev, rotateTex.image.image, "rotateTex");
        setVkImageName(ctx_.vkDev, SSAOTex.image.image, "SSAO");
        setVkImageName(ctx_.vkDev, SSAOBlurXTex.image.image, "SSAOBlurX");
        setVkImageName(ctx_.vkDev, SSAOBlurYTex.image.image, "SSAOBlurY");

        renderers_.emplace_back(ssaoShaderToColor, false);
        renderers_.emplace_back(blurXShaderToColor, false);
        renderers_.emplace_back(blurYShaderToColor, false);
        renderers_.emplace_back(finalShaderToColor, false);

        // None of these renderers performs depth buffer writes, so the second parameter, useDepth,
        // should be set to false:
        renderers_.emplace_back(SSAO, false);
        renderers_.emplace_back(ssaoColorToShader, false);

        renderers_.emplace_back(BlurX, false);
        renderers_.emplace_back(blurXColorToShader, false);

        renderers_.emplace_back(BlurY, false);
        renderers_.emplace_back(blurYColorToShader, false);

        renderers_.emplace_back(SSAOFinal, false);
        renderers_.emplace_back(finalColorToShader, false);
    }

    // For debugging purposes, we expose access to intermediate textures. 
    inline VulkanTexture getSSAO() const { return SSAOTex; }
    inline VulkanTexture getBlurX() const { return SSAOBlurXTex; }
    inline VulkanTexture getBlurY() const { return SSAOBlurYTex; }

    // The SSAO parameters are chosen arbitrarily and can be tweaked using the ImGui interface:
    struct Params
    {
        float scale_ = 1.0f;
        float bias_ = 0.2f;
        float zNear = 0.1f;
        float zFar = 1000.0f;
        float radius = 0.2f;
        float attScale = 1.0f;
        float distScale = 0.5f;
    } * params;

private:
    // the rotation vectors texture that contains 16 random vec3 vectors. This technique was
    // proposed by Crytek in the early days of real-time SSAO algorithms
    VulkanTexture rotateTex;
    VulkanTexture SSAOTex, SSAOBlurXTex, SSAOBlurYTex;

    BufferAttachment SSAOParamBuffer;

    QuadProcessor SSAO, BlurX, BlurY, SSAOFinal;

    ColorToShaderOptimalBarrier ssaoColorToShader;
    ShaderOptimalToColorBarrier ssaoShaderToColor;

    ColorToShaderOptimalBarrier blurXColorToShader;
    ShaderOptimalToColorBarrier blurXShaderToColor;

    ColorToShaderOptimalBarrier blurYColorToShader;
    ShaderOptimalToColorBarrier blurYShaderToColor;

    ColorToShaderOptimalBarrier finalColorToShader;
    ShaderOptimalToColorBarrier finalShaderToColor;
};

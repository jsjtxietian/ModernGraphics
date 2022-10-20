#if 0
// for simplicity rather than for speed or precision, so it does not use
// importance sampling and convolves the input cube map using simple Monte Carlo
// integration and the Hammersley sequence to generate uniformly distributed 2D points on
// an equirectangular projection of our input cube map.
// read Brian Karis's paper at https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
// http://paulbourke.net/panorama/cubemaps/index.html

// Technically, we should have a separate convolution for each different BRDF. This
// is, however, not practical in terms of storage, memory, and performance on mobile. It is
// wrong but good enough.

#include <imgui/imgui.h>
#include "Framework/VulkanApp.h"

#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
// Image_Resize's implementation is included in UtilsVulkan.cpp
#include "stb_image_resize.h"

int numPoints = 1024;

/// From Henry J. Warren's "Hacker's Delight"
float radicalInverse_VdC(uint32_t bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

// From http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// By definition of the Hammersley point set, the i-th point can be generated using the following function,
vec2 hammersley2d(uint32_t i, uint32_t N)
{
    return vec2(float(i) / float(N), radicalInverse_VdC(i));
}

void convolveDiffuse(const vec3 *data, int srcW, int srcH, int dstW, int dstH, vec3 *output, int numMonteCarloSamples)
{
    // our code supports only equirectangular projections where the width is twice the height of the image.
    // resize the input environment cube map into a smaller image sized dstW x dstH:
    assert(srcW == 2 * srcH);

    if (srcW != 2 * srcH)
        return;

    std::vector<vec3> tmp(dstW * dstH);

    stbir_resize_float_generic(
        reinterpret_cast<const float *>(data), srcW, srcH, 0,
        reinterpret_cast<float *>(tmp.data()), dstW, dstH, 0, 3,
        STBIR_ALPHA_CHANNEL_NONE, 0, STBIR_EDGE_CLAMP, STBIR_FILTER_CUBICBSPLINE, STBIR_COLORSPACE_LINEAR, nullptr);

    const vec3 *scratch = tmp.data();
    srcW = dstW;
    srcH = dstH;

    // Then, we iterate over every pixel of the output cube map. We calculate two vectors,
    // V1 and V2. The first vector, V1, is the direction to the current pixel of the output
    // cube map. The second one, V2, is the direction to a randomly selected pixel of the
    // input cube map:

    for (int y = 0; y != dstH; y++)
    {
        printf("Line %i...\n", y);
        const float theta1 = float(y) / float(dstH) * Math::PI;
        for (int x = 0; x != dstW; x++)
        {
            const float phi1 = float(x) / float(dstW) * Math::TWOPI;
            const vec3 V1 = vec3(sin(theta1) * cos(phi1), sin(theta1) * sin(phi1), cos(theta1));
            vec3 color = vec3(0.0f);
            float weight = 0.0f;
            for (int i = 0; i != numMonteCarloSamples; i++)
            {
                const vec2 h = hammersley2d(i, numMonteCarloSamples);
                const int x1 = int(floor(h.x * srcW));
                const int y1 = int(floor(h.y * srcH));
                const float theta2 = float(y1) / float(srcH) * Math::PI;
                const float phi2 = float(x1) / float(srcW) * Math::TWOPI;
                const vec3 V2 = vec3(sin(theta2) * cos(phi2), sin(theta2) * sin(phi2), cos(theta2));
                // We use the dot product between V1 and V2 to convolve the values of the input cube
                // map. This is done according to the implementation of PrefilterEnvMap()
                // from the following paper: https://cdn2.unrealengine.com/Resources/
                // files/2013SiggraphPresentationsNotes-26915738.pdf. To speed up
                // our CPU-based implementation, we sacrifice some precision by replacing NdotL >
                // 0 from the original paper with 0.01f. The output value is renormalized using the
                // sum of all NdotL weights:
                const float D = std::max(0.0f, glm::dot(V1, V2));
                if (D > 0.01f)
                {
                    color += scratch[y1 * srcW + x1] * D;
                    weight += D;
                }
            }
            output[y * dstW + x] = color / weight;
        }
    }
}

void process_cubemap(const char *filename, const char *outFilename)
{
    int w, h, comp;
    const float *img = stbi_loadf(filename, &w, &h, &comp, 3);

    if (!img)
    {
        printf("Failed to load [%s] texture\n", filename);
        fflush(stdout);
        return;
    }

    const int dstW = 256;
    const int dstH = 128;

    std::vector<vec3> out(dstW * dstH);

    convolveDiffuse((vec3 *)img, w, h, dstW, dstH, out.data(), numPoints);

    stbi_image_free((void *)img);
    stbi_write_hdr(outFilename, dstW, dstH, 3, (float *)out.data());
}

int main()
{
    process_cubemap("data/piazza_bologni_1k.hdr", "data/piazza_bologni_1k_irradiance.hdr");

    return 0;
}

#endif
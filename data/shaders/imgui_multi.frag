// The fragment shader takes the UV texture coordinates and an optional item color as
// input. The only output is the fragment color:

#version 460
#extension GL_EXT_nonuniform_qualifier : require
layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 color;
layout(location = 0) out vec4 outColor;
layout(binding = 3) uniform sampler2D textures[];

// texture index as a push constant
layout(push_constant) uniform pushBlock
{ uint index; } pushConsts;

// we "decode" the passed texture
// index and decide how to interpret this texture's contents before outputting the
// fragment color. The higher 16 bits of the texture index indicate whether the fetched
// texture value should be interpreted as a color or as the depth buffer value
void main() {
    const uint kDepthTextureMask = 0xFFFF;
    uint texType = (pushConsts.index >> 16) & kDepthTextureMask;
    // The actual texture index in the textures array is stored in the lower 16 bits.
    uint tex = pushConsts.index & kDepthTextureMask;
    vec4 value = texture(textures[nonuniformEXT(tex)], uv);
    // If the texture type is a standard font texture or a red-green-blue (RGB) image, we
    // multiply by the color and return. Otherwise, if the texture contains depth values, we
    // output a grayscale value:
    outColor = (texType == 0) ? (color * value) : vec4(value.rrr, 1.0);
}
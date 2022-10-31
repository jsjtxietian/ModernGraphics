/**/
#version 460

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UniformBuffer { float exposure; float maxWhite; float bloomStrength; float adaptationSpeed; } ubo;

// Three texture samplers are required—the main framebuffer with the HDR scene, the
// 1x1 average luminance texture, and the blurred bloom texture. The parameters of
// the HDR tone-mapping function are controlled by ImGui:
layout(binding = 1) uniform sampler2D texScene;
layout(binding = 2) uniform sampler2D texLuminance;
layout(binding = 3) uniform sampler2D texBloom;

// Extended Reinhard tone mapping operator
// The maxWhite value is tweaked to represent the maximal brightness value in the
// scene. Everything brighter than this value will be mapped to 1.0:
vec3 Reinhard2(vec3 x)
{
	return (x * (1.0 + x / (ubo.maxWhite * ubo.maxWhite))) / (1.0 + x);
}

void main()
{
	// After the tone mapping is done, the bloom texture can be added on top of everything
	vec3 color = texture(texScene, uv).rgb;
	vec3 bloom = texture(texBloom, vec2(uv.x, 1.0 - uv.y)).rgb;
	float avgLuminance = texture(texLuminance, vec2(0.5, 0.5)).x;

	float midGray = 0.5;

	color *= ubo.exposure * midGray / (avgLuminance + 0.001);
	color = Reinhard2(color);
	outColor = vec4(color + ubo.bloomStrength * bloom, 1.0);
}

/**/
#version 460

layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform UniformBuffer { uint width; uint height; } ubo;

struct TransparentFragment {
	vec4 color;
	float depth;
	uint next;
};

layout (binding = 1) buffer Heads { uint heads[] ; };

layout (binding = 2) buffer Lists { TransparentFragment fragments[]; };

// Fragment shader inputs contain a texScene texture that provides all the opaque objects in rendered format. 
layout (binding = 3) uniform sampler2D texScene;

void main()
{
#define MAX_FRAGMENTS 64
	TransparentFragment frags[64];

	int numFragments = 0;
	uint headIdx = heads[uint(gl_FragCoord.y) * ubo.width + uint(gl_FragCoord.x)]; // imageLoad(heads, ivec2(gl_FragCoord.xy)).r;
	uint idx = headIdx;

	// copy the linked list for this fragment into an local array
	while (idx != 0xFFFFFFFF && numFragments < MAX_FRAGMENTS)
	{
		frags[numFragments] = fragments[idx];
		numFragments++;
		idx = fragments[idx].next;
	}

	// sort the array by depth by using insertion sort from largest to smallest. This is
	// fast, considering we have a reasonably small number of overlapping fragments
	for (int i = 1; i < numFragments; i++) {
		TransparentFragment toInsert = frags[i];
		uint j = i;
		while (j > 0 && toInsert.depth > frags[j-1].depth) {
			frags[j] = frags[j-1];
			j--;
		}
		frags[j] = toInsert;
	}

	// blend the fragments together. 
	// get the color of the closest non-transparent object from the frame buffer
	vec4 color = texture(texScene, vec2(uv.x, 1.0 - uv.y));

	// traverse the array based on their alpha values, and combine the colors using the alpha channel
	for (int i = 0; i < numFragments; i++)
	{
		// Clamping is necessary to prevent any HDR values leaking into the alpha channel:
		color = mix( color, vec4(frags[i].color), clamp(float(frags[i].color.a), 0.0, 1.0) );
	}

	outColor = vec4(color.xyz, 1.0);
}

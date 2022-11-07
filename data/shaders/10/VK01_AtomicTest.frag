// The fragment shader fills the value buffer with fragment coordinates. To keep
// track of the number of added fragments, we can use the count buffer, which holds
// an integer atomic counter. To enable atomic buffer access instructions, we can set
// the GLSL version to 4.6. The value output buffer contains a fragment index and its
// screen coordinates. The ubo uniform buffer contains the framebuffer dimensions
// for calculating the number of output pixels:

#version 460 core

//layout (early_fragment_tests) in;

layout (set = 0, binding = 0) buffer Atomic { uint count; };

struct node { uint idx; float xx, yy; };

layout (set = 0, binding = 1) buffer Values { node value[]; };

layout (set = 0, binding = 2) uniform UniformBuffer { float width; float height; } ubo;

layout(location = 0) out vec4 outColor;

// calculates the total number of screen pixels and compares our
// current counter with the number of pixels. The gl_HelperInvocation check
// helps us avoid touching the fragments list for any helper invocations of the fragment
// shader. If there is still space in the buffer, we increment the counter atomically and
// write out a new fragment. As we are not actually outputting anything to the frame
// buffer, we can use discard after updating the list

void main()
{
	const uint maxPixels = uint(ubo.width) * uint(ubo.height);

	// Check LinkedListSBO is full
	if (count < maxPixels && gl_HelperInvocation == false)
	{
		uint idx = atomicAdd(count, 1);

		// Exchange new head index and previous head index
		value[idx].idx = idx;
		value[idx].xx  = gl_FragCoord.x;
		value[idx].yy  = gl_FragCoord.y;
	}

	discard;
	outColor = vec4(0.0);	
}

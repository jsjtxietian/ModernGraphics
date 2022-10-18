//
#version 460

layout(location = 0) out vec3 uvw;
layout(location = 1) out vec3 v_worldNormal;
layout(location = 2) out vec4 v_worldPos;

// The matIdx output attribute contains the index of the material that was used in
// the fragment shader. The flat attribute instructs the GPU to avoid interpolating this value
layout(location = 3) out flat uint matIdx;

#include <data/shaders/07/VK01.h>
#include <data/shaders/07/VK01_VertCommon.h>

void main()
{
	// fetch the DrawData typed buffer. Using the per-instance data and
	// local gl_VertexIndex, we will calculate the offset as index data
	DrawData dd = drawDataBuffer.data[gl_BaseInstance];

	uint refIdx = dd.indexOffset + gl_VertexIndex;
	// The vertex index is calculated by adding the global vertex offset for this mesh to the
	// vertex index fetched from the ibo buffer
	ImDrawVert v = sbo.data[ibo.data[refIdx] + dd.vertexOffset];

	// The object-to-world transformation is read directly from transformBuffer using the instance index
	mat4 model = transformBuffer.data[gl_BaseInstance];

	v_worldPos   = model * vec4(v.x, v.y, v.z, 1.0);
	v_worldNormal = transpose(inverse(mat3(model))) * vec3(v.nx, v.ny, v.nz);

	/* Assign shader outputs */
	gl_Position = ubo.proj * ubo.view * v_worldPos;
	matIdx = dd.material;
	uvw = vec3(v.u, v.v, 1.0);
}

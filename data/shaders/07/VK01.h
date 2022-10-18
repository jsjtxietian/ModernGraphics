//

#extension GL_EXT_shader_explicit_arithmetic_types_int64: enable


// contains the memory layout of the per-vertex attributes in the ImDrawVert structure
struct ImDrawVert   { float x, y, z; float u, v; float nx, ny, nz; };

// contains information for rendering a mesh instance with a specific material
// The mesh and material indices represent offsets into GPU buffers
// The level of detail the lod field indicates the relative offset to the vertex data. 
// The indexOffset and vertexOffset fields contain byte offsets into the mesh index and geometry buffers. 
// The transformIndex field stores the index of the global object-to-world-space transformation that's calculated
// by scene graph routines
struct DrawData {
	uint mesh;
	uint material;
	uint lod;
	uint indexOffset;
	uint vertexOffset;
	uint transformIndex;
};

#include <data/shaders/07/MaterialData.h>

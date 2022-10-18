// two per-frame uniforms – the model-view-projection matrix and the camera position in the world space:
layout(binding = 0) uniform  UniformBuffer { mat4 proj; mat4 view; vec4 cameraPos; } ubo;
// the indices and vertices to be in separate buffers
layout(binding = 1) readonly buffer SBO    { ImDrawVert data[]; } sbo;
layout(binding = 2) readonly buffer IBO    { uint   data[]; } ibo;
layout(binding = 3) readonly buffer DrawBO { DrawData data[]; } drawDataBuffer;
layout(binding = 5) readonly buffer XfrmBO { mat4 data[]; } transformBuffer;

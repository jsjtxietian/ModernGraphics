//  Advanced Scenegraph Rendering Pipeline presentation by Markus Tavenrath and Christoph Kubisch from NVIDIA

#pragma once

#include "Renderer.h"
#include "Scene/Scene.h"
#include "Scene/Material.h"
#include "Scene/VtxData.h"

#include <taskflow/taskflow.hpp>

// A single instance of VKSceneData can be
// shared between multiple renderers to simplify multipass rendering techniques
// The input scene contains the linearized scene graph in
// the format of our scene-converter tool. A linear list of packed material data is stored
// in a separate file, which is also written by the SceneConverter tool. Environment
// and irradiance maps are passed from an external context because they can be shared
// with other renderers:

// To conclude the GPU part of VKSceneData, it is important to note that GPU buffer
// handles for node transformations and shape lists are not stored here because different
// renderers and additional processors may alter these buffers—for example, a frustum culler
// may remove some invisible shapes:

// Container of mesh data, material data and scene nodes with transformations
struct VKSceneData
{
	// uses our new resources management scheme to load all the scene data.
	// The mesh file contains vertex and index buffers for all geometry in the scene.
	VKSceneData(VulkanRenderContext &ctx,
				const char *meshFile,
				const char *sceneFile,
				const char *materialFile,
				VulkanTexture envMap,
				VulkanTexture irradianceMap,
				bool asyncLoad = false);

	// Three shared textures come first, which are shared by all the rendering shapes to
	// handle PBR lighting calculations. The list of all textures used in materials is also
	// stored here and used externally by MultiRenderer:
	VulkanTexture envMapIrradiance_;
	VulkanTexture envMap_;
	VulkanTexture brdfLUT_;

	// Shared GPU buffers representing per-object materials and node global
	// transformations are exposed to external classes too:
	VulkanBuffer material_;
	VulkanBuffer transforms_;

	VulkanRenderContext &ctx;

	TextureArrayAttachment allMaterialTextures;

	// Probably the second largest GPU buffer after the array of textures is the mesh
	// geometry buffer. References to its parts are stored here. An internal reference to the
	// Vulkan context is at the end of the GPU-related fields:
	BufferAttachment indexBuffer_;
	BufferAttachment vertexBuffer_;

	MeshData meshData_;

	// local CPU-accessible scene, material, and mesh data arrays:
	Scene scene_;
	std::vector<MaterialDescription> materials_;

	// a shapes list and global transformations for each shape.
	// This is done because the list of scene nodes does not
	// map one-to-one shapes due to the invisibility of some nodes or due to the absence
	// of attached node geometry:
	std::vector<glm::mat4> shapeTransforms_;

	std::vector<DrawData> shapes_;

	void loadScene(const char *sceneFile);
	void loadMeshes(const char *meshFile);

	void convertGlobalToShapeTransforms();
	void recalculateAllTransforms();
	void uploadGlobalTransforms();

	void updateMaterial(int matIdx);

	/* async loading */
	struct LoadedImageData
	{
		int index_ = 0;
		int w_ = 0;
		int h_ = 0;
		const uint8_t *img_ = nullptr;
	};

	std::vector<std::string> textureFiles_;
	std::vector<LoadedImageData> loadedFiles_;
	std::mutex loadedFilesMutex_;

private:
	tf::Taskflow taskflow_;
	tf::Executor executor_;
};

constexpr const char *DefaultMeshVertexShader = "data/shaders/07/VK01.vert";
constexpr const char *DefaultMeshFragmentShader = "data/shaders/07/VK01.frag";

struct MultiRenderer : public Renderer
{
	// resembles all the renderers in our new framework and
	// takes as input references to VulkanRenderContext and VKSceneData
	// and a list of output textures for offscreen rendering.
	// Custom render passes may be required for depth-only rendering in shadow
	// mapping or for average lighting calculations:
	MultiRenderer(
		VulkanRenderContext &ctx,
		VKSceneData &sceneData,
		const char *vtxShaderFile = DefaultMeshVertexShader,
		const char *fragShaderFile = DefaultMeshFragmentShader,
		const std::vector<VulkanTexture> &outputs = std::vector<VulkanTexture>{},
		RenderPass screenRenderPass = RenderPass(),
		const std::vector<BufferAttachment> &auxBuffers = std::vector<BufferAttachment>{},
		const std::vector<TextureAttachment> &auxTextures = std::vector<TextureAttachment>{});

	void fillCommandBuffer(VkCommandBuffer cmdBuffer, size_t currentImage, VkFramebuffer fb = VK_NULL_HANDLE, VkRenderPass rp = VK_NULL_HANDLE) override;
	void updateBuffers(size_t currentImage) override;

	void updateIndirectBuffers(size_t currentImage, bool *visibility = nullptr);

	// stores the camera matrices for later uploading to the GPU uniform buffer:
	inline void setMatrices(const glm::mat4 &proj, const glm::mat4 &view)
	{
		const glm::mat4 m1 = glm::scale(glm::mat4(1.f), glm::vec3(1.f, -1.f, 1.f));
		ubo_.proj_ = proj;
		ubo_.view_ = view * m1;
	}

	// stores a local copy of the current camera position for lighting calculations:
	inline void setCameraPosition(const glm::vec3 &cameraPos)
	{
		ubo_.cameraPos_ = glm::vec4(cameraPos, 1.0f);
	}

	inline const VKSceneData &getSceneData() const { return sceneData_; }

	// Async loading in Chapter9
	bool checkLoadedTextures();

private:
	//  starts with a reference to the VKSceneData object.
	// The only GPU data we use in this class is a list of indirect
	// drawing commands and the shapes list, one for each image in the swapchain.
	// The last few fields contain local copies of the camera-related matrices and
	// camera-position vector:
	VKSceneData &sceneData_;

	std::vector<VulkanBuffer> indirect_;
	std::vector<VulkanBuffer> shape_;

	struct UBO
	{
		mat4 proj_;
		mat4 view_;
		vec4 cameraPos_;
	} ubo_;
};

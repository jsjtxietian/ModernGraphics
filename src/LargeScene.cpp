// see Scene data sceheme.ong
// The VKSceneData class loads the geometry data for all the scene objects, a list of
// material parameters, and an array of textures, referenced by individual materials. All the
// loaded data is transferred into the appropriate GPU buffers. The MultiRenderer class
// maintains the Shape and Transform lists in dedicated GPU buffers. Internally, the Shape
// List points to individual items in the Material and Transform lists, and it also holds offsets
// to the index and vertex data in the Mesh geometry buffer. At each frame, the VulkanApp
// class asks MultiRenderer to fill the command buffer with indirect draw commands to
// render the shapes of the scene. The parameters of the indirect draw command are taken
// directly from the Shape list.

#include "Framework/VulkanApp.h"
#include "Framework/GuiRenderer.h"
#include "Framework/MultiRenderer.h"

struct MyApp : public CameraApp
{
    MyApp()
        // create a window that takes up 95% of our screen space
        : CameraApp(-95, -95),
          // Two environment maps are loaded for PBR lighting:
          envMap(ctx_.resources.loadCubeMap("data/piazza_bologni_1k.hdr")),
          irrMap(ctx_.resources.loadCubeMap("data/piazza_bologni_1k_irradiance.hdr")),
          sceneData(ctx_, "data/meshes/test.meshes", "data/meshes/test.scene", "data/meshes/test.materials", envMap, irrMap),
          sceneData2(ctx_, "data/meshes/test2.meshes", "data/meshes/test2.scene", "data/meshes/test2.materials", envMap, irrMap),
          multiRenderer(ctx_, sceneData),
          multiRenderer2(ctx_, sceneData2),
          imgui(ctx_)
    {
        positioner = CameraPositioner_FirstPerson(glm::vec3(-10.0f, -3.0f, 3.0f), glm::vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, 1.0f, 0.0f));

        onScreenRenderers_.emplace_back(multiRenderer);
        onScreenRenderers_.emplace_back(multiRenderer2);
        onScreenRenderers_.emplace_back(imgui, false);
    }

    void draw3D() override
    {
        const mat4 p = getDefaultProjection();
        const mat4 view = camera.getViewMatrix();

        //  passes the current camera parameters to both scene renderers
        multiRenderer.setMatrices(p, view);
        multiRenderer2.setMatrices(p, view);

        multiRenderer.setCameraPosition(positioner.getPosition());
        multiRenderer2.setCameraPosition(positioner.getPosition());
    }

private:
    VulkanTexture envMap;
    VulkanTexture irrMap;

    VKSceneData sceneData;
    VKSceneData sceneData2;

    MultiRenderer multiRenderer;
    MultiRenderer multiRenderer2;
    GuiRenderer imgui;
};

int main()
{
    MyApp app;
    app.mainLoop();
    return 0;
}

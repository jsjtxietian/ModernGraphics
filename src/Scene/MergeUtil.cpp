#include "Material.h"
#include "MergeUtil.h"

#include <algorithm>
#include <map>

// calculating the number of merged indices. We remember the starting vertex offset for all of the
// meshes. The loop shifts all the indices in individual mesh blocks of the meshData.
// indexData_ array. Also, for each Mesh object, a new minVtxOffset value is
// assigned to the vertex-data offset field. The return value is the difference between
// the original and merged index count. This difference is also the offset to the point
// where the merged index data starts
static uint32_t shiftMeshIndices(MeshData &meshData, const std::vector<uint32_t> &meshesToMerge)
{
    auto minVtxOffset = std::numeric_limits<uint32_t>::max();
    for (auto i : meshesToMerge)
        minVtxOffset = std::min(meshData.meshes_[i].vertexOffset, minVtxOffset);

    auto mergeCount = 0u; // calculated by summing index counts in meshesToMerge

    // now shift all the indices in individual index blocks [use minVtxOffset]
    for (auto i : meshesToMerge)
    {
        auto &m = meshData.meshes_[i];
        // for how much should we shift the indices in mesh [m]
        const uint32_t delta = m.vertexOffset - minVtxOffset;

        const auto idxCount = m.getLODIndicesCount(0);
        for (auto ii = 0u; ii < idxCount; ii++)
            meshData.indexData_[m.indexOffset + ii] += delta;

        m.vertexOffset = minVtxOffset;

        // sum all the deleted meshes' indices
        mergeCount += idxCount;
    }

    return meshData.indexData_.size() - mergeCount;
}

// copies indices for each mesh into the newIndices array:
// All the meshesToMerge now have the same vertexOffset and individual index values are shifted by appropriate amount
// Here we move all the indices to appropriate places in the new index array
static void mergeIndexArray(MeshData &md, const std::vector<uint32_t> &meshesToMerge, std::map<uint32_t, uint32_t> &oldToNew)
{
    std::vector<uint32_t> newIndices(md.indexData_.size());
    // Two offsets in the new indices array (one begins at the start, the second one after all the copied indices)
    uint32_t copyOffset = 0,
             mergeOffset = shiftMeshIndices(md, meshesToMerge);

    // For each mesh, we decide where to copy its index data. The copyOffset value is used
    // for meshes that are not merged, and the mergeOffset value starts at the beginning
    // of the merged index data returned by the shiftMeshIndices() function.

    // Two variables contain mesh indices of the merged mesh and the copied mesh. We
    // iterate all the meshes to check whether the current one needs to be merged:
    const auto mergedMeshIndex = md.meshes_.size() - meshesToMerge.size();
    auto newIndex = 0u;
    for (auto midx = 0u; midx < md.meshes_.size(); midx++)
    {
        const bool shouldMerge = std::binary_search(meshesToMerge.begin(), meshesToMerge.end(), midx);

        // Each index is stored in an old-to-new correspondence map
        oldToNew[midx] = shouldMerge ? mergedMeshIndex : newIndex;
        newIndex += shouldMerge ? 0 : 1;

        // The offset of the index block for this mesh is modified, so first calculate the source
        // offset for the index data:
        auto &mesh = md.meshes_[midx];
        auto idxCount = mesh.getLODIndicesCount(0);
        // move all indices to the new array at mergeOffset
        const auto start = md.indexData_.begin() + mesh.indexOffset;
        mesh.indexOffset = copyOffset;
        // We choose between two offsets and copy index data from the original array to the
        // output. The new index array is copied into the mesh data structure:
        const auto offsetPtr = shouldMerge ? &mergeOffset : &copyOffset;
        std::copy(start, start + idxCount, newIndices.begin() + *offsetPtr);
        *offsetPtr += idxCount;
    }

    md.indexData_ = newIndices;

    // all the merged indices are now in lastMesh
    // One last step in the merge process is the creation of a merged mesh. Copy the first of
    // the merged mesh descriptors and assign new Lateral Offset Device (LOD) offsets:
    Mesh lastMesh = md.meshes_[meshesToMerge[0]];
    lastMesh.indexOffset = copyOffset;
    lastMesh.lodOffset[0] = copyOffset;
    lastMesh.lodOffset[1] = mergeOffset;
    lastMesh.lodCount = 1;
    md.meshes_.push_back(lastMesh);
}

// combine multiple meshes into one and delete scene nodes referring to merged meshes.
// The merging of mesh data requires only the index-data modification

// The mergeScene() routine omits a couple of important things. First, we merge only
// the finest LOD level. For our purpose, this is sufficient because our scene contains a large
// amount of simple (one to two triangles) meshes with only a single LOD. Second, we assume
// that the merged meshes have the same transformation. This is also the case for our test
// scene, but if correct transformation is necessary, all the vertices should be transformed into
// the global coordinate system and then transformed back to the local coordinates of the
// node where we place the resulting merged mesh.

void mergeScene(Scene &scene, MeshData &meshData, const std::string &materialName)
{
    // Find material index
    // To avoid string comparisons, convert material names to their indices in the material name array:
    int oldMaterial = (int)std::distance(std::begin(scene.materialNames_), std::find(std::begin(scene.materialNames_), std::end(scene.materialNames_), materialName));

    // When you have the material index, collect all the scene nodes that will be deleted:
    std::vector<uint32_t> toDelete;
    for (auto i = 0u; i < scene.hierarchy_.size(); i++)
        if (scene.meshes_.contains(i) && scene.materialForNode_.contains(i) && (scene.materialForNode_.at(i) == oldMaterial))
            toDelete.push_back(i);

    // The number of meshes to be merged is the same as the number of deleted scene
    // nodes (in our scene, at least), so convert scene-node indices into mesh indices:
    std::vector<uint32_t> meshesToMerge(toDelete.size());

    // Convert toDelete indices to mesh indices
    std::transform(toDelete.begin(), toDelete.end(), meshesToMerge.begin(), [&scene](uint32_t i)
                   { return scene.meshes_.at(i); });

    // TODO: if merged mesh transforms are non-zero, then we should pre-transform individual mesh vertices in meshData using local transform

    // old-to-new mesh indices
    // merges index data and assigns changed mesh indices to scene nodes
    std::map<uint32_t, uint32_t> oldToNew;

    // now move all the meshesToMerge to the end of array
    mergeIndexArray(meshData, meshesToMerge, oldToNew);

    // cutoff all but one of the merged meshes (insert the last saved mesh from meshesToMerge - they are all the same)
    // cut out all the merged meshes and attach a new node containing the merged meshes to the scene graph:
    eraseSelected(meshData.meshes_, meshesToMerge);

    for (auto &n : scene.meshes_)
        n.second = oldToNew[n.second];

    // reattach the node with merged meshes [identity transforms are assumed]
    int newNode = addNode(scene, 0, 1);
    scene.meshes_[newNode] = meshData.meshes_.size() - 1;
    scene.materialForNode_[newNode] = (uint32_t)oldMaterial;

    deleteSceneNodes(scene, toDelete);
}

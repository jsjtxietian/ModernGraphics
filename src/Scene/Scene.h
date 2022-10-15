#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

using glm::mat4;

// we do not define std::vector<Node*> Children - this is already present in the aiNode from assimp
constexpr const int MAX_NODE_LEVEL = 16;

// Left Child – Right Sibling tree representation
// The local and global transforms are also stored in separate arrays and can be easily
// mapped to a GPU buffer without conversion, making them directly accessible from GLSL shaders
struct Hierarchy
{
    // parent for this node (or -1 for root)
    int parent_;
    // first child for a node (or -1)
    int firstChild_;
    // next sibling for a node (or -1)
    int nextSibling_;
    // last added node (or -1)
    int lastSibling_;
    // cached node level
    int level_;
};

/* This scene is converted into a descriptorSet(s) in MultiRenderer class
   This structure is also used as a storage type in SceneExporter tool
 */
struct Scene
{
    // local transformations for each node and global transforms
    // + an array of 'dirty/changed' local transforms
    std::vector<mat4> localTransform_;
    std::vector<mat4> globalTransform_;

    // list of nodes whose global transform must be recalculated
    std::vector<int> changedAtThisFrame_[MAX_NODE_LEVEL];

    // Hierarchy component
    std::vector<Hierarchy> hierarchy_;

    // Mesh component: Which node corresponds to which node
    std::unordered_map<uint32_t, uint32_t> meshes_;

    // Material component: Which material belongs to which node
    std::unordered_map<uint32_t, uint32_t> materialForNode_;

    // Node name component: Which name is assigned to the node
    std::unordered_map<uint32_t, uint32_t> nameForNode_;

    // List of scene node names
    std::vector<std::string> names_;

    // Debug list of material names
    std::vector<std::string> materialNames_;
};
#include "Scene.h"
#include "Utils/Utils.h"

#include <algorithm>
#include <numeric>

void saveStringList(FILE *f, const std::vector<std::string> &lines);
void loadStringList(FILE *f, std::vector<std::string> &lines);

// allocates a new scene node and adds it to the scene hierarchy
int addNode(Scene &scene, int parent, int level)
{
    // First, the addition process acquires a new node identifier, which is the current size
    // of the hierarchy array. New identity transforms are added to the local and global
    // transform arrays. The hierarchy for the newly added node only consists of the parent reference:
    int node = (int)scene.hierarchy_.size();
    {
        // TODO: resize aux arrays (local/global etc.)
        scene.localTransform_.push_back(glm::mat4(1.0f));
        scene.globalTransform_.push_back(glm::mat4(1.0f));
    }
    scene.hierarchy_.push_back({.parent_ = parent, .lastSibling_ = -1});

    // If we have a parent, we must fix its first child reference and, potentially, the next
    // sibling reference of some other node. If a parent node has no children, we must
    // directly set its firstChild_ field; otherwise, we should run over the siblings of
    // this child to find out where to add the next sibling:
    if (parent > -1)
    {
        // find first item (sibling)
        int s = scene.hierarchy_[parent].firstChild_;
        if (s == -1)
        {
            scene.hierarchy_[parent].firstChild_ = node;
            scene.hierarchy_[node].lastSibling_ = node;
        }
        else
        {
            int dest = scene.hierarchy_[s].lastSibling_;
            if (dest <= -1)
            {
                // no cached lastSibling, iterate nextSibling indices
                // After the for loop, we assign our new node as the next sibling of the last child. Note
                // that this linear run over the siblings is not really necessary if we store the index of the
                // last child node that was added.
                for (dest = s; scene.hierarchy_[dest].nextSibling_ != -1; dest = scene.hierarchy_[dest].nextSibling_)
                    ;
            }
            scene.hierarchy_[dest].nextSibling_ = node;
            scene.hierarchy_[s].lastSibling_ = node;
        }
    }

    // The level of this node is stored for correct global transformation updating. To keep
    // the structure valid, we will store the negative indices for the newly added node:
    scene.hierarchy_[node].level_ = level;
    scene.hierarchy_[node].nextSibling_ = -1;
    scene.hierarchy_[node].firstChild_ = -1;
    return node;
}

// starts with a given node and recursively descends
// to each and every child node, adding it to the changedAtLevel_ arrays
void markAsChanged(Scene &scene, int node)
{
    int level = scene.hierarchy_[node].level_;
    scene.changedAtThisFrame_[level].push_back(node);

    // TODO: use non-recursive iteration with aux stack
    for (int s = scene.hierarchy_[node].firstChild_; s != -1; s = scene.hierarchy_[s].nextSibling_)
        markAsChanged(scene, s);
}

int findNodeByName(const Scene &scene, const std::string &name)
{
    // Extremely simple linear search without any hierarchy reference
    // To support DFS/BFS searches separate traversal routines are needed

    for (size_t i = 0; i < scene.localTransform_.size(); i++)
        if (scene.nameForNode_.contains(i))
        {
            int strID = scene.nameForNode_.at(i);
            if (strID > -1)
                if (scene.names_[strID] == name)
                    return (int)i;
        }

    return -1;
}

int getNodeLevel(const Scene &scene, int n)
{
    int level = -1;
    for (int p = 0; p != -1; p = scene.hierarchy_[p].parent_, level++)
        ;
    return level;
}

bool mat4IsIdentity(const glm::mat4 &m);
void fprintfMat4(FILE *f, const glm::mat4 &m);

// Depending on how frequently local transformations are updated, it may be
// more performant to eliminate the list of recently updated nodes and always
// perform a full update. Profile your real code

// CPU version of global transform update []
void recalculateGlobalTransforms(Scene &scene)
{
    // start from the root layer of the list of changed scene nodes, supposing we have
    // only one root node. This is because root node global transforms coincide with their
    // local transforms. The changed nodes list is then cleared
    if (!scene.changedAtThisFrame_[0].empty())
    {
        int c = scene.changedAtThisFrame_[0][0];
        scene.globalTransform_[c] = scene.localTransform_[c];
        scene.changedAtThisFrame_[0].clear();
    }

    // ensure that we have parents so that the loops are
    // linear and there are no conditions inside. We will start from level 1 because the root
    // level is already being handled
    for (int i = 1; i < MAX_NODE_LEVEL && (!scene.changedAtThisFrame_[i].empty()); i++)
    {
        // iterate all the changed nodes at this level
        for (const int &c : scene.changedAtThisFrame_[i])
        {
            int p = scene.hierarchy_[c].parent_;
            scene.globalTransform_[c] = scene.globalTransform_[p] * scene.localTransform_[c];
        }
        scene.changedAtThisFrame_[i].clear();
    }
}

void loadMap(FILE *f, std::unordered_map<uint32_t, uint32_t> &map)
{
    // the count of {key, value} pairs is read from a file
    std::vector<uint32_t> ms;

    uint32_t sz = 0;
    fread(&sz, 1, sizeof(sz), f);

    // all the key-value pairs are loaded with a single fread call:
    ms.resize(sz);
    fread(ms.data(), sizeof(int), sz, f);

    // the array is converted into a hash table:
    for (size_t i = 0; i < (sz / 2); i++)
        map[ms[i * 2 + 0]] = ms[i * 2 + 1];
}

void loadScene(const char *fileName, Scene &scene)
{
    FILE *f = fopen(fileName, "rb");

    if (!f)
    {
        printf("Cannot open scene file '%s'. Please run SceneConverter or MergeMeshes", fileName);
        return;
    }

    uint32_t sz = 0;
    fread(&sz, sizeof(sz), 1, f);

    scene.hierarchy_.resize(sz);
    scene.globalTransform_.resize(sz);
    scene.localTransform_.resize(sz);
    // TODO: check > -1
    // TODO: recalculate changedAtThisLevel() - find max depth of a node [or save scene.maxLevel]
    // fread() reads the transformations and hierarchical data for all the scene nodes:
    fread(scene.localTransform_.data(), sizeof(glm::mat4), sz, f);
    fread(scene.globalTransform_.data(), sizeof(glm::mat4), sz, f);
    fread(scene.hierarchy_.data(), sizeof(Hierarchy), sz, f);

    // Mesh for node [index to some list of buffers]
    // Node-to-material and node-to-mesh mappings are loaded with the calls to the
    // loadMap() helper routine:
    loadMap(f, scene.materialForNode_);
    loadMap(f, scene.meshes_);

    // If there is still some data left, we must read the scene node names and material names:
    if (!feof(f))
    {
        loadMap(f, scene.nameForNode_);
        loadStringList(f, scene.names_);

        loadStringList(f, scene.materialNames_);
    }

    fclose(f);
}

void saveMap(FILE *f, const std::unordered_map<uint32_t, uint32_t> &map)
{
    // A temporary {key, value} pair array is allocated:
    std::vector<uint32_t> ms;
    ms.reserve(map.size() * 2);

    // All the values from std::unordered_map are copied to the array:
    for (const auto &m : map)
    {
        ms.push_back(m.first);
        ms.push_back(m.second);
    }

    const uint32_t sz = static_cast<uint32_t>(ms.size());
    // The count of {key, value} pairs is written to the file:
    fwrite(&sz, sizeof(sz), 1, f);
    // the {key, value} pairs are written with one fwrite() call:
    fwrite(ms.data(), sizeof(int), ms.size(), f);
}

// write the count of scene nodes:
void saveScene(const char *fileName, const Scene &scene)
{
    FILE *f = fopen(fileName, "wb");

    const uint32_t sz = (uint32_t)scene.hierarchy_.size();
    fwrite(&sz, sizeof(sz), 1, f);

    // Three fwrite() calls save the local and global transformations, followed by the hierarchical information
    fwrite(scene.localTransform_.data(), sizeof(glm::mat4), sz, f);
    fwrite(scene.globalTransform_.data(), sizeof(glm::mat4), sz, f);
    fwrite(scene.hierarchy_.data(), sizeof(Hierarchy), sz, f);

    // Mesh for node [index to some list of buffers]
    // store the node-to-materials and node-to-mesh mappings:
    saveMap(f, scene.materialForNode_);
    saveMap(f, scene.meshes_);

    if (!scene.names_.empty() && !scene.nameForNode_.empty())
    {
        saveMap(f, scene.nameForNode_);
        saveStringList(f, scene.names_);

        saveStringList(f, scene.materialNames_);
    }
    fclose(f);
}

bool mat4IsIdentity(const glm::mat4 &m)
{
    return (m[0][0] == 1 && m[0][1] == 0 && m[0][2] == 0 && m[0][3] == 0 &&
            m[1][0] == 0 && m[1][1] == 1 && m[1][2] == 0 && m[1][3] == 0 &&
            m[2][0] == 0 && m[2][1] == 0 && m[2][2] == 1 && m[2][3] == 0 &&
            m[3][0] == 0 && m[3][1] == 0 && m[3][2] == 0 && m[3][3] == 1);
}

void fprintfMat4(FILE *f, const glm::mat4 &m)
{
    if (mat4IsIdentity(m))
    {
        fprintf(f, "Identity\n");
    }
    else
    {
        fprintf(f, "\n");
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
                fprintf(f, "%f ;", m[i][j]);
            fprintf(f, "\n");
        }
    }
}

void dumpTransforms(const char *fileName, const Scene &scene)
{
    FILE *f = fopen(fileName, "a+");
    for (size_t i = 0; i < scene.localTransform_.size(); i++)
    {
        fprintf(f, "Node[%d].localTransform: ", (int)i);
        fprintfMat4(f, scene.localTransform_[i]);
        fprintf(f, "Node[%d].globalTransform: ", (int)i);
        fprintfMat4(f, scene.globalTransform_[i]);
        fprintf(f, "Node[%d].globalDet = %f; localDet = %f\n", (int)i, glm::determinant(scene.globalTransform_[i]), glm::determinant(scene.localTransform_[i]));
    }
    fclose(f);
}

void printChangedNodes(const Scene &scene)
{
    for (int i = 0; i < MAX_NODE_LEVEL && (!scene.changedAtThisFrame_[i].empty()); i++)
    {
        printf("Changed at level(%d):\n", i);

        for (const int &c : scene.changedAtThisFrame_[i])
        {
            int p = scene.hierarchy_[c].parent_;
            // scene.globalTransform_[c] = scene.globalTransform_[p] * scene.localTransform_[c];
            printf(" Node %d. Parent = %d; LocalTransform: ", c, p);
            fprintfMat4(stdout, scene.localTransform_[i]);
            if (p > -1)
            {
                printf(" ParentGlobalTransform: ");
                fprintfMat4(stdout, scene.globalTransform_[p]);
            }
        }
    }
}

// Shift all hierarchy components in the nodes
// increments individual fields of the Hierarchy structure by the given amount:
void shiftNodes(Scene &scene, int startOffset, int nodeCount, int shiftAmount)
{
    auto shiftNode = [shiftAmount](Hierarchy &node)
    {
        if (node.parent_ > -1)
            node.parent_ += shiftAmount;
        if (node.firstChild_ > -1)
            node.firstChild_ += shiftAmount;
        if (node.nextSibling_ > -1)
            node.nextSibling_ += shiftAmount;
        if (node.lastSibling_ > -1)
            node.lastSibling_ += shiftAmount;
        // node->level_ does not have to be shifted
    };

    // If there are too many nodes, we can use std::execution::par with std::transform
    //	std::transform(scene.hierarchy_.begin() + startOffset, scene.hierarchy_.begin() + nodeCount, scene.hierarchy_.begin() + startOffset, shiftNode);

    //	for (auto i = scene.hierarchy_.begin() + startOffset ; i != scene.hierarchy_.begin() + nodeCount ; i++)
    //		shiftNode(*i);

    for (int i = 0; i < nodeCount; i++)
        shiftNode(scene.hierarchy_[i + startOffset]);
}

using ItemMap = std::unordered_map<uint32_t, uint32_t>;

// Add the items from otherMap shifting indices and values along the way
// adds the otherMap collection to the m output map and shifts item indices by specified amounts:
void mergeMaps(ItemMap &m, const ItemMap &otherMap, int indexOffset, int itemOffset)
{
    for (const auto &i : otherMap)
        m[i.first + indexOffset] = i.second + itemOffset;
}

/**
    There are different use cases for scene merging.
    The simplest one is the direct "gluing" of multiple scenes into one [all the material lists and mesh lists are merged and indices in all scene nodes are shifted appropriately]
    The second one is creating a "grid" of objects (or scenes) with the same material and mesh sets.
    For the second use case we need two flags: 'mergeMeshes' and 'mergeMaterials' to avoid shifting mesh indices
*/
// creates a new scene node named "NewRoot"
// and adds all the root scene nodes from the list to the new scene as children of the
// "NewRoot" node. In the accompanying source-code bundle, this routine has
// two more parameters, mergeMeshes and mergeMaterials, which allow the
// creation of composite scenes with shared mesh and material data. We omit these
// non-essential parameters to shorten the description:

void mergeScenes(Scene &scene, const std::vector<Scene *> &scenes,
                 const std::vector<glm::mat4> &rootTransforms, const std::vector<uint32_t> &meshCounts,
                 bool mergeMeshes, bool mergeMaterials)
{
    // Create new root node
    scene.hierarchy_ = {
        {.parent_ = -1,
         .firstChild_ = 1,
         .nextSibling_ = -1,
         .lastSibling_ = -1,
         .level_ = 0}};

    // Name and transform arrays initially contain a single element, "NewRoot":
    scene.nameForNode_[0] = 0;
    scene.names_ = {"NewRoot"};

    scene.localTransform_.push_back(glm::mat4(1.f));
    scene.globalTransform_.push_back(glm::mat4(1.f));

    if (scenes.empty())
        return;

    // While iterating the scenes, we merge and shift all the arrays and maps. The next few
    // variables keep track of item counts in the output scene:
    int offs = 1;
    int meshOffs = 0;
    int nameOffs = (int)scene.names_.size();
    int materialOfs = 0;
    auto meshCount = meshCounts.begin();

    if (!mergeMaterials)
        scene.materialNames_ = scenes[0]->materialNames_;

    // This implementation is not the best possible one, not least because we risk merging
    // all the scene-graph components in a single routine:
    // FIXME: too much logic (for all the components in a scene, though mesh data and materials go separately - there are dedicated data lists)
    for (const Scene *s : scenes)
    {
        mergeVectors(scene.localTransform_, s->localTransform_);
        mergeVectors(scene.globalTransform_, s->globalTransform_);

        mergeVectors(scene.hierarchy_, s->hierarchy_);

        mergeVectors(scene.names_, s->names_);
        if (mergeMaterials)
            mergeVectors(scene.materialNames_, s->materialNames_);

        int nodeCount = (int)s->hierarchy_.size();

        shiftNodes(scene, offs, nodeCount, offs);

        mergeMaps(scene.meshes_, s->meshes_, offs, mergeMeshes ? meshOffs : 0);
        mergeMaps(scene.materialForNode_, s->materialForNode_, offs, mergeMaterials ? materialOfs : 0);
        mergeMaps(scene.nameForNode_, s->nameForNode_, offs, nameOffs);

        // At each iteration, we add the sizes of the current arrays to global offsets:
        offs += nodeCount;

        materialOfs += (int)s->materialNames_.size();
        nameOffs += (int)s->names_.size();

        if (mergeMeshes)
        {
            meshOffs += *meshCount;
            meshCount++;
        }
    }

    // Logically, the routine is complete, but there is one more step to perform. Each scene
    // node contains a cached index of the last sibling node, which we have to set for the
    // new root nodes. Each root node can now have a new local transform, which we set
    // in the following loop:
    // fixing 'nextSibling' fields in the old roots (zero-index in all the scenes)
    offs = 1;
    int idx = 0;
    for (const Scene *s : scenes)
    {
        int nodeCount = (int)s->hierarchy_.size();
        bool isLast = (idx == scenes.size() - 1);
        // calculate new next sibling for the old scene roots
        int next = isLast ? -1 : offs + nodeCount;
        scene.hierarchy_[offs].nextSibling_ = next;
        // attach to new root
        scene.hierarchy_[offs].parent_ = 0;

        // transform old root nodes, if the transforms are given
        if (!rootTransforms.empty())
            scene.localTransform_[offs] = rootTransforms[idx] * scene.localTransform_[offs];

        offs += nodeCount;
        idx++;
    }

    // At the end of the routine, we should increment all the levels of the scene nodes but
    // leave the "NewRoot" node untouched—hence, +1:
    // now shift levels of all nodes below the root
    for (auto i = scene.hierarchy_.begin() + 1; i != scene.hierarchy_.end(); i++)
        i->level_++;
}

void dumpSceneToDot(const char *fileName, const Scene &scene, int *visited)
{
    FILE *f = fopen(fileName, "w");
    fprintf(f, "digraph G\n{\n");
    for (size_t i = 0; i < scene.globalTransform_.size(); i++)
    {
        std::string name = "";
        std::string extra = "";
        if (scene.nameForNode_.contains(i))
        {
            int strID = scene.nameForNode_.at(i);
            name = scene.names_[strID];
        }
        if (visited)
        {
            if (visited[i])
                extra = ", color = red";
        }
        fprintf(f, "n%d [label=\"%s\" %s]\n", (int)i, name.c_str(), extra.c_str());
    }
    for (size_t i = 0; i < scene.hierarchy_.size(); i++)
    {
        int p = scene.hierarchy_[i].parent_;
        if (p > -1)
            fprintf(f, "\t n%d -> n%d\n", p, (int)i);
    }
    fprintf(f, "}\n");
    fclose(f);
}

/** A rather long algorithm (and the auxiliary routines) to delete a number of scene nodes from the hierarchy */
/* */

// Add an index to a sorted index array, avoids adding items twice:
// One subtle requirement here is that the array is sorted. When all the children come
// strictly after their parents, this is not a problem. Otherwise, std::find() should
// be used, which naturally increases the runtime cost of the algorithm.
static void addUniqueIdx(std::vector<uint32_t> &v, uint32_t index)
{
    if (!std::binary_search(v.begin(), v.end(), index))
        v.push_back(index);
}

// Recurse down from a node and collect all nodes which are already marked for deletion
// When we want to delete one node, all its children
// must also be marked for deletion. We collect all the nodes to be deleted with the
// following recursive routine. To do this, we iterate all the children,, while traversing a scene graph.
// Each iterated index is added to the array:
static void collectNodesToDelete(const Scene &scene, int node, std::vector<uint32_t> &nodes)
{
    for (int n = scene.hierarchy_[node].firstChild_; n != -1; n = scene.hierarchy_[n].nextSibling_)
    {
        addUniqueIdx(nodes, n);
        collectNodesToDelete(scene, n, nodes);
    }
}

// returns a deleted node replacement index:
// If the input is empty, no replacement is necessary. If we have no replacement for the
// node, we recurse to the next sibling of the deleted node.
int findLastNonDeletedItem(const Scene &scene, const std::vector<int> &newIndices, int node)
{
    // we have to be more subtle:
    //   if the (newIndices[firstChild_] == -1), we should follow the link and extract the last non-removed item
    //   ..
    if (node == -1)
        return -1;

    return (newIndices[node] == -1) ? findLastNonDeletedItem(scene, newIndices, scene.hierarchy_[node].nextSibling_) : newIndices[node];
}

// replaces the pair::second value in each map's item:
void shiftMapIndices(std::unordered_map<uint32_t, uint32_t> &items, const std::vector<int> &newIndices)
{
    std::unordered_map<uint32_t, uint32_t> newItems;
    for (const auto &m : items)
    {
        int newIndex = newIndices[m.first];
        if (newIndex != -1)
            newItems[newIndex] = m.second;
    }
    items = newItems;
}

// The deleteSceneNodes() routine allows us to compress and optimize a scene graph
// while merging multiple meshes with the same material.
// Approximately an O ( N * Log(N) * Log(M)) algorithm (N = scene.size, M = nodesToDelete.size) to delete a collection of nodes from scene graph
void deleteSceneNodes(Scene &scene, const std::vector<uint32_t> &nodesToDelete)
{
    // 0) Add all the nodes down below in the hierarchy
    // starts by adding all child nodes to the deleted nodes list.
    // To keep track of moved nodes, we create a nodes linear list of indices, starting at 0:
    auto indicesToDelete = nodesToDelete;
    for (auto i : indicesToDelete)
        collectNodesToDelete(scene, i, indicesToDelete);

    // aux array with node indices to keep track of the moved ones [moved = [](node) { return (node != nodes[node]); ]
    std::vector<int> nodes(scene.hierarchy_.size());
    std::iota(nodes.begin(), nodes.end(), 0);

    // 1.a) Move all the indicesToDelete to the end of 'nodes' array (and cut them off, a variation of swap'n'pop for multiple elements)
    // Afterward, we remember the source node count and remove all the indices from
    // our linear index list. To fix the child node indices, we create a linear mapping table
    // from old node indices to the new ones:
    auto oldSize = nodes.size();
    eraseSelected(nodes, indicesToDelete);

    // 1.b) Make a newIndices[oldIndex] mapping table
    std::vector<int> newIndices(oldSize, -1);
    for (int i = 0; i < nodes.size(); i++)
        newIndices[nodes[i]] = i;

    // 2) Replace all non-null parent/firstChild/nextSibling pointers in all the nodes by new positions
    // Before deleting nodes from the hierarchy array, we remap all node indices. The
    // following lambda modifies a single Hierarchy item by finding the non-null node
    // in the newIndices container:
    auto nodeMover = [&scene, &newIndices](Hierarchy &h)
    {
        return Hierarchy{
            .parent_ = (h.parent_ != -1) ? newIndices[h.parent_] : -1,
            .firstChild_ = findLastNonDeletedItem(scene, newIndices, h.firstChild_),
            .nextSibling_ = findLastNonDeletedItem(scene, newIndices, h.nextSibling_),
            .lastSibling_ = findLastNonDeletedItem(scene, newIndices, h.lastSibling_)};
    };

    // The std::transform() algorithm modifies all the nodes in the hierarchy.
    // After fixing node indices, we are ready to actually delete data. Three calls to
    // eraseSelected() throw away the unused hierarchy and transformation items:
    std::transform(scene.hierarchy_.begin(), scene.hierarchy_.end(), scene.hierarchy_.begin(), nodeMover);

    // 3) Finally throw away the hierarchy items
    eraseSelected(scene.hierarchy_, indicesToDelete);

    // 4) As in mergeScenes() routine we also have to adjust all the "components" (i.e., meshes, materials, names and transformations)

    // 4a) Transformations are stored in arrays, so we just erase the items as we did with the scene.hierarchy_
    eraseSelected(scene.localTransform_, indicesToDelete);
    eraseSelected(scene.globalTransform_, indicesToDelete);

    // 4b) All the maps should change the key values with the newIndices[] array
    // Finally, we need to adjust the indices in mesh, material, and name maps. For this,
    // we use the shiftMapIndices() function shown here:
    shiftMapIndices(scene.meshes_, newIndices);
    shiftMapIndices(scene.materialForNode_, newIndices);
    shiftMapIndices(scene.nameForNode_, newIndices);

    // 5) scene node names list is not modified, but in principle it can be (remove all non-used items and adjust the nameForNode_ map)
    // 6) Material names list is not modified also, but if some materials fell out of use
}

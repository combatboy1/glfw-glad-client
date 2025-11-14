#ifndef RENDERING_H
#define RENDERING_H

#include <vector>
#include "playercontroller.h"
#include "block.h"
#include "mini_glm.h"
#include "texture.h"

// Face indices
enum Face {
    FACE_TOP = 0,
    FACE_BOTTOM,
    FACE_NORTH,
    FACE_SOUTH,
    FACE_WEST,
    FACE_EAST,
    FACE_COUNT
};

namespace rendering {
    void InitFlatPlane(const Block& prototype, int planeY = 0);
    void RenderVisible(const glm::mat4& proj, const glm::mat4& view);
    const Block* GetBlockAt(int x, int z);
    void SetBlockAt(int x, int z, const Block* blk);
    void Shutdown();

    // Set the texture atlas used for block face UVs. Can be nullptr to disable atlas (color-only).
    void SetAtlas(TextureAtlas* atlas);
}

#endif // RENDERING_H
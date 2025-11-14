#ifndef TEXTURE_H
#define TEXTURE_H

#include "gl_includes.h"
#include <string>
#include <vector>
#include <cstdint>

// Simple texture atlas loader + helper.
// Loads an image file into an OpenGL texture and treats it as a grid of equally-sized tiles.
// No dependency other than stb_image (stb implementation is included in texture.cpp).

class TextureAtlas {
public:
    TextureAtlas();
    ~TextureAtlas();

    // Load atlas from file. tileWidth/tileHeight are the pixel size of a single tile in the atlas.
    // Returns true on success.
    bool LoadFromFile(const std::string& filepath, int tileWidth, int tileHeight, bool flipVertically = true);

    // Get OpenGL texture ID
    GLuint GetTextureId() const { return texId_; }

    // Get atlas dimensions in tiles
    int GetColumns() const { return cols_; }
    int GetRows() const { return rows_; }

    // Given a tile index (0..cols*rows-1) compute UV rectangle (u0,v0,u1,v1)
    // UV coords are returned in 0..1 range; v is bottom-to-top consistent with OpenGL textures.
    void GetTileUV(int tileIndex, float& u0, float& v0, float& u1, float& v1) const;

    // fallback: whether atlas is valid
    bool IsValid() const { return texId_ != 0 && cols_ > 0 && rows_ > 0; }

private:
    GLuint texId_;
    int width_, height_;
    int tileW_, tileH_;
    int cols_, rows_;
};

#endif // TEXTURE_H
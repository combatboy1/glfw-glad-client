#define STB_IMAGE_IMPLEMENTATION
#include "texture.h"
#include "stb_image.h"
#include <iostream>

TextureAtlas::TextureAtlas()
    : texId_(0), width_(0), height_(0), tileW_(0), tileH_(0), cols_(0), rows_(0) {
}

TextureAtlas::~TextureAtlas() {
    if (texId_) {
        glDeleteTextures(1, &texId_);
        texId_ = 0;
    }
}

bool TextureAtlas::LoadFromFile(const std::string& filepath, int tileWidth, int tileHeight, bool flipVertically) {
    if (texId_) {
        glDeleteTextures(1, &texId_);
        texId_ = 0;
    }

    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);
    int channels = 0;
    unsigned char* data = stbi_load(filepath.c_str(), &width_, &height_, &channels, 4);
    if (!data) {
        std::cerr << "TextureAtlas::LoadFromFile failed to load image: " << filepath << "\n";
        return false;
    }

    if (tileWidth <= 0 || tileHeight <= 0) {
        std::cerr << "TextureAtlas::LoadFromFile invalid tile size\n";
        stbi_image_free(data);
        return false;
    }

    tileW_ = tileWidth;
    tileH_ = tileHeight;
    cols_ = width_ / tileW_;
    rows_ = height_ / tileH_;
    if (cols_ <= 0 || rows_ <= 0) {
        std::cerr << "TextureAtlas::LoadFromFile atlas smaller than tile size: " << width_ << "x" << height_ << "\n";
        stbi_image_free(data);
        return false;
    }

    std::cout << "TextureAtlas: image " << filepath << " (" << width_ << "x" << height_ << "), tiles " << cols_ << "x" << rows_ << "\n";

    glGenTextures(1, &texId_);
    glBindTexture(GL_TEXTURE_2D, texId_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
    return true;
}

void TextureAtlas::GetTileUV(int tileIndex, float& u0, float& v0, float& u1, float& v1) const {
    if (!IsValid()) {
        u0 = v0 = 0.0f;
        u1 = v1 = 1.0f;
        return;
    }
    if (tileIndex < 0) tileIndex = 0;
    int col = tileIndex % cols_;
    int row = tileIndex / cols_;
    if (col < 0) col = 0;
    if (row < 0) row = 0;
    if (col >= cols_) col = cols_ - 1;
    if (row >= rows_) row = rows_ - 1;

    float invW = 1.0f / float(width_);
    float invH = 1.0f / float(height_);

    // Make tile 0 = top-left (common atlas layout). We flipped v when loading, so ensure correct orientation:
    u0 = (col * tileW_) * invW;
    u1 = ((col + 1) * tileW_) * invW;

    // Flip v-axis so row 0 corresponds to top row
    v1 = 1.0f - (row * tileH_) * invH;
    v0 = 1.0f - ((row + 1) * tileH_) * invH;
}
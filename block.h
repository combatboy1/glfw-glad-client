#ifndef BLOCK_H
#define BLOCK_H

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <cctype>

#include "gl_includes.h"
#include "mini_glm.h"

// Block now supports per-face atlas tile indices.
// Faces: 0 = top, 1 = bottom, 2 = north (-Z), 3 = south (+Z), 4 = west (-X), 5 = east (+X)
class Block {
private:
    int numericId = -1;
    std::string id = "Block";
    std::string displayName = "Block";
    float hardness = 1.0f;
    bool solid = true;
    bool transparent = false;
    int faceTile[6] = { 0,0,0,0,0,0 }; // atlas tile index per face

public:
    Block() = default;
    Block(const std::string& id_, const std::string& displayName_ = "")
        : numericId(-1), id(id_), displayName(displayName_.empty() ? id_ : displayName_), hardness(1.0f), solid(true), transparent(false) {
    }

    Block(int numericId_, const std::string& id_, const std::string& displayName_, float hardness_, bool solid_, bool transparent_)
        : numericId(numericId_), id(id_), displayName(displayName_), hardness(hardness_), solid(solid_), transparent(transparent_) {
    }

    int getNumericId() const { return numericId; }
    void setNumericId(int v) { numericId = v; }

    const std::string& getId() const { return id; }
    void setId(const std::string& newId) { id = newId; }

    const std::string& getDisplayName() const { return displayName; }
    void setDisplayName(const std::string& newDisplayName) { displayName = newDisplayName; }

    float getHardness() const { return hardness; }
    void setHardness(float v) { hardness = v; }

    bool isSolid() const { return solid; }
    void setSolid(bool s) { solid = s; }

    bool isTransparent() const { return transparent; }
    void setTransparent(bool t) { transparent = t; }

    // color fallback (used if no atlas)
    glm::vec3 getColor() const {
        if (id == "Stone") return glm::vec3(0.5f, 0.5f, 0.55f);
        if (id == "Dirt")  return glm::vec3(0.45f, 0.32f, 0.15f);
        if (id == "Grass") return glm::vec3(0.2f, 0.7f, 0.2f);
        std::size_t h = std::hash<std::string>{}(id);
        float r = 0.3f + float((h >> 0) & 0xFF) / 255.0f * 0.7f;
        float g = 0.3f + float((h >> 8) & 0xFF) / 255.0f * 0.7f;
        float b = 0.3f + float((h >> 16) & 0xFF) / 255.0f * 0.7f;
        return glm::vec3(r, g, b);
    }

    // Per-face tile index setters/getters
    void SetTileForAllFaces(int tile) {
        for (int i = 0; i < 6; ++i) faceTile[i] = tile;
    }
    void SetTileForFace(int face, int tile) {
        if (face >= 0 && face < 6) faceTile[face] = tile;
    }
    int GetTileForFace(int face) const {
        if (face >= 0 && face < 6) return faceTile[face];
        return faceTile[0];
    }

private:
    // --- lightweight JSON parser helpers (same approach as before) ---
    static std::string readFileContents(const std::string& filename) {
        std::ifstream ifs(filename, std::ios::in | std::ios::binary);
        if (!ifs) throw std::runtime_error("Failed to open file: " + filename);
        std::string contents;
        ifs.seekg(0, std::ios::end);
        contents.resize((size_t)ifs.tellg());
        ifs.seekg(0, std::ios::beg);
        ifs.read(&contents[0], contents.size());
        return contents;
    }

    static void skipWhitespace(const std::string& s, size_t& pos) {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
    }

    static bool extractStringInRegion(const std::string& region, const std::string& key, std::string& out) {
        std::string target = "\"" + key + "\"";
        size_t p = region.find(target);
        if (p == std::string::npos) return false;
        p += target.size();
        size_t colon = region.find(':', p);
        if (colon == std::string::npos) return false;
        size_t pos = colon + 1;
        skipWhitespace(region, pos);
        if (pos >= region.size() || region[pos] != '"') return false;
        ++pos;
        std::string val;
        while (pos < region.size()) {
            char c = region[pos++];
            if (c == '"') break;
            if (c == '\\' && pos < region.size()) {
                char esc = region[pos++];
                if (esc == 'n') val.push_back('\n');
                else if (esc == 't') val.push_back('\t');
                else val.push_back(esc);
            }
            else {
                val.push_back(c);
            }
        }
        out = val;
        return true;
    }

    static bool extractNumberInRegion(const std::string& region, const std::string& key, double& out) {
        std::string target = "\"" + key + "\"";
        size_t p = region.find(target);
        if (p == std::string::npos) return false;
        p += target.size();
        size_t colon = region.find(':', p);
        if (colon == std::string::npos) return false;
        size_t pos = colon + 1;
        skipWhitespace(region, pos);
        if (pos >= region.size()) return false;
        size_t start = pos;
        bool found = false;
        while (pos < region.size()) {
            char c = region[pos];
            if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.' || c == 'e' || c == 'E') {
                ++pos;
                found = true;
            }
            else break;
        }
        if (!found) return false;
        std::string numstr = region.substr(start, pos - start);
        try { out = std::stod(numstr); }
        catch (...) { return false; }
        return true;
    }

    static bool extractIntInRegion(const std::string& region, const std::string& key, int& out) {
        double tmp;
        if (!extractNumberInRegion(region, key, tmp)) return false;
        out = static_cast<int>(tmp);
        return true;
    }

    static bool extractBoolInRegion(const std::string& region, const std::string& key, bool& out) {
        std::string target = "\"" + key + "\"";
        size_t p = region.find(target);
        if (p == std::string::npos) return false;
        p += target.size();
        size_t colon = region.find(':', p);
        if (colon == std::string::npos) return false;
        size_t pos = colon + 1;
        skipWhitespace(region, pos);
        if (pos + 4 <= region.size() && region.substr(pos, 4) == "true") { out = true; return true; }
        if (pos + 5 <= region.size() && region.substr(pos, 5) == "false") { out = false; return true; }
        return false;
    }

public:
    // Load blocks from file (reads per-face tile indices if present)
    static std::vector<Block> LoadBlocksFromFile(const std::string& filename) {
        std::string file = readFileContents(filename);
        size_t pos = file.find('[');
        if (pos == std::string::npos) throw std::runtime_error("Invalid JSON: expected '[' at top level");
        ++pos;
        std::vector<Block> out;
        while (true) {
            size_t objStart = file.find('{', pos);
            if (objStart == std::string::npos) break;
            size_t i = objStart;
            int depth = 0;
            for (; i < file.size(); ++i) {
                if (file[i] == '{') ++depth;
                else if (file[i] == '}') {
                    --depth;
                    if (depth == 0) break;
                }
            }
            if (i >= file.size()) break;
            size_t objEnd = i;
            std::string region = file.substr(objStart, objEnd - objStart + 1);

            int nid = -1;
            extractIntInRegion(region, "id", nid);

            std::string name = "Unknown";
            extractStringInRegion(region, "name", name);

            std::string display = name;
            extractStringInRegion(region, "displayName", display);

            double hardd = 1.0;
            extractNumberInRegion(region, "hardness", hardd);

            bool solidVal = true;
            extractBoolInRegion(region, "solid", solidVal);

            bool transpVal = false;
            extractBoolInRegion(region, "transparent", transpVal);

            // create block
            Block b(nid, name, display, static_cast<float>(hardd), solidVal, transpVal);

            // Support per-face tile indices:
            // JSON keys supported: "top", "bottom", "left", "right", "forwards"/"forward", "backwards"/"back"
            // Also support "tile" to set same tile for all faces
            int defaultTile = 0;
            if (extractIntInRegion(region, "tile", defaultTile)) {
                b.SetTileForAllFaces(defaultTile);
            }

            int t;
            if (extractIntInRegion(region, "top", t)) b.SetTileForFace(0, t);
            if (extractIntInRegion(region, "bottom", t)) b.SetTileForFace(1, t);
            if (extractIntInRegion(region, "left", t)) b.SetTileForFace(4, t);   // left -> west
            if (extractIntInRegion(region, "right", t)) b.SetTileForFace(5, t);  // right -> east

            // forwards / forward -> north (-Z)
            if (extractIntInRegion(region, "forwards", t) || extractIntInRegion(region, "forward", t)) {
                b.SetTileForFace(2, t);
            }
            // backwards / back -> south (+Z)
            if (extractIntInRegion(region, "backwards", t) || extractIntInRegion(region, "back", t)) {
                b.SetTileForFace(3, t);
            }

            out.push_back(b);
            pos = objEnd + 1;
        }
        return out;
    }

    static std::unordered_map<int, Block> LoadBlocksMapByNumericId(const std::string& filename) {
        auto vec = LoadBlocksFromFile(filename);
        std::unordered_map<int, Block> map;
        map.reserve(vec.size());
        for (const auto& b : vec) map[b.getNumericId()] = b;
        return map;
    }

    static std::unordered_map<std::string, Block> LoadBlocksMapByName(const std::string& filename) {
        auto vec = LoadBlocksFromFile(filename);
        std::unordered_map<std::string, Block> map;
        map.reserve(vec.size());
        for (const auto& b : vec) map[b.getId()] = b;
        return map;
    }
};

#endif // BLOCK_H
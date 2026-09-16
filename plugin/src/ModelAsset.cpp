// Rockband Mod (2026) addition -- see README.md for the credit / changes
// note (AGPL-3.0 §5a).
#include "ModelAsset.h"
#include <unordered_map>
#include <string>

using namespace juce::gl;

bool ModelAsset::parseObj(const char* objText, int objTextSize, juce::Array<float>& outVerts, juce::Array<unsigned int>& outIdx)
{
    juce::Array<juce::Vector3D<float>> positions, normals;
    juce::Array<juce::Point<float>> uvs;
    std::unordered_map<std::string, unsigned int> vertexCache;   // "vi/ti/ni" -> output vertex index

    // BinaryData's embedded bytes are not nul-terminated -- stop at the
    // known byte count (or an embedded nul, whichever comes first), never
    // read past the buffer looking for one
    auto lines = juce::StringArray::fromLines(juce::String::fromUTF8(objText, objTextSize));
    for (auto lineRaw : lines)
    {
        const auto line = lineRaw.trim();
        if (line.startsWith("v "))
        {
            auto t = juce::StringArray::fromTokens(line.substring(2), " ", "");
            t.removeEmptyStrings();
            if (t.size() >= 3)
                positions.add({ t[0].getFloatValue(), t[1].getFloatValue(), t[2].getFloatValue() });
        }
        else if (line.startsWith("vn "))
        {
            auto t = juce::StringArray::fromTokens(line.substring(3), " ", "");
            t.removeEmptyStrings();
            if (t.size() >= 3)
                normals.add({ t[0].getFloatValue(), t[1].getFloatValue(), t[2].getFloatValue() });
        }
        else if (line.startsWith("vt "))
        {
            auto t = juce::StringArray::fromTokens(line.substring(3), " ", "");
            t.removeEmptyStrings();
            if (t.size() >= 2)
                uvs.add({ t[0].getFloatValue(), t[1].getFloatValue() });
        }
        else if (line.startsWith("f "))
        {
            auto t = juce::StringArray::fromTokens(line.substring(2), " ", "");
            t.removeEmptyStrings();
            if (t.size() < 3)
                continue;
            // fan-triangulate: the conversion tool asks Assimp to triangulate
            // on export, so this loop normally runs once per face (a
            // no-op safety net, not the common case)
            for (int i = 1; i + 1 < t.size(); ++i)
            {
                const juce::String tri[3] = { t[0], t[i], t[i + 1] };
                for (auto& corner : tri)
                {
                    const auto key = corner.toStdString();
                    if (auto it = vertexCache.find(key); it != vertexCache.end())
                    {
                        outIdx.add(it->second);
                        continue;
                    }
                    const auto parts = juce::StringArray::fromTokens(corner, "/", "");
                    const int vi = parts[0].getIntValue() - 1;
                    const int ti = (parts.size() > 1 && parts[1].isNotEmpty()) ? parts[1].getIntValue() - 1 : -1;
                    const int ni = (parts.size() > 2 && parts[2].isNotEmpty()) ? parts[2].getIntValue() - 1 : -1;
                    if (vi < 0 || vi >= positions.size())
                        return false;   // malformed -- fail the whole load rather than draw garbage
                    const auto p = positions[vi];
                    const auto n = (ni >= 0 && ni < normals.size()) ? normals[ni] : juce::Vector3D<float>(0, 1, 0);
                    const auto uv = (ti >= 0 && ti < uvs.size()) ? uvs[ti] : juce::Point<float>(0.0f, 0.0f);
                    outVerts.add(p.x); outVerts.add(p.y); outVerts.add(p.z);
                    outVerts.add(n.x); outVerts.add(n.y); outVerts.add(n.z);
                    outVerts.add(uv.x); outVerts.add(uv.y);
                    const auto newIdx = (unsigned int) (outVerts.size() / 8 - 1);
                    if (newIdx > 0xFFFFu)
                        return false;   // stylized props only -- not expected to need 32-bit indices
                    vertexCache[key] = newIdx;
                    outIdx.add(newIdx);
                }
            }
        }
    }
    return ! outIdx.isEmpty();
}

bool ModelAsset::loadTexture(const void* pngData, int pngSize)
{
    auto img = juce::ImageFileFormat::loadFrom(pngData, (size_t) pngSize);
    if (! img.isValid())
        return false;
    img = img.convertedToFormat(juce::Image::ARGB);
    const int w = img.getWidth(), h = img.getHeight();
    juce::HeapBlock<uint8_t> rgba((size_t) w * (size_t) h * 4);
    {
        const juce::Image::BitmapData bd(img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const auto c = bd.getPixelColour(x, y);
                auto* px = rgba.getData() + ((size_t) y * (size_t) w + (size_t) x) * 4;
                px[0] = c.getRed(); px[1] = c.getGreen(); px[2] = c.getBlue(); px[3] = c.getAlpha();
            }
    }
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.getData());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

bool ModelAsset::load(const char* objText, int objTextSize, const void* pngData, int pngSize)
{
    release();
    juce::Array<float> verts;
    juce::Array<unsigned int> idx;
    if (! parseObj(objText, objTextSize, verts, idx))
        return false;

    juce::Array<unsigned short> idx16;
    idx16.ensureStorageAllocated(idx.size());
    for (auto i : idx)
        idx16.add((unsigned short) i);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * (int) sizeof(float), verts.getRawDataPointer(), GL_STATIC_DRAW);
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx16.size() * (int) sizeof(unsigned short), idx16.getRawDataPointer(), GL_STATIC_DRAW);
    indexCount = idx16.size();

    if (! loadTexture(pngData, pngSize))
    {
        release();
        return false;
    }
    return true;
}

void ModelAsset::release()
{
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (ibo) { glDeleteBuffers(1, &ibo); ibo = 0; }
    if (texId) { glDeleteTextures(1, &texId); texId = 0; }
    indexCount = 0;
}

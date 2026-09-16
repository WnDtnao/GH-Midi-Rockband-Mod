// Rockband Mod (2026) addition -- see README.md for the credit / changes
// note (AGPL-3.0 §5a).
#pragma once
#include <juce_opengl/juce_opengl.h>

// Loads a plain Wavefront .obj (positions/normals/UVs/triangulated faces
// only -- no materials, no quads/ngons) plus a PNG texture into a GL mesh +
// GL texture. The .obj/.png pair is produced once, offline, by
// tools/model_convert/ (which does the real work of reading YARG's Unity FBX
// assets via Assimp) -- this class only ever parses the already-simple
// output of that conversion, never anything Unity/FBX-shaped, and links no
// asset-import library into the shipped plugin.
//
// Vertex layout uploaded to GL_ARRAY_BUFFER: pos.xyz, normal.xyz, uv.xy (8
// floats/vertex) -- deliberately close to HighwayRenderer::Mesh's existing
// pos+normal+matId layout, just swapping the material-ID float for a UV pair
// and letting the shader sample a texture instead of thresholding matId.
class ModelAsset
{
public:
    ModelAsset() = default;
    ~ModelAsset() { jassert(vbo == 0 && ibo == 0 && texId == 0); }   // release() must run first, with a live GL context

    // objText/objTextSize: the .obj file's full text, as raw embedded bytes
    // -- NOT assumed nul-terminated (BinaryData's embedded files aren't).
    // pngData/pngSize: an embedded PNG. Requires a current GL context, same
    // as HighwayRenderer's own mesh-building calls in newOpenGLContextCreated().
    bool load(const char* objText, int objTextSize, const void* pngData, int pngSize);

    // Requires a current GL context (call from openGLContextClosing(), same
    // as HighwayRenderer::openGLContextClosing() already does for its meshes).
    void release();

    bool isLoaded() const { return vbo != 0 && indexCount > 0; }
    unsigned int getVbo() const { return vbo; }
    unsigned int getIbo() const { return ibo; }
    unsigned int getTextureId() const { return texId; }
    int getIndexCount() const { return indexCount; }

private:
    bool parseObj(const char* objText, int objTextSize, juce::Array<float>& outVerts, juce::Array<unsigned int>& outIdx);
    bool loadTexture(const void* pngData, int pngSize);

    unsigned int vbo = 0, ibo = 0, texId = 0;
    int indexCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModelAsset)
};

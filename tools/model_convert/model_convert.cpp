// Rockband Mod (2026) addition -- see README.md for the credit / changes
// note (AGPL-3.0 §5a).
//
// Dev-time only: converts a Unity FBX mesh (as used by YARG,
// https://github.com/YARC-Official/YARG, LGPL-3.0-or-later) into a plain
// Wavefront .obj that plugin/src/ModelAsset.cpp can parse without Assimp
// linked into the shipped plugin. Never built as part of the main plugin
// build -- see this directory's CMakeLists.txt.
//
// Usage: model_convert <input.fbx> <output.obj>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cstdio>

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: %s <input.fbx> <output.obj>\n", argv[0]);
        return 1;
    }

    Assimp::Importer importer;
    // FlipUVs: Unity/FBX and OpenGL commonly disagree on the V axis. If a
    // converted texture looks upside-down once rendered, try dropping this
    // flag first before assuming the texture itself is wrong.
    const aiScene* scene = importer.ReadFile(argv[1],
        aiProcess_Triangulate | aiProcess_GenSmoothNormals
        | aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs
        | aiProcess_ValidateDataStructure);
    if (scene == nullptr || scene->mNumMeshes == 0)
    {
        std::fprintf(stderr, "assimp import failed: %s\n", importer.GetErrorString());
        return 1;
    }

    FILE* out = std::fopen(argv[2], "w");
    if (out == nullptr)
    {
        std::fprintf(stderr, "could not open %s for writing\n", argv[2]);
        return 1;
    }
    std::fprintf(out, "# converted by tools/model_convert from %s\n", argv[1]);
    std::fprintf(out, "# source: YARG (https://github.com/YARC-Official/YARG), LGPL-3.0-or-later\n");

    unsigned int vertBase = 1;   // OBJ indices are 1-based
    for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[m];
        const bool hasUV = mesh->HasTextureCoords(0);
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            const auto& v = mesh->mVertices[i];
            std::fprintf(out, "v %f %f %f\n", v.x, v.y, v.z);
        }
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            if (mesh->HasNormals())
            {
                const auto& n = mesh->mNormals[i];
                std::fprintf(out, "vn %f %f %f\n", n.x, n.y, n.z);
            }
            else
                std::fprintf(out, "vn 0 1 0\n");
        }
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            if (hasUV)
            {
                const auto& uv = mesh->mTextureCoords[0][i];
                std::fprintf(out, "vt %f %f\n", uv.x, uv.y);
            }
            else
                std::fprintf(out, "vt 0 0\n");
        }
        for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
        {
            const auto& face = mesh->mFaces[f];
            if (face.mNumIndices != 3)
                continue;   // aiProcess_Triangulate should make this a no-op
            std::fprintf(out, "f");
            for (unsigned int k = 0; k < 3; ++k)
            {
                const unsigned int idx = face.mIndices[k] + vertBase;
                std::fprintf(out, " %u/%u/%u", idx, idx, idx);
            }
            std::fprintf(out, "\n");
        }
        vertBase += mesh->mNumVertices;
    }
    std::fclose(out);
    std::printf("wrote %s (%u mesh(es))\n", argv[2], scene->mNumMeshes);
    return 0;
}

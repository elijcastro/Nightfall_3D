#ifndef TEXTURELOADER_H
#define TEXTURELOADER_H

#include <vector>
#include <string>
#include <iostream>
#include <assimp/material.h>
#include "Mesh.h"

class TextureLoader {
public:
    static unsigned int LoadTextureFromFile(const std::string& filename,
                                            const std::string& directory);

    static std::vector<Texture> LoadMaterialTextures(aiMaterial* mat,
                                                     aiTextureType type,
                                                     std::string typeName,
                                                     const std::string& directory);
};

// SOLO DECLARACIÓN — NO DEFINICIÓN
unsigned int LoadCubemap(const std::vector<std::string>& faces);

#endif

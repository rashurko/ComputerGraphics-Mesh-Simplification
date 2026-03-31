#ifndef MODEL_H
#define MODEL_H

#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "OBJ_Loader.h"
#include "stb_image.h"

#include "shader.hpp"
#include "mesh.hpp"

class Model {
    public:
        Model(std::string path) {
            loadModel(path);
        }

        void Draw(Shader &shader) {
            for (unsigned int i = 0; i < meshes.size(); i++) {
                meshes[i].Draw(shader);
            }
        }

        glm::vec3 getLocalX() const {
            return localX;
        }

        glm::vec3 getLocalY() const {
            return localY;
        }

        glm::vec3 getLocalZ() const {
            return localZ;
        }

        glm::vec3 setLocalX(glm::vec3 newX) {
            localX = newX;
            return localX;
        }

        glm::vec3 setLocalY(glm::vec3 newY) {
            localY = newY;
            return localY;
        }

        glm::vec3 setLocalZ(glm::vec3 newZ) {
            localZ = newZ;
            return localZ;
        }
        
    private:
        // model data
        std::vector<Mesh> meshes;
        std::string directory;

        // local axis
        glm::vec3 localX;
        glm::vec3 localY;
        glm::vec3 localZ;

        void loadModel(std::string path) {
            // Initialize loader
            objl::Loader loader;

            // Load .obj file
            bool isLoaded = loader.LoadFile(path);

            // Check if the file is properly loaded
            if (isLoaded) {

                // Go through each loaded mesh
                for (int i = 0; i < loader.LoadedMeshes.size(); i++) {
                    // copy the current mesh
                    objl::Mesh curMesh = loader.LoadedMeshes[i];

                    // process and store the mesh
                    meshes.push_back(processMesh(curMesh));
                }

            } else {
                std::cout << "Failed to load " << path << std::endl;
            }

            // Declare local axes
            localX = glm::vec3(1.0f, 0.0f, 0.0f);
            localY = glm::vec3(0.0f, 1.0f, 0.0f);
            localZ = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        Mesh processMesh(objl::Mesh mesh) {
            std::vector<Vertex> vertices;
            std::vector<unsigned int> indices;
            std::vector<Texture> textures;

            // Go through each vertex and push it into the vertices vector
            for (int i = 0; i < mesh.Vertices.size(); i++) {
                Vertex vertex;
                vertex.Position = glm::vec3(mesh.Vertices[i].Position.X, mesh.Vertices[i].Position.Y, mesh.Vertices[i].Position.Z);
                vertex.Normal = glm::vec3(mesh.Vertices[i].Normal.X, mesh.Vertices[i].Normal.Y, mesh.Vertices[i].Normal.Z);
                vertex.TexCoords = glm::vec2(mesh.Vertices[i].TextureCoordinate.X, mesh.Vertices[i].TextureCoordinate.Y);
                vertices.push_back(vertex);
            }

            // Go through each index and push it into the indices vector
            for (int i = 0; i < mesh.Indices.size(); i++) {
                indices.push_back(mesh.Indices[i]);
            } 

            // Process the material
            Texture texture;
            texture.id = TextureFromFile(mesh.MeshMaterial, directory);
            texture.type = "texture_diffuse";
            textures.push_back(texture);

            // Return the processed mesh
            return Mesh(vertices, indices, textures);   
        }

        unsigned int TextureFromFile(objl::Material mat, std::string directory) {
            std::string filename = std::string(mat.name);
            filename = directory + '/' + filename;

            unsigned int textureID;
            glGenTextures(1, &textureID);

            int width, height, nrComponents;
            unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
            if (data)
            {
                GLenum format;
                if (nrComponents == 1)
                    format = GL_RED;
                else if (nrComponents == 3)
                    format = GL_RGB;
                else if (nrComponents == 4)
                    format = GL_RGBA;

                glBindTexture(GL_TEXTURE_2D, textureID);
                glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                stbi_image_free(data);
            }
            else
            {
                std::cout << "Texture failed to load at path: " << mat.name << std::endl;
                stbi_image_free(data);
            }

            return textureID;
        }
};

#endif
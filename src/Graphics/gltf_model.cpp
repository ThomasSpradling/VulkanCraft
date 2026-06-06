#include "gltf_model.h"
#include "../errors.h"
#include <iostream>

GLTFModel::GLTFModel(const VulkanRenderer &renderer, std::filesystem::path file_path)
    : m_renderer(renderer)
{
    Load(file_path);
}

void GLTFModel::Load(std::filesystem::path file_path) {
    //// Load file into memory ////
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;

    std::string warnings;
    std::string errors;

    std::string ext = file_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Load based on correct file format
    {
        bool ret = false;
        if (ext == ".glb") {
            ret = loader.LoadBinaryFromFile(&model, &errors, &warnings, file_path.string());
        } else if (ext == ".gltf") {
            ret = loader.LoadASCIIFromFile(&model, &errors, &warnings, file_path.string());
        } else {
            throw std::runtime_error("Invalid gltf file!");
        }
    }

    if (!warnings.empty())
        std::cerr << "GLTF Warning: " << warnings << std::endl;

    if (!errors.empty())
        throw std::runtime_error("GLTF Error: " + errors);

    uint32_t default_scene_index = model.defaultScene == -1 ? 0 : model.defaultScene;
    const tinygltf::Scene &scene = model.scenes[default_scene_index];

    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    for (const tinygltf::Mesh &mesh : model.meshes) {
        GLTFMeshAsset new_mesh;
        vertices.clear();
        indices.clear();

        // We combine all primitives into one mesh
        for (const tinygltf::Primitive &primitive : mesh.primitives) {
            Assert(primitive.indices >= 0, "GLTF Error in " + file_path.string() + ": Missing index buffer accessor.");

            MeshPrimitive new_primitive {
                .start_index = static_cast<uint32_t>(indices.size()),
                .index_count = static_cast<uint32_t>(model.accessors[primitive.indices].count)
            };

            size_t initial_vertex = vertices.size();

            //// Indices ////
            {
                tinygltf::Accessor &index_accessor = model.accessors[primitive.indices];
                indices.reserve(indices.size() + index_accessor.count);

                IterateAccessor<uint32_t>(model, index_accessor, [&](uint32_t i, uint32_t index) {
                    indices.push_back(index + initial_vertex);
                });
            }

            //// Positions ////
            {
                tinygltf::Accessor &position_accessor = model.accessors[primitive.attributes.at("POSITION")];
                vertices.resize(vertices.size() + position_accessor.count);

                IterateAccessor<glm::vec3>(model, position_accessor, [&](uint32_t i, glm::vec3 pos) {
                    MeshVertex new_vertex {
                        .position = pos,
                        .uv_x = 0.0f,
                        .normal = glm::vec3(1, 0, 0),
                        .uv_y = 0.0f,
                        .color = glm::vec4(1.0f),
                    };
                    vertices[initial_vertex + i] = new_vertex;
                });
            }

            //// Normals ////
            if (primitive.attributes.contains("NORMAL")) {
                tinygltf::Accessor &normal_accessor = model.accessors[primitive.attributes.at("NORMAL")];
                
                IterateAccessor<glm::vec3>(model, normal_accessor, [&](uint32_t i, glm::vec3 normal) {
                    vertices[initial_vertex + i].normal = normal;
                });
            }

            //// Color ////
            if (primitive.attributes.contains("COLOR_0")) {
                tinygltf::Accessor &normal_accessor = model.accessors[primitive.attributes.at("COLOR_0")];
                
                IterateAccessor<glm::vec4>(model, normal_accessor, [&](uint32_t i, glm::vec4 color) {
                    vertices[initial_vertex + i].color = color;
                });
            }

            //// UV ////
            if (primitive.attributes.contains("TEXCOORD_0")) {
                tinygltf::Accessor &normal_accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                
                IterateAccessor<glm::vec2>(model, normal_accessor, [&](uint32_t i, glm::vec2 uv) {
                    vertices[initial_vertex + i].uv_x = uv.x;
                    vertices[initial_vertex + i].uv_y = uv.y;
                });
            }

            new_mesh.primitives.push_back(new_primitive);
        }

        // Upload to GPU
        new_mesh.mesh_buffers = m_renderer.UploadGPUMesh(vertices, indices);
        m_meshes.emplace_back(std::make_shared<GLTFMeshAsset>(std::move(new_mesh)));
    }
}

void GLTFModel::CleanUp() {
    for (auto &mesh : m_meshes) {
        m_renderer.DestroyGPUMesh(mesh->mesh_buffers);
    }
}

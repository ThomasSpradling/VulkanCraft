#include "gltf_model.h"
#include "../errors.h"
#include "descriptor_allocator.h"
#include "descriptor_writer.h"
#include "renderable.h"

#define GLM_ENABLE_EXPERIMENTAL 
#include <glm/gtx/quaternion.hpp>
#include <glm/ext/vector_float4.hpp>
#include <iostream>
#include <tuple>
#include <glm/gtc/type_ptr.hpp>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

GLTFModel::GLTFModel(const VulkanRenderer &renderer, const GLTFCoreData &core, std::filesystem::path file_path)
    : m_renderer(renderer)
    , m_core(core)
{
    m_descriptor_allocator = std::make_unique<DescriptorAllocator>(renderer.GetDevice());
    std::vector<DescriptorPoolRatios> ratios {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 },
    };
    m_descriptor_allocator->Init(16, ratios);
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

    //// ---- Load Samplers ---- ////
    for (const tinygltf::Sampler &sampler : model.samplers) {
        VkSampler device_sampler;
        // TODO: Check that minFilter and magFilter yield same mipmap modes
        auto [min_filter, mipmap_mode1] = GetVulkanTextureFilters(sampler.minFilter);
        auto [mag_filter, mipmap_mode2] = GetVulkanTextureFilters(sampler.magFilter);
        VkSamplerCreateInfo sampler_create_info {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = mag_filter,
            .minFilter = min_filter,
            .mipmapMode = mipmap_mode1,
            .addressModeU = GetVulkanAddressMode(sampler.wrapS),
            .addressModeV = GetVulkanAddressMode(sampler.wrapT),
        };
        VK_CHECK(vkCreateSampler(m_renderer.GetDevice(), &sampler_create_info, nullptr, &device_sampler));
        m_samplers.push_back(device_sampler);
    }

    //// ---- Load Images ---- ////
    for (tinygltf::Image &image : model.images) {
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        VkExtent3D extent {
            .width = static_cast<uint32_t>(image.width),
            .height = static_cast<uint32_t>(image.height),
            .depth = 1,
        };
        GPUImage device_image {
            .extent = extent,
            .format = format,
        };
        VkImageCreateInfo image_create_info {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = extent,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VmaAllocationCreateInfo allocation_create_info {
            .flags = 0,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };

        vmaCreateImage(m_renderer.GetMemoryAllocator(), &image_create_info, &allocation_create_info, &device_image.image, &device_image.allocation, nullptr);

        VkImageViewCreateInfo image_view_create_info {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = device_image.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = device_image.format,
            .components = COMPONENT_MAPPING_DEFAULT,
            .subresourceRange = IMAGE_SUBRESOURCE_RANGE_ALL,
        };
        vkCreateImageView(m_renderer.GetDevice(), &image_view_create_info, nullptr, &device_image.image_view);

        m_renderer.LoadImageData(device_image, image.image.data(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_images.push_back(std::make_shared<GPUImage>(std::move(device_image)));
    }

    //// ---- Load Materials ---- ////
    if (model.materials.empty()) {
        // Create a single default material

        GLTFMaterialResources resources {
            .pass = MaterialPass::Opaque,
        };
        resources.descriptor_set = m_descriptor_allocator->AllocateDescriptorSet(m_core.material_layout);

        // Create uniform buffer
        VkBufferCreateInfo buffer_create_info {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .size = sizeof(GLTFMaterialData),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VmaAllocationCreateInfo allocation_create_info {
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
        VmaAllocationInfo alloc_info;
        VK_CHECK(vmaCreateBuffer(m_renderer.GetMemoryAllocator(), &buffer_create_info, &allocation_create_info, &resources.uniform_buffer, &resources.uniform_buffer_alloc, &alloc_info));
        auto *uniform_data = static_cast<GLTFMaterialData *>(alloc_info.pMappedData);
        uniform_data->color_factors = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        uniform_data->metallic_roughness_factors = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);

        // Create Albedo image
        VkSampler albedo_sampler = m_renderer.GetDefaultSamplers().nearest;
        resources.color_image = m_renderer.GetDefaultTextures().gray;

        // Create Metallic Roughness Image
        VkSampler metallic_roughness_sampler = m_renderer.GetDefaultSamplers().nearest;
        resources.metallic_roughness_image = m_renderer.GetDefaultTextures().white;

        DescriptorWriter(m_renderer.GetDevice())
            .WriteBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {
                .buffer = resources.uniform_buffer,
                .offset = 0,
                .range = sizeof(GLTFMaterialData),
            })
            .WriteImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, {
                .sampler = albedo_sampler,
                .imageView = resources.color_image->image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            })
            .WriteImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, {
                .sampler = metallic_roughness_sampler,
                .imageView = resources.metallic_roughness_image->image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            })
            .Write(resources.descriptor_set);
        m_material_data.push_back(std::move(resources));
    }

    for (const tinygltf::Material &material : model.materials) {
        // TODO: Better error detection
        GLTFMaterialResources resources;
        if (material.alphaMode == "BLEND") {
            resources.pass = MaterialPass::Transparent;
        } else {
            resources.pass = MaterialPass::Opaque;
        }

        // Allocate a descriptor set and write all these material data to it.
        resources.descriptor_set = m_descriptor_allocator->AllocateDescriptorSet(m_core.material_layout);
        
        // Create uniform buffer
        VkBufferCreateInfo buffer_create_info {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .size = sizeof(GLTFMaterialData),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VmaAllocationCreateInfo allocation_create_info {
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
        VmaAllocationInfo alloc_info;
        VK_CHECK(vmaCreateBuffer(m_renderer.GetMemoryAllocator(), &buffer_create_info, &allocation_create_info, &resources.uniform_buffer, &resources.uniform_buffer_alloc, &alloc_info));
        auto *uniform_data = static_cast<GLTFMaterialData *>(alloc_info.pMappedData);
        uniform_data->color_factors = glm::make_vec4(material.pbrMetallicRoughness.baseColorFactor.data());
        uniform_data->metallic_roughness_factors = glm::vec4(
            material.pbrMetallicRoughness.metallicFactor,
            material.pbrMetallicRoughness.roughnessFactor,
            0.0f, 0.0f
        );

        // Find albedo image
        VkSampler albedo_sampler;
        int color_texture_index = material.pbrMetallicRoughness.baseColorTexture.index;
        if (color_texture_index > -1) {
            tinygltf::Texture &albedo_texture = model.textures[color_texture_index];
            albedo_sampler = albedo_texture.sampler > -1
                ? m_samplers[albedo_texture.sampler]
                : m_renderer.GetDefaultSamplers().nearest;
            resources.color_image = albedo_texture.source > -1 
                ? m_images[albedo_texture.source]
                : m_renderer.GetDefaultTextures().gray;
        } else {
            std::cerr << "WARNING: Missing Color Image\n";
            albedo_sampler = m_renderer.GetDefaultSamplers().nearest;
            resources.color_image = m_renderer.GetDefaultTextures().gray;
        }

        // Find metallic roughness image
        VkSampler metallic_roughness_sampler;
        int metallic_roughness_texture_index = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
        if (metallic_roughness_texture_index > -1) {
            tinygltf::Texture &metallic_roughness_texture = model.textures[metallic_roughness_texture_index];
            metallic_roughness_sampler = metallic_roughness_texture.sampler > -1 
                ? m_samplers[metallic_roughness_texture.sampler]
                : m_renderer.GetDefaultSamplers().nearest;
            resources.metallic_roughness_image = metallic_roughness_texture.source > -1
                ? m_images[metallic_roughness_texture.source]
                : m_renderer.GetDefaultTextures().white;
        } else {
            // std::cerr << "WARNING: Missing Metallic Roughness Image\n";
            metallic_roughness_sampler = m_renderer.GetDefaultSamplers().nearest;
            resources.metallic_roughness_image = m_renderer.GetDefaultTextures().white;
        }

        DescriptorWriter(m_renderer.GetDevice())
            .WriteBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {
                .buffer = resources.uniform_buffer,
                .offset = 0,
                .range = sizeof(GLTFMaterialData),
            })
            .WriteImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, {
                .sampler = albedo_sampler,
                .imageView = resources.color_image->image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            })
            .WriteImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, {
                .sampler = metallic_roughness_sampler,
                .imageView = resources.metallic_roughness_image->image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            })
            .Write(resources.descriptor_set);
        m_material_data.push_back(std::move(resources));
    }

    //// ---- Load Meshes ---- ////
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

            new_primitive.material_descriptor_set = m_material_data[std::max(0, primitive.material)].descriptor_set;
            new_primitive.pass = m_material_data[std::max(0, primitive.material)].pass;

            // TODO: make it depend on whether this primitive is transparent or opaque
            if (new_primitive.pass == MaterialPass::Opaque) {
                new_primitive.pipeline = m_core.opaque_pipeline;
                new_primitive.pipeline_layout = m_core.opaque_pipeline_layout;
            } else if (new_primitive.pass == MaterialPass::Transparent) {
                new_primitive.pipeline = m_core.transparent_pipeline;
                new_primitive.pipeline_layout = m_core.transparent_pipeline_layout;
            }

            new_mesh.primitives.push_back(new_primitive);
        }

        // Upload to GPU
        new_mesh.mesh_buffers = m_renderer.UploadGPUMesh(vertices, indices);
        m_meshes.emplace_back(std::make_shared<GLTFMeshAsset>(std::move(new_mesh)));
    }

    //// ---- Load Nodes ---- ////
    for (uint32_t i = 0; i < scene.nodes.size(); ++i) {
        tinygltf::Node &root_node = model.nodes[scene.nodes[i]];
        std::shared_ptr<GLTFSceneNode> root_scene_node = std::make_shared<GLTFSceneNode>();
        m_nodes.push_back(root_scene_node);
        LoadNode(model, root_node, m_nodes.back());
    }
}

void GLTFModel::CleanUp() {
    for (auto &mesh : m_meshes) {
        m_renderer.DestroyGPUMesh(mesh->mesh_buffers);
    }

    for (auto &sampler : m_samplers) {
        vkDestroySampler(m_renderer.GetDevice(), sampler, nullptr);
    }

    for (auto &image : m_images) {
        vkDestroyImageView(m_renderer.GetDevice(), image->image_view, nullptr);
        vmaDestroyImage(m_renderer.GetMemoryAllocator(), image->image, image->allocation);
    }

    for (auto &resource : m_material_data) {
        vmaDestroyBuffer(m_renderer.GetMemoryAllocator(), resource.uniform_buffer, resource.uniform_buffer_alloc);
    }
    m_descriptor_allocator->Destroy();
}

void GLTFModel::Draw(DrawContext &context) {
    // for each node:
        // draw_node

    for (auto &node : m_nodes) {
        // node->world_transform
        if (node->mesh == nullptr)
            continue;

        for (auto &primitive : node->mesh->primitives) {
            RenderObject render_object {
                .index_count = primitive.index_count,
                .start_index = primitive.start_index,
                .index_buffer = node->mesh->mesh_buffers.index_buffer,
                .transform = node->world_transform,
                .vertex_buffer = node->mesh->mesh_buffers.device_address,

                .pipeline = primitive.pipeline,
                .pipeline_layout = primitive.pipeline_layout,

                .descriptor_set = primitive.material_descriptor_set,
            };

            if (primitive.pass == MaterialPass::Opaque)
                context.opaque_objects.push_back(std::move(render_object));
            else if (primitive.pass == MaterialPass::Transparent)
                context.transparent_objects.push_back(std::move(render_object));
        }
    }

    // for (auto &mesh : m_meshes) {
    //     for (auto &primitive : mesh->primitives) {
    //         context.render_objects.push_back({
    //             .index_count = primitive.index_count,
    //             .start_index = primitive.start_index,
    //             .index_buffer = mesh->mesh_buffers.index_buffer,
    //             .transform = glm::mat4(1.0f),
    //             .vertex_buffer = mesh->mesh_buffers.device_address,

    //             .pipeline = primitive.pipeline,
    //             .pipeline_layout = primitive.pipeline_layout,

    //             .descriptor_set = primitive.material_descriptor_set,
    //         });
    //     }
    // }
}

void GLTFModel::LoadNode(const tinygltf::Model &model, const tinygltf::Node &node, std::shared_ptr<GLTFSceneNode> scene_node) {
    scene_node->local_transform = GetNodeTransform(node);
    std::shared_ptr<GLTFSceneNode> parent = scene_node->parent.lock();
    scene_node->world_transform = parent
        ? parent->world_transform * scene_node->local_transform
        : scene_node->local_transform;

    if (node.mesh != -1) {
        scene_node->mesh = m_meshes[node.mesh];
    }

    // create new node and connect children and transforms
    for (uint32_t i = 0; i < node.children.size(); ++i) {
        std::shared_ptr<GLTFSceneNode> child = std::make_shared<GLTFSceneNode>();
        child->parent = scene_node;
        scene_node->children.push_back(child);
        m_nodes.push_back(child);
        LoadNode(model, model.nodes[node.children[i]], child);
    }
}

glm::mat4 GLTFModel::GetNodeTransform(const tinygltf::Node &node) {
    glm::vec3 translation_vector = glm::vec3(0.0f);
    if (node.translation.size())
        translation_vector = glm::make_vec3(node.translation.data());

    glm::vec3 scale_vector = glm::vec3(1.0f);
    if (node.scale.size())
        scale_vector = glm::make_vec3(node.scale.data());

    glm::quat rotation_quaternion = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (node.rotation.size()) {
        glm::vec4 rotation_vector = glm::make_vec4(node.rotation.data());
        rotation_quaternion = glm::quat(rotation_vector.w, rotation_vector.x, rotation_vector.y, rotation_vector.z);
    }

    glm::mat4 translation = glm::translate(glm::mat4(1.0f), translation_vector);
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), scale_vector);
    glm::mat4 rotation = glm::toMat4(rotation_quaternion);

    glm::mat4 transform = glm::mat4(1.0f);
    if (node.matrix.size())
        transform = glm::make_mat4(node.matrix.data());

    return translation * rotation * scale * transform;
}

std::pair<VkFilter, VkSamplerMipmapMode> GLTFModel::GetVulkanTextureFilters(int gltf_filter) {
    VkFilter filter = VK_FILTER_NEAREST;
    VkSamplerMipmapMode mip_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    switch (gltf_filter) {
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
            filter = VK_FILTER_LINEAR;
            mip_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            filter = VK_FILTER_LINEAR;
            mip_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
            filter = VK_FILTER_NEAREST;
            mip_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
            filter = VK_FILTER_NEAREST;
            mip_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
    }
    return std::make_pair(filter, mip_mode);
}

VkSamplerAddressMode GLTFModel::GetVulkanAddressMode(int gltf_wrapping) {
    switch (gltf_wrapping) {
        case TINYGLTF_TEXTURE_WRAP_REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        default:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

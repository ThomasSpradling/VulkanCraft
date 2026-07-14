#include "GLTFModel.h"
#include "Core/Math.h"
#include "Platform/Graphics/CommandBuffer.h"
#include "Platform/Graphics/VulkanImage.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <glm/ext/quaternion_common.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <stdexcept>
#include <variant>

#include <mikktspace.h>

// =================== //
// ---- GLTF Node ---- //
// =================== //

// GLTFModel::Node(glm::dev3, );

// GLTFModel::Node(glm::dvec3 initial_translation, glm::dquat initial_rotation, glm::dvec3 initial_scale, glm::dmat4 initial_matrix)
// {}

GLTFModel::Node::Node(glm::dvec3 initial_translation, glm::dquat initial_rotation, glm::dvec3 initial_scale, glm::dmat4 initial_matrix)
    : initial_translation(initial_translation)
    , initial_rotation(initial_rotation)
    , initial_scale(initial_scale)
    , initial_matrix(initial_matrix)
    , translation(initial_translation)
    , rotation(initial_rotation)
    , scale(initial_scale)
    , matrix(initial_matrix)
{}

void GLTFModel::Node::MarkDirty() {
    global_dirty = true;
    local_dirty = true;

    for (auto &child : children) {
        child->MarkDirty();
    }
}

glm::dmat4 GLTFModel::Node::ComputeLocalTransform() {
    if (local_dirty) {
        cached_local_transform = glm::translate(glm::dmat4(1.0), translation)
            * glm::dmat4(rotation)
            * glm::scale(glm::dmat4(1.0), scale)
            * matrix;
        local_dirty = false;
        return cached_local_transform;
    } else {
        return cached_local_transform;
    }
}

glm::dmat4 GLTFModel::Node::ComputeGlobalTransform() {
    if (global_dirty) {
        Node *p = parent;
        cached_global_transform = ComputeLocalTransform();
        while (p) {
            cached_global_transform = p->ComputeLocalTransform() * cached_global_transform;
            p = p->parent;
        }
        global_dirty = false;
        return cached_global_transform;
    } else {
        return cached_global_transform;
    }
}

void GLTFModel::Node::Update() {
    ComputeGlobalTransform();
    for (auto &child : children) {
        child->Update();
    }
}

// =========================== //
// ---- Animation Sampler ---- //
// =========================== //

GLTFModel::AnimationOutput GLTFModel::AnimationSampler::Sample(float current_time) const {
    Assert(!times.empty(), "Cannot sample invalid animation!");

    if (interpolation_method == AnimationInterpolation::CubicSpline) {
        Assert(output.size() == 3 * times.size(), "Mismatching timestamps!");
    } else {
        Assert(output.size() == times.size(), "Mismatching timestamps!");
    }

    const auto value_at = [&](size_t key) -> const AnimationOutput & {
        return interpolation_method == AnimationInterpolation::CubicSpline
            ? output[key * 3 + 1]
            : output[key];
    };

    if (current_time <= times.front())
        return value_at(0);

    if (current_time >= times.back())
        return value_at(times.size() - 1);

    float normalized_time = 0.0f;
    uint32_t start_index = 0;
    uint32_t end_index = 0;
    float duration = 0.0f;

    for (uint32_t i = 0; i < times.size(); ++i) {
        if (i == times.size() - 1)
            return value_at(i);

        if (current_time == times[i])
            return value_at(i);

        if (current_time >= times[i] && current_time < times[i+1]) {
            duration = times[i+1] - times[i];
            normalized_time = (current_time - times[i]) / duration;
            start_index = i;
            end_index = i + 1;
            break;
        }
    }

    AnimationOutput start_value = value_at(start_index);
    AnimationOutput end_value = value_at(end_index);

    switch (interpolation_method) {
        case AnimationInterpolation::Step: {
            return start_value;
        }
        case AnimationInterpolation::Linear: {
            return std::visit([&](const auto &start) -> AnimationOutput {
                using T = std::decay_t<decltype(start)>;

                const T *end = std::get_if<T>(&end_value);
                if (!end)
                    throw std::runtime_error("Animation output don't match!");

                if constexpr (std::is_same_v<T, glm::quat>) {
                    return glm::slerp(start, *end, normalized_time);
                } else {
                    return glm::mix(start, *end, normalized_time);
                }
            }, start_value);
        }
        case AnimationInterpolation::CubicSpline: {
            AnimationOutput out_tangent = output[start_index * 3 + 2];
            AnimationOutput in_tangent = output[end_index * 3 + 0];
            
            return std::visit([&](const auto &start) -> AnimationOutput {
                using T = std::decay_t<decltype(start)>;

                float t = normalized_time;

                const T *end = std::get_if<T>(&end_value);
                const T *in_tang = std::get_if<T>(&in_tangent);
                const T *out_tang = std::get_if<T>(&out_tangent);
                if (!end || !in_tang || !out_tang)
                    throw std::runtime_error("Animation outputs do not match!");

                const T b_start = *out_tang;
                const T a_end = *in_tang;
                const T v_start = start;
                const T v_end = *end;
                    
                T result = (2*t*t*t - 3*t*t + 1) * v_start
                    + duration * (t*t*t - 2*t*t + t) * b_start
                    + (-2*t*t*t + 3*t*t) * v_end
                    + duration * (t*t*t - t*t) * a_end;

                if constexpr (std::is_same_v<T, glm::quat>) {
                    result = glm::normalize(result);
                }

                return result;
            }, start_value);
        }
    }

    throw std::runtime_error("Could not interpolate with invalid interpolation method!");
}

// ==================== //
// ---- GLTF Model ---- //
// ==================== //

GLTFModel::GLTFModel(const VulkanDevice &device, GPUResourceManager &resource_manager, const std::filesystem::path &file_path)
    : m_device(device)
    , m_resource_manager(resource_manager)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;

    std::string warnings;
    std::string errors;

    std::string ext = file_path.extension().string();
    std::ranges::transform(ext, ext.begin(), ::tolower);

    {
        bool loaded = 0;
        if (ext == ".glb") {
            loaded = loader.LoadBinaryFromFile(&model, &errors, &warnings, file_path.string());
        } else if (ext == ".gltf") {
            loaded = loader.LoadASCIIFromFile(&model, &errors, &warnings, file_path.string());
        } else {
            throw std::runtime_error("Invalid file extension for GLTF!");
        }

        if (!loaded) {
            throw std::runtime_error(std::format("Failed to load GLTF file '{}'.\nErrors: {}\nWarnings: {}",
                    file_path.string(),
                    errors.empty() ? "<none>" : errors,
                    warnings.empty() ? "<none>" : warnings
                )
            );
        }
    }

    if (!warnings.empty())
        std::cerr << "GLTF Warning: " << warnings << "\n";

    if (!errors.empty())
        throw std::runtime_error("GLTF Error: " + errors);

    Assert(model.scenes.size() >= 1, "There are no scenes!");
    const tinygltf::Scene &scene = model.scenes[model.defaultScene == -1 ? 0 : model.defaultScene];

    // Handle default texture + all GLTF textures
    m_textures.resize(model.textures.size() + 1);
    LoadMaterials(model);
    LoadTextures(model);

    VertexData data {};
    m_linear_nodes.resize(model.nodes.size());
    m_root_nodes.reserve(scene.nodes.size());
    for (int root_node_index : scene.nodes) {
        LoadNode(nullptr, root_node_index, model, data);
    }
    LoadAnimations(model);

    for (auto &node : m_root_nodes) {
        node->Update();
    }

    m_vertex_buffer = VulkanBuffer::BufferBuilder(m_device)
        .Size(data.vertices.size() * sizeof(MeshVertex))
        .AddUsage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
        .AddUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
        .AddUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        .Build();
    m_vertex_buffer->Upload(data.vertices);

    m_index_buffer = VulkanBuffer::BufferBuilder(m_device)
        .Size(data.indices.size() * sizeof(uint32_t))
        .AddUsage(VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
        .AddUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
        .Build();
    m_index_buffer->Upload(data.indices);

    std::vector<GPUMaterial> device_materials;
    device_materials.resize(m_materials.size());
    std::ranges::transform(m_materials, device_materials.begin(), [this](const Material &material) {
        const Texture &albedo_texture = m_textures[material.albedo_texture_index];
        const Texture &normal_texture = m_textures[material.normal_texture_index];

        return GPUMaterial {
            .color_factors = material.color_factors,

            .albedo_texture = albedo_texture.srgb_texture_id,
            .albedo_sampler = albedo_texture.sampler_id,
            .albedo_texcoord = material.albedo_texcoord,
            
            .normal_texture = normal_texture.unorm_texture_id,
            .normal_sampler = normal_texture.sampler_id,
            .normal_texcoord = material.normal_texcoord,
            .normal_texture_scale = material.normal_texture_scale,
        };
    });

    m_material_buffer = VulkanBuffer::BufferBuilder(m_device)
        .Size(device_materials.size() * sizeof(GPUMaterial))
        .AddUsage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
        .AddUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
        .AddUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        .Build();
    m_material_buffer->Upload(device_materials);
}

GLTFModel::~GLTFModel() {
    m_vertex_buffer.reset();
    m_index_buffer.reset();
    m_material_buffer.reset();
}

// void GLTFModel::Update() {
//     for (auto &node : m_root_nodes) {
//         node->Update();
//     }
// }

void GLTFModel::PlayAnimation(uint32_t index, float time, bool loop) {
    if (index >= m_animations.size()) {
        throw std::runtime_error("Animation out of bounds!");
    }
    
    const Animation &animation = m_animations[index];

    if (loop) {
        if (animation.duration > 0.0f) {
            time = std::fmod(time, animation.duration);

            if (time < 0.0f)
                time += animation.duration;
        } else {
            time = 0.0f;
        }
    }

    for (const AnimationChannel &channel : animation.channels) {
        Assert(channel.sampler_index < animation.samplers.size(), "Invalid animation sampler!");

        const AnimationSampler &sampler = animation.samplers[channel.sampler_index];

        if (channel.node_index >= m_linear_nodes.size() || !m_linear_nodes[channel.node_index]) {
            continue;
        }
        
        AnimationOutput output = sampler.Sample(time);
        Node &node = *m_linear_nodes[channel.node_index];
        
        switch(channel.target) {
            case AnimationTarget::Translation: {
                if (auto *translation = std::get_if<glm::vec3>(&output); translation != nullptr) {
                    node.translation = *translation;
                    node.MarkDirty();
                } else {
                    throw std::runtime_error(std::format("Cannot sample animation output at time {} as a translation!", time));
                }
                break;
            }
            case AnimationTarget::Rotation: {
                if (auto *rotation = std::get_if<glm::quat>(&output); rotation != nullptr) {
                    node.rotation = *rotation;
                    node.MarkDirty();
                } else {
                    throw std::runtime_error(std::format("Cannot sample animation output at time {} as a rotation!", time));
                }
                break;
            }
            case AnimationTarget::Scale: {
                if (auto *scale = std::get_if<glm::vec3>(&output); scale != nullptr) {
                    node.scale = *scale;
                    node.MarkDirty();
                } else {
                    throw std::runtime_error(std::format("Cannot sample animation output at time {} as a scale!", time));
                }
                break;
            }
            case AnimationTarget::Weights: {
                if (auto *weight = std::get_if<float>(&output); weight != nullptr) {
                    // nothing for now
                } else {
                    throw std::runtime_error(std::format("Cannot sample animation output at time {} as a weight!", time));
                }
                break;
            }
            case AnimationTarget::None:
            default:
                continue;
        }
    }

    for (auto &root_node : m_root_nodes) {
        root_node->Update();
    }
}

void GLTFModel::ResetPositions() {
    ForEachNode([](Node &node) {
        node.matrix = node.initial_matrix;
        node.rotation = node.initial_rotation;
        node.translation = node.initial_translation;
        node.scale = node.initial_scale;
    });

    for (auto &root_node : m_root_nodes) {
        root_node->MarkDirty();
        root_node->Update();
    }
}

void GLTFModel::ForEachNode(const std::function<void(Node &node)> &callback) {
    for (auto &node : m_linear_nodes) {
        if (node)
            callback(*node);
    }
}

void GLTFModel::LoadTextures(tinygltf::Model &model) {
    //// Load Images ////

    m_textures[0].srgb_texture_id = 0;
    m_textures[0].unorm_texture_id = 0;
    m_textures[0].sampler_id = 0;

    struct ImageTextures {
        bool srgb_cached = false;
        TextureId srgb_image = 0;

        bool unorm_cached = false;
        TextureId unorm_image = 0;
    };
    std::vector<ImageTextures> texture_image_cache;
    texture_image_cache.resize(model.images.size());
    
    struct TextureSampler {
        bool cached = false;
        SamplerId sampler = 0;
    };
    std::vector<TextureSampler> sampler_cache;
    sampler_cache.resize(model.samplers.size());

    for (uint32_t i = 0; i < model.textures.size(); ++i) {
        uint32_t texture_index = i + 1;
        tinygltf::Texture &texture = model.textures[i];

        bool needs_srgb = m_textures[texture_index].needs_srgb;
        bool needs_unorm = m_textures[texture_index].needs_unorm;

        if (!needs_srgb && !needs_unorm)
            continue;

        if (texture.sampler == -1) {
            m_textures[texture_index].sampler_id = 0;
        } else if (sampler_cache[texture.sampler].cached) {
            m_textures[texture_index].sampler_id = sampler_cache[texture.sampler].sampler;
        } else {
            // Create new vulkan sampler
            const tinygltf::Sampler &sampler = model.samplers[texture.sampler];

            int mag_filter = sampler.magFilter == -1
                ? TINYGLTF_TEXTURE_FILTER_LINEAR
                : sampler.magFilter;

            int min_filter = sampler.minFilter == -1
                ? TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR
                : sampler.minFilter;

            VkSamplerCreateInfo create_info {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = GetVulkanFilter(mag_filter),
                .minFilter = GetVulkanFilter(min_filter),
                .mipmapMode = GetVulkanMipmapMode(min_filter),
                .addressModeU = GetVulkanWrapMode(sampler.wrapS),
                .addressModeV = GetVulkanWrapMode(sampler.wrapT),
                .maxLod = UseMipMaps(min_filter) ? VK_LOD_CLAMP_NONE : 0.0f,
            };

            VkSampler vulkan_sampler;
            VK_CHECK(vkCreateSampler(m_device.Device(), &create_info, nullptr, &vulkan_sampler));
            m_device.SetDebugName(vulkan_sampler, sampler.name);

            SamplerId id = m_resource_manager.AddSampler(vulkan_sampler);
            sampler_cache[texture.sampler].sampler = id;
            sampler_cache[texture.sampler].cached = true;

            m_textures[texture_index].sampler_id = id;
        }

        if (texture.source == -1) {
            m_textures[texture_index].srgb_texture_id = 0;
            m_textures[texture_index].unorm_texture_id = 0;
            continue;
        }

        tinygltf::Image &image = model.images[texture.source];

        Assert(image.component == 4, "Only RGBA images are supported!");
        Assert(image.bits == 8, "Only 8-bit images are supported!");
        Assert(image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE, "Only unsigned byte images are supported!");

        const auto create_image = [&](VkFormat format, const std::string &debug_tag) -> TextureId {
            auto vulkan_image = VulkanImage::ImageBuilder(m_device)
                .Image2D(image.width, image.height)
                .Format(format)
                .MipMaps()
                .AddUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                .Build();
            vulkan_image->TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            vulkan_image->Upload(image.image.data(), image.image.size());

            m_device.ImmediateSubmit(QueueType::Graphics, [&](const CommandBuffer &cmd) {
                cmd.GenerateMipMaps(*vulkan_image, VK_FILTER_LINEAR);
                cmd.ImageMemoryBarrier(*vulkan_image)
                    .DestAccess(VK_ACCESS_2_SHADER_SAMPLED_READ_BIT)
                    .DestStage(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT)
                    .TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    .Execute();
            });

            std::string debug_name = image.name == ""
                ? std::format("GLTF Image [{}]", texture.source)
                : image.name;

            vulkan_image->SetDebugName(std::format("{} ({})", debug_name, debug_tag));
            return m_resource_manager.AddTexture(vulkan_image);
        };

        auto &cached_image = texture_image_cache[texture.source];

        if (needs_srgb && !cached_image.srgb_cached) {
            cached_image.srgb_image = create_image(m_device.GetNonLinearColorFormat(), "sRGB");
            cached_image.srgb_cached = true;
        }

        if (needs_unorm && !cached_image.unorm_cached) {
            cached_image.unorm_image = create_image(m_device.GetLinearColorFormat(), "UNORM");
            cached_image.unorm_cached = true;
        }

        m_textures[texture_index].srgb_texture_id = cached_image.srgb_image;
        m_textures[texture_index].unorm_texture_id = cached_image.unorm_image;
    }
}

void GLTFModel::LoadMaterials(tinygltf::Model &model) {
    m_materials.reserve(model.materials.size() + 1);

    m_materials.push_back(Material {
        .color_factors = glm::vec4(1.0f),
        .albedo_texture_index = 0,
        .normal_texture_index = 0,
    });
    for (auto &material : model.materials) {
        auto base_color = material.pbrMetallicRoughness.baseColorFactor;

        uint32_t albedo_texture_index = material.pbrMetallicRoughness.baseColorTexture.index + 1;
        uint32_t normal_texture_index = material.normalTexture.index + 1;

        m_textures[albedo_texture_index].needs_srgb = true;
        m_textures[normal_texture_index].needs_unorm = true;

        uint32_t albedo_texcoord = material.pbrMetallicRoughness.baseColorTexture.texCoord;
        uint32_t normal_texcoord = material.normalTexture.texCoord;

        m_materials.push_back(Material {
            .color_factors = glm::make_vec4(base_color.data()),
            .albedo_texture_index = static_cast<uint32_t>(albedo_texture_index),
            .albedo_texcoord = albedo_texcoord,
            .normal_texture_index = static_cast<uint32_t>(normal_texture_index),
            .normal_texcoord = normal_texcoord,
            .normal_texture_scale = static_cast<float>(material.normalTexture.scale),
        });

        std::cout << "TEXTURE SCALE: " << material.normalTexture.scale << "\n";
    }
}

void GLTFModel::LoadNode(Node *parent, uint32_t node_index, tinygltf::Model &model, VertexData &data) {
    if (node_index < 0 || static_cast<size_t>(node_index) >= model.nodes.size())
        throw std::runtime_error("Canot load out of bounds node!");
    
    auto translation = glm::dvec3(0.0);
    auto rotation = glm::dquat(1.0, 0.0, 0.0, 0.0);
    auto scale = glm::dvec3(1.0);
    auto matrix = glm::dmat4(1.0);

    tinygltf::Node &node = model.nodes[node_index];

    if (node.translation.size() == 3)
        translation = glm::make_vec3(node.translation.data());
    if (node.rotation.size() == 4)
        rotation = glm::dquat::wxyz(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
    if (node.scale.size() == 3)
        scale = glm::make_vec3(node.scale.data());
    if (node.matrix.size() == 16)
        matrix = glm::make_mat4(node.matrix.data());

    std::shared_ptr<Node> current_node = std::make_shared<Node>(translation, rotation, scale, matrix);
    current_node->parent = parent;

    m_linear_nodes[node_index] = current_node;

    for (auto &child_index : node.children) {
        LoadNode(current_node.get(), child_index, model, data);
    }

    if (node.mesh != -1) {
        tinygltf::Mesh &mesh = model.meshes[node.mesh];
        Mesh constructed_mesh {};
        
        for (tinygltf::Primitive &primitive : mesh.primitives) {
            Assert(primitive.mode == TINYGLTF_MODE_TRIANGLES, "We currently only support triangle primitives!");
            
            std::vector<MeshVertex> primitive_vertices;
            std::vector<uint32_t> primitive_indices;

            bool has_normals = primitive.attributes.contains("NORMAL");
            bool generate_indices = primitive.indices == -1;

            const auto material_index = static_cast<uint32_t>(primitive.material + 1);
            bool has_normal_texture = (material_index > 0) && (m_materials[material_index].normal_texture_index > 0);
        
            //// Indices ////
            if (!generate_indices) {
                tinygltf::Accessor &index_accessor = model.accessors[primitive.indices];
                primitive_indices.reserve(primitive_indices.size() + index_accessor.count);

                IterateAccessor(model, index_accessor, [&](uint32_t i, const std::vector<ComponentType> &value, int accessor_type) {
                    uint32_t vertex_index;
                    if (auto u8_val = TryGetScalar<uint8_t>(value, accessor_type)) {
                        vertex_index = *u8_val;
                    } else if (auto u16_val = TryGetScalar<uint16_t>(value, accessor_type)) {
                        vertex_index = *u16_val;
                    } else if (auto u32_val = TryGetScalar<uint32_t>(value, accessor_type)) {
                        vertex_index = *u32_val;
                    } else {
                        throw std::runtime_error("Invalid triangle index!");
                    }

                    primitive_indices.push_back(static_cast<uint32_t>(vertex_index));
                });
            }

            //// Positions ////
            {
                Assert(primitive.attributes.contains("POSITION"), "Cannot generate a GLTF mesh without positions!");
                tinygltf::Accessor &position_accessor = model.accessors[primitive.attributes["POSITION"]];
                primitive_vertices.resize(primitive_vertices.size() + position_accessor.count);

                // If indices have not been generated, we will generate them here manually
                IterateAccessor(model, position_accessor, [&](uint32_t i, const std::vector<ComponentType> &value, int accessor_type) {
                    if (auto vec = TryGetVector<3, float>(value, accessor_type)) {
                        MeshVertex new_vertex {
                            .position = *vec,
                            .normal = glm::vec3(0.0f, 0.0f, 1.0f),
                            .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
                            .uv0 = glm::vec2(0.0f),
                            .uv1 = glm::vec2(0.0f),
                            .color = glm::vec4(1.0f),
                            .joints0 = glm::uvec2(0),
                            .weights0 = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
                        };

                        if (generate_indices) {
                            primitive_indices.push_back(static_cast<uint32_t>(i));
                        }
                        primitive_vertices[i] = new_vertex;
                    } else {
                        throw std::runtime_error("Could not access position!");
                    }
                });
            }

            //// Normals ////
            if (has_normals) {
                tinygltf::Accessor &normal_accessor = model.accessors[primitive.attributes["NORMAL"]];
                IterateAccessor(model, normal_accessor, [&](uint32_t i, const std::vector<ComponentType> &value, int accessor_type) {
                    if (auto vec = TryGetVector<3, float>(value, accessor_type)) {
                        primitive_vertices[i].normal = glm::normalize(*vec);
                    } else {
                        throw std::runtime_error("Could not access normal!");
                    }
                });
            }
            

            //// Tangents ////
            bool has_tangents = primitive.attributes.contains("TANGENT");
            if (has_tangents && has_normals) {
                tinygltf::Accessor &tangent_accessor = model.accessors[primitive.attributes["TANGENT"]];
                IterateAccessor(model, tangent_accessor, [&](uint32_t i, const std::vector<ComponentType> &value, int accessor_type) {
                    if (auto vec = TryGetVector<4, float>(value, accessor_type)) {
                        primitive_vertices[i].tangent = *vec;
                    } else {
                        throw std::runtime_error("Could not access tangent!");
                    }
                });
            }

            //// Texture Coords ////
            if (primitive.attributes.contains("TEXCOORD_0")) {
                tinygltf::Accessor &uv_accessor = model.accessors[primitive.attributes["TEXCOORD_0"]];
                IterateAccessor(model, uv_accessor, [&](uint32_t i, const std::vector<ComponentType> &value, int accessor_type) {
                    if (auto vec = TryGetVector<2, float>(value, accessor_type)) {
                        primitive_vertices[i].uv0 = *vec;
                    } else {
                        throw std::runtime_error("Could not access UV0!");
                    }
                });
            }

            if (primitive.attributes.contains("TEXCOORD_1")) {
                tinygltf::Accessor &uv_accessor = model.accessors[primitive.attributes["TEXCOORD_1"]];
                IterateAccessor(model, uv_accessor, [&](uint32_t i, const std::vector<ComponentType> &value, int accessor_type) {
                    if (auto vec = TryGetVector<2, float>(value, accessor_type)) {
                        primitive_vertices[i].uv1 = *vec;
                    } else {
                        throw std::runtime_error("Could not access UV1!");
                    }
                });
            }

            //// Color ////
            if (primitive.attributes.contains("COLOR_0")) {
                tinygltf::Accessor &color_accessor = model.accessors[primitive.attributes["COLOR_0"]];
                IterateAccessor(model, color_accessor, [&](uint32_t i, const std::vector<ComponentType> &value, int accessor_type) {
                    if (auto vec3 = TryGetVector<3, float>(value, accessor_type)) {
                        primitive_vertices[i].color = glm::clamp(glm::vec4(*vec3, 1.0f), 0.0f, 1.0f);
                    } else if (auto vec4 = TryGetVector<4, float>(value, accessor_type)) {
                        primitive_vertices[i].color = glm::clamp(*vec4, 0.0f, 1.0f);
                    } else {
                        throw std::runtime_error("Could not access color 0!");
                    }
                });
            }

            //// Joints ////
            if (primitive.attributes.contains("JOINTS_0")) {
                tinygltf::Accessor &color_accessor = model.accessors[primitive.attributes["JOINTS_0"]];
                IterateAccessor(model, color_accessor, [&](uint32_t i, const std::vector<ComponentType> &value, int accessor_type) {
                    if (auto u8_vec = TryGetVector<4, uint8_t>(value, accessor_type)) {
                        primitive_vertices[i].joints0 = *u8_vec;
                    } else if (auto u16_vec = TryGetVector<4, uint16_t>(value, accessor_type)) {
                        primitive_vertices[i].joints0 = *u16_vec;
                    } else {
                        throw std::runtime_error("Could not access joint 0!");
                    }
                });
            }

            //// Weights ////
            if (primitive.attributes.contains("WEIGHTS_0")) {
                tinygltf::Accessor &weight_accessor = model.accessors[primitive.attributes["WEIGHTS_0"]];
                IterateAccessor(model, weight_accessor, [&](uint32_t i, const std::vector<ComponentType> &value, int accessor_type) {
                    if (auto vec = TryGetVector<4, float>(value, accessor_type)) {
                        primitive_vertices[i].weights0 = *vec;
                    } else {
                        throw std::runtime_error("Could not access weight 0!");
                    }
                });
            }

            if (!has_normals) {
                //// Generate flat normals ////
                std::cout << "Could not find supplied vertex normals! Generating flat normals.\n";

                std::vector<MeshVertex> replaced_vertices;
                std::vector<uint32_t> replaced_indices;

                replaced_vertices.reserve(primitive_indices.size());
                replaced_indices.reserve(primitive_indices.size());

                for (uint32_t i = 0; i < primitive_indices.size(); i += 3) {
                    // For each triangle, calculate the flat normal

                    MeshVertex v0 = primitive_vertices[primitive_indices[i]];
                    MeshVertex v1 = primitive_vertices[primitive_indices[i + 1]];
                    MeshVertex v2 = primitive_vertices[primitive_indices[i + 2]];

                    glm::vec3 normal = glm::cross(v1.position - v0.position, v2.position - v0.position);
                    if (NearlyEqual(normal, glm::vec3(0.0f)))
                        normal = glm::vec3(0.0f, 0.0f, 1.0f);
                    normal = glm::normalize(normal);

                    v0.normal = normal;
                    v1.normal = normal;
                    v2.normal = normal;

                    // Generate new vertices to avoid degenerate vertex normals
                    auto first_index = static_cast<uint32_t>(replaced_vertices.size());

                    replaced_indices.push_back(first_index + 0);
                    replaced_indices.push_back(first_index + 1);
                    replaced_indices.push_back(first_index + 2);

                    replaced_vertices.push_back(v0);
                    replaced_vertices.push_back(v1);
                    replaced_vertices.push_back(v2);
                }

                primitive_indices = std::move(replaced_indices);
                primitive_vertices = std::move(replaced_vertices);
            }

            if (!has_tangents && has_normal_texture && has_normals) {
                //// Generate tangents using MikkTSpace algorithm ////
                std::cout << "Could not find supplied vertex tangents! Generating default tangents.\n";

                uint32_t normal_uvs = m_materials[material_index].normal_texcoord;
                GenerateTangents(primitive_vertices, primitive_indices, normal_uvs);
            }

            //// Push back to global data ////

            const auto index_count = static_cast<uint32_t>(primitive_indices.size());
            const auto initial_index = static_cast<uint32_t>(data.indices.size());
            const auto initial_vertex = static_cast<uint32_t>(data.vertices.size());

            std::ranges::transform(primitive_indices, primitive_indices.begin(), [initial_vertex](uint32_t local_index) {
                return local_index + initial_vertex;
            });
            data.indices.insert(data.indices.end(), primitive_indices.begin(), primitive_indices.end());
            data.vertices.insert(data.vertices.end(), primitive_vertices.begin(), primitive_vertices.end());

            constructed_mesh.primitives.push_back(Primitive {
                .start_index = initial_index,
                .index_count = index_count,
                .material_index = material_index,
            });
        }

        current_node->mesh = constructed_mesh;
    }

    if (!parent) {
        m_root_nodes.push_back(current_node);
    } else {
        parent->children.push_back(current_node);
    }
}

void GLTFModel::LoadAnimations(tinygltf::Model &model) {
    std::cout << "The animations for this model are: \n";
    for (uint32_t i = 0; i < model.animations.size(); ++i) {
        std::cout << "[" << i << "] " << model.animations[i].name << "\n";
    }

    for (const auto &animation : model.animations) {
        std::vector<AnimationChannel> channels {};
        channels.reserve(animation.channels.size());

        std::vector<AnimationSampler> samplers {};
        samplers.reserve(animation.samplers.size());

        float end = 0.0f;

        for (const auto &channel : animation.channels) {
            if (channel.target_node == -1 || static_cast<size_t>(channel.target_node) >= m_linear_nodes.size() || !m_linear_nodes[channel.target_node]) {
                continue;
            }

            if (channel.sampler < 0 || static_cast<size_t>(channel.sampler) >= animation.samplers.size()) {
                throw std::runtime_error("Animation channel sampler index is out of bounds!");
            }

            AnimationTarget target = AnimationTarget::None;
            if (channel.target_path == "translation") {
                target = AnimationTarget::Translation;
            } else if (channel.target_path == "rotation") {
                target = AnimationTarget::Rotation;
            } else if (channel.target_path == "scale") {
                target = AnimationTarget::Scale;
            } else if (channel.target_path == "weights") {
                target = AnimationTarget::Weights;
                std::cerr << "Warning: Morph animations are not currently supported!\n";
                continue;
            } else {
                std::cerr << "Warning: Invalid animation target '" << channel.target_path << "'\n";
                continue;
            }

            if (target == AnimationTarget::None) {
                std::cerr << "Warning: Attempted to target [None] for animation!\n";
            }

            channels.push_back(AnimationChannel{
                .sampler_index = static_cast<uint32_t>(channel.sampler),
                .node_index = static_cast<uint32_t>(channel.target_node),
                .target = target,
            });
        }

        for (auto &sampler : animation.samplers) {
            AnimationInterpolation interpolation = AnimationInterpolation::Linear;
            if (sampler.interpolation == "LINEAR") {
                interpolation = AnimationInterpolation::Linear;
            } else if (sampler.interpolation == "STEP") {
                interpolation = AnimationInterpolation::Step;
            } else if (sampler.interpolation == "CUBICSPLINE") {
                interpolation = AnimationInterpolation::CubicSpline;
            } else {
                throw std::runtime_error(std::format("Invalid interpolation method '{}'.", sampler.interpolation));
            }

            tinygltf::Accessor &time_accessor = model.accessors[sampler.input];
            tinygltf::Accessor &output_accessor = model.accessors[sampler.output];

            std::vector<float> times;
            times.resize(time_accessor.count);

            IterateAccessor(model, time_accessor, [&](uint32_t i, const std::vector<ComponentType> &value, int accessor_type) {
                if (auto time = TryGetScalar<float>(value, accessor_type)) {
                    times[i] = *time;
                } else {
                    throw std::runtime_error("Time accessor for animation must contain floats!");
                }
            });

            std::vector<AnimationOutput> output;
            output.resize(output_accessor.count);

            IterateAccessor(model, output_accessor, [&](uint32_t i, const std::vector<ComponentType> &value, int accessor_type) {
                if (auto f = TryGetScalar<float>(value, accessor_type)) {
                    // weights
                    output[i] = *f;
                } else if (auto vec3 = TryGetVector<3, float>(value, accessor_type)) {
                    // translation / scale
                    output[i] = *vec3;
                } else if (auto vec4 = TryGetVector<4, float>(value, accessor_type)) {
                    // rotation
                    glm::vec4 rotation = *vec4;
                    output[i] = glm::quat(rotation[3], rotation[0], rotation[1], rotation[2]);
                } else {
                    throw std::runtime_error("Invalid type for animation outputs!");
                }
            });

            samplers.push_back(AnimationSampler {
                .interpolation_method = interpolation,
                .times = times,
                .output = output,
            });
        }

        for (const AnimationChannel &channel : channels) {
            const AnimationSampler &sampler = samplers[channel.sampler_index];

            end = std::max(end, sampler.times.back());
        }

        m_animations.push_back(Animation {
            .name = animation.name,
            .channels = channels,
            .samplers = samplers,
            .duration = end,
        });
    }
}

void GLTFModel::GenerateTangents(std::vector<MeshVertex> &primitive_vertices, std::vector<uint32_t> &primitive_indices, uint32_t uv_set) {
    if (uv_set > 1) {
        throw std::runtime_error("Could not generate tangents since only TEXCOORD_0 and TEXCOORD_1 are supported");
    }

    struct MikkUserData {
        const std::vector<MeshVertex> *vertices;
        const std::vector<uint32_t> *indices;
        std::vector<glm::vec4> *out_tangents;
        uint32_t uv_set;
    };

    std::vector<glm::vec4> tangents(primitive_indices.size());
    MikkUserData user_data {
        .vertices = &primitive_vertices,
        .indices = &primitive_indices,
        .out_tangents = &tangents,
        .uv_set = uv_set,
    };

    SMikkTSpaceInterface inferface {
        .m_getNumFaces = [](const SMikkTSpaceContext *context) -> int {
            const auto *data = static_cast<const MikkUserData *>(context->m_pUserData);
            return static_cast<int>(data->indices->size() / 3);
        },
        .m_getNumVerticesOfFace = [](const SMikkTSpaceContext *context, int) ->int {
            return 3;
        },
        .m_getPosition = [](const SMikkTSpaceContext *context, float position_out[3], int face, int vert) {
            const auto *data = static_cast<const MikkUserData *>(context->m_pUserData);
            const uint32_t index = (*data->indices)[face * 3 + vert];

            const glm::vec3 &position = (*data->vertices)[index].position;
            position_out[0] = position.x;
            position_out[1] = position.y;
            position_out[2] = position.z;
        },
        .m_getNormal = [](const SMikkTSpaceContext *context, float normal_out[3], int face, int vert) {
            const auto *data = static_cast<const MikkUserData *>(context->m_pUserData);
            const uint32_t index = (*data->indices)[face * 3 + vert];

            const glm::vec3 &normal = (*data->vertices)[index].normal;
            normal_out[0] = normal.x;
            normal_out[1] = normal.y;
            normal_out[2] = normal.z;
        },
        .m_getTexCoord = [](const SMikkTSpaceContext *context, float uv_out[2], int face, int vert) {
            const auto *data = static_cast<const MikkUserData *>(context->m_pUserData);
            const uint32_t index = (*data->indices)[face * 3 + vert];
            const MeshVertex &vertex = (*data->vertices)[index];

            const glm::vec2 &uv = data->uv_set == 0 ? vertex.uv0 : vertex.uv1;
            uv_out[0] = uv.x;
            uv_out[1] = uv.y;
        },
        .m_setTSpaceBasic = [](const SMikkTSpaceContext *context, const float tangent[3], float sign, int face, int vert) {
            const auto *data = static_cast<const MikkUserData *>(context->m_pUserData);
            (*data->out_tangents)[face * 3 + vert] = glm::vec4(tangent[0], tangent[1], tangent[2], sign);
        },
    };

    SMikkTSpaceContext context {
        .m_pInterface = &inferface,
        .m_pUserData = &user_data,
    };

    if (genTangSpaceDefault(&context) == 0) {
        throw std::runtime_error("Failed to generate tangents!");
    }

    //// Generate new vertices to avoid degenerate tangents ////
    std::vector<MeshVertex> replaced_vertices;
    std::vector<uint32_t> replaced_indices;

    replaced_vertices.reserve(primitive_indices.size());
    replaced_indices.reserve(primitive_indices.size());

    for (uint32_t i = 0; i < primitive_indices.size(); ++i) {
        MeshVertex vertex = primitive_vertices[primitive_indices[i]];
        vertex.tangent = tangents[i];

        replaced_indices.push_back(static_cast<uint32_t>(replaced_vertices.size()));
        replaced_vertices.push_back(vertex);
    }

    primitive_vertices = std::move(replaced_vertices);
    primitive_indices = std::move(replaced_indices);
}

void GLTFModel::IterateAccessor(tinygltf::Model &model, tinygltf::Accessor &accessor, const std::function<void(uint32_t, std::vector<ComponentType>, int)> &callback) {
    const auto get_component_value = [](const std::vector<unsigned char> &data, uint32_t byte_offset, int component_type, bool normalized) -> ComponentType {
        // Note: glTF uses little-endian buffer access

        if (byte_offset >= data.size())
            throw std::runtime_error("Out of buffer range!");

        switch (component_type) {
            case TINYGLTF_COMPONENT_TYPE_BYTE: {
                auto value = std::bit_cast<int8_t>(data[byte_offset]);
                if (normalized)
                    return std::max(static_cast<float>(value) / 127.0f, -1.0f);
                return value;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                uint8_t value = data[byte_offset];
                if (normalized)
                    return static_cast<float>(value) / 255.0f;
                return value;
            }
            case TINYGLTF_COMPONENT_TYPE_SHORT: {
                auto value0 = static_cast<uint16_t>(data[byte_offset + 0]);
                auto value1 = static_cast<uint16_t>(data[byte_offset + 1]);
                uint16_t unsigned_value = value0 | (value1 << 8);
                auto value = std::bit_cast<int16_t>(unsigned_value);
                if (normalized)
                    return std::max(static_cast<float>(value) / 32767.0f, -1.0f);
                return value;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                auto value0 = static_cast<uint16_t>(data[byte_offset + 0]);
                auto value1 = static_cast<uint16_t>(data[byte_offset + 1]);
                uint16_t value = value0 | (value1 << 8);
                if (normalized)
                    return static_cast<float>(value) / 65535.0f;
                return value;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                if (normalized)
                    throw std::runtime_error("UNSIGNED_INT accessor cannot be normalized!");

                auto value0 = static_cast<uint32_t>(data[byte_offset + 0]);
                auto value1 = static_cast<uint32_t>(data[byte_offset + 1]);
                auto value2 = static_cast<uint32_t>(data[byte_offset + 2]);
                auto value3 = static_cast<uint32_t>(data[byte_offset + 3]);

                uint32_t value = value0 | (value1 << 8) | (value2 << 16) | (value3 << 24);
                return value;
            }
            case TINYGLTF_COMPONENT_TYPE_FLOAT: {
                if (normalized)
                    throw std::runtime_error("FLOAT accessor cannot be normalized!");

                auto value0 = static_cast<uint32_t>(data[byte_offset + 0]);
                auto value1 = static_cast<uint32_t>(data[byte_offset + 1]);
                auto value2 = static_cast<uint32_t>(data[byte_offset + 2]);
                auto value3 = static_cast<uint32_t>(data[byte_offset + 3]);

                uint32_t value = value0 | (value1 << 8) | (value2 << 16) | (value3 << 24);
                return std::bit_cast<float>(value);
            }
            default:
                throw std::runtime_error("Unable to consume value in accessor!");
        }
    };

    const auto get_zero_component = [&](int component_type) -> ComponentType {
        switch (component_type) {
            case TINYGLTF_COMPONENT_TYPE_BYTE: {
                if (accessor.normalized) return float();
                return int8_t();
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                if (accessor.normalized) return float();
                return uint8_t();
            }
            case TINYGLTF_COMPONENT_TYPE_SHORT: {
                if (accessor.normalized) return float();
                return int16_t();
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                if (accessor.normalized) return float();
                return uint16_t();
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                return uint32_t();
            }
            case TINYGLTF_COMPONENT_TYPE_FLOAT: {
                return float();
            }
            default:
                throw std::runtime_error("Unable to consume value in accessor!");
        }
    };

    const size_t stride_size = GetStrideSize(accessor.type, accessor.componentType);

    const auto get_element = [&](const std::vector<unsigned char> &data, uint32_t byte_offset, int accessor_type, int component_type) -> std::vector<ComponentType> {
        const size_t component_count = GetAccessorTypeCount(accessor_type); 
        const size_t component_size = GetComponentSize(component_type);
        
        std::vector<ComponentType> value {};
        value.reserve(component_count);

        // Account for padding as per Sec. 3.6.2.4 of gltf spec
        if (accessor_type == TINYGLTF_TYPE_MAT2 && component_size == 1) {
            const uint32_t padding_size = 4;
            for (uint32_t i = 0; i < 2; ++i) {
                for (uint32_t j = 0; j < 2; ++j) {
                    uint32_t c = static_cast<uint32_t>(i * padding_size + j * component_size);
                    value.push_back(get_component_value(data, byte_offset + c, component_type, accessor.normalized));
                }
            }
        } else if (accessor_type == TINYGLTF_TYPE_MAT3 && component_size == 1) {
            const uint32_t padding_size = 4;
            for (uint32_t i = 0; i < 3; ++i) {
                for (uint32_t j = 0; j < 3; ++j) {
                    auto c = static_cast<uint32_t>(i * padding_size + j * component_size);
                    value.push_back(get_component_value(data, byte_offset + c, component_type, accessor.normalized));
                }
            }
        } else if (accessor_type == TINYGLTF_TYPE_MAT3 && component_size == 2) {
            const uint32_t padding_size = 8;
            for (uint32_t i = 0; i < 3; ++i) {
                for (uint32_t j = 0; j < 3; ++j) {
                    auto c = static_cast<uint32_t>(i * padding_size + j * component_size);
                    value.push_back(get_component_value(data, byte_offset + c, component_type, accessor.normalized));
                }
            }
        } else {
            for (uint32_t c = 0; c < component_count; ++c) {
                value.push_back(get_component_value(data, static_cast<uint32_t>(byte_offset + c * component_size), component_type, accessor.normalized));
            }
        }

        return value;
    };

    const auto get_zero_element = [&](int accessor_type, int component_type) -> std::vector<ComponentType> {
        const uint32_t num_components = GetAccessorTypeCount(accessor_type);

        std::vector<ComponentType> components {};
        components.reserve(num_components);

        for (uint32_t i = 0; i < num_components; ++i) {
            components.push_back(get_zero_component(component_type));
        }
        return components;
    };

    if (!accessor.sparse.isSparse) {
        //// Regular Access ////

        if (accessor.bufferView == -1) {
            for (uint32_t i = 0; i < accessor.count; ++i) {
                callback(i, get_zero_element(accessor.type, accessor.componentType), accessor.type);
            }
            return;
        }

        tinygltf::BufferView &buffer_view = model.bufferViews[accessor.bufferView];
        tinygltf::Buffer &buffer = model.buffers[buffer_view.buffer];
    
        size_t offset = buffer_view.byteOffset + accessor.byteOffset;
        size_t stride = buffer_view.byteStride == 0 ? stride_size : buffer_view.byteStride;
    
        for (uint32_t i = 0; i < accessor.count; ++i) {
            const auto element_offset = static_cast<uint32_t>(offset + i * stride);
            callback(i, get_element(buffer.data, element_offset, accessor.type, accessor.componentType), accessor.type);
        }
        return;
    }
    
    //// Sparse Access ////

    // indices from main array to replace via these values
    std::unordered_map<uint32_t, std::vector<ComponentType>> sparse_replacements;

    tinygltf::BufferView &indices_view = model.bufferViews[accessor.sparse.indices.bufferView];
    tinygltf::Buffer &indices = model.buffers[indices_view.buffer];

    tinygltf::BufferView &values_view = model.bufferViews[accessor.sparse.values.bufferView];
    tinygltf::Buffer &values = model.buffers[values_view.buffer];

    auto index_offset = static_cast<uint32_t>(accessor.sparse.indices.byteOffset + indices_view.byteOffset);
    auto value_offset = static_cast<uint32_t>(accessor.sparse.values.byteOffset + values_view.byteOffset);

    for (int i = 0; i < accessor.sparse.count; ++i) {
        uint32_t value_element_offset = static_cast<uint32_t>(value_offset + i * stride_size);
        uint32_t sparse_index_offset = index_offset + i * GetComponentSize(accessor.sparse.indices.componentType);

        // Grab the index and convert to uint32
        ComponentType gltf_index = get_component_value(indices.data, sparse_index_offset, accessor.sparse.indices.componentType, false);
        uint32_t index = 0;
        if (const auto *u8_value = std::get_if<uint8_t>(&gltf_index)) {
            index = *u8_value;
        } else if (const auto *u16_value = std::get_if<uint16_t>(&gltf_index)) {
            index = *u16_value;
        } else if (const auto *u32_value = std::get_if<uint32_t>(&gltf_index)) {
            index = *u32_value;
        } else {
            throw std::runtime_error("Invalid sparse accessor index type!");
        }

        sparse_replacements[index] = get_element(values.data, value_element_offset, accessor.type, accessor.componentType);
    }

    if (accessor.bufferView == -1) {
        for (uint32_t i = 0; i < accessor.count; ++i) {
            if (sparse_replacements.contains(i)) {
                callback(i, sparse_replacements[i], accessor.type);
                continue;
            }
            callback(i, get_zero_element(accessor.type, accessor.componentType), accessor.type);
        }
        return;
    }

    tinygltf::BufferView &buffer_view = model.bufferViews[accessor.bufferView];
    tinygltf::Buffer &buffer = model.buffers[buffer_view.buffer];

    size_t offset = buffer_view.byteOffset + accessor.byteOffset;
    size_t stride = buffer_view.byteStride == 0 ? stride_size : buffer_view.byteStride;

    for (uint32_t i = 0; i < accessor.count; ++i) { 
        if (sparse_replacements.contains(i)) {
            callback(i, sparse_replacements[i], accessor.type);
            continue;
        }
        const auto element_offset = static_cast<uint32_t>(offset + i * stride);
        callback(i, get_element(buffer.data, element_offset, accessor.type, accessor.componentType), accessor.type);
    }
}

template <typename T>
std::optional<T> GLTFModel::TryGetScalar(const std::vector<ComponentType> &accessor_value, int accessor_type) {
    if (accessor_type != TINYGLTF_TYPE_SCALAR || accessor_value.size() != 1)
        return std::nullopt;

    const auto *component = std::get_if<T>(&accessor_value[0]);
    if (component == nullptr)
        return std::nullopt;

    return *component;
}

template <glm::length_t L, typename T>
std::optional<glm::vec<L, T>> GLTFModel::TryGetVector(const std::vector<ComponentType> &accessor_value, int accessor_type) {
    if ((L == 2 && accessor_type != TINYGLTF_TYPE_VEC2)
        || (L == 3 && accessor_type != TINYGLTF_TYPE_VEC3)
        || (L == 4 && accessor_type != TINYGLTF_TYPE_VEC4)
        || (L < 2 || L > 4)
        || accessor_value.size() != L
    )
        return std::nullopt;

    glm::vec<L, T> value {};

    for (glm::length_t i = 0; i < L; ++i) {
        const auto *component = std::get_if<T>(&accessor_value[i]);
        if (component == nullptr)
            return std::nullopt;

        value[i] = *component;
    }

    return value;
}

template <glm::length_t C, glm::length_t R, typename T>
std::optional<glm::mat<C, R, T>> GLTFModel::TryGetMatrix(const std::vector<ComponentType> &accessor_value, int accessor_type) {
    if (C != R
        || (C == 2 && accessor_type != TINYGLTF_TYPE_MAT2)
        || (C == 3 && accessor_type != TINYGLTF_TYPE_MAT3)
        || (C == 4 && accessor_type != TINYGLTF_TYPE_MAT4)
        || (C < 2 || C > 4)
        || accessor_value.size() != C * R
    )
        return std::nullopt;

    glm::mat<C, R, T> value {};

    for (glm::length_t c = 0; c < C; ++c) {
        for (glm::length_t r = 0; r < R; ++r) {
            const auto *component = std::get_if<T>(&accessor_value[c * R + r]);
            if (component == nullptr)
                return std::nullopt;

            value[c][r] = *component;
        }
    }

    return value;
}

VkFilter GLTFModel::GetVulkanFilter(int filter) {
    switch (filter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
            return VK_FILTER_NEAREST;
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            return VK_FILTER_LINEAR;
        default:
            return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode GLTFModel::GetVulkanMipmapMode(int min_filter) {
    switch (min_filter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;

        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;

        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

bool GLTFModel::UseMipMaps(int min_filter) {
    switch (min_filter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            return true;

        default:
            return false;
    }
}

VkSamplerAddressMode GLTFModel::GetVulkanWrapMode(int wrap_mode) {
    switch (wrap_mode) {
        case TINYGLTF_TEXTURE_WRAP_REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;

        case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

        default:
            Assert(false, "Invalid glTF texture wrap mode");
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

uint32_t GLTFModel::GetComponentSize(int component_type) {
    switch (component_type) {
        case TINYGLTF_COMPONENT_TYPE_BYTE:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return 1;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            return 2;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            return 4;
        default:
            throw std::runtime_error("Cannot get size of this component type!");
    }
}

uint32_t GLTFModel::GetAccessorTypeCount(int accessor_type) {
    switch (accessor_type) {
        case TINYGLTF_TYPE_SCALAR:
            return 1;
        case TINYGLTF_TYPE_VEC2:
            return 2;
        case TINYGLTF_TYPE_VEC3:
            return 3;
        case TINYGLTF_TYPE_VEC4:
            return 4;
        case TINYGLTF_TYPE_MAT2:
            return 4;
        case TINYGLTF_TYPE_MAT3:
            return 9;
        case TINYGLTF_TYPE_MAT4:
            return 16;
        default:
            throw std::runtime_error("Cannot get component count of this accessor type!");
    }
}

uint32_t GLTFModel::GetStrideSize(int accessor_type, int component_type) {
    uint32_t accessor_size = GetAccessorTypeCount(accessor_type);
    uint32_t component_size = GetComponentSize(component_type);
    
    if (accessor_type == TINYGLTF_TYPE_MAT2 && component_size == 1) {
        return 2 * 4;
    } else if (accessor_type == TINYGLTF_TYPE_MAT3 && component_size == 1) {
        return 3 * 4;
    } else if (accessor_type == TINYGLTF_TYPE_MAT3 && component_size == 2) {
        return 3 * 8;
    } else {
        return accessor_size * component_size;
    }
}

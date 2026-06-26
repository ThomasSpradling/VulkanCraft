#pragma once
// #include "../game.h"
#include "../Application/Application.h"
#include "../Events/EventListener.h"
#include "../Graphics/gpu_structs.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include "../Graphics/GLTFModel.h"
#include "../Graphics/Renderable.h"
#include "ChunkRenderer.h"

#include "../Platform/Sockets/SocketAPI.h"
#include "../Network/NetworkHost.h"

struct SceneData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 view_projection;
    glm::vec4 ambient_color;
    glm::vec4 sunlight_direction;
    glm::vec4 sunlight_color;
};

class GameClient : public IGame {
public:
    virtual void Initialize() override;
    virtual void ShutDown() override;
    virtual void Update(float delta_time) override;
    virtual void Render(std::optional<Frame> frame, float delta_time) override;
private:
    struct PerFrameData {
        VkDescriptorSet global_descriptor_set = VK_NULL_HANDLE;
        VkBuffer global_uniform_buffer = VK_NULL_HANDLE;
        VmaAllocation global_uniform_buffer_alloc = VK_NULL_HANDLE;

        VkImage depth_image;
        VkImageView depth_image_view;
        VmaAllocation depth_image_alloc;
    };

    struct PushConstantData {
        glm::mat4 model;
        VkDeviceAddress vertex_buffer;
    };

    struct Player {
        uint32_t id;
        glm::vec3 position;
        float yaw, pitch;
    };
private:
    // float m_yaw = 90.0f;
    // float m_pitch = 0.0f;
    glm::vec3 m_player_direction { 0.0f, 0.0f, 0.0f };

    // Temporary for input testing
    glm::vec3 m_current_color = { 164.0f/256.0f, 30.0f/256.0f, 34.0f/256.0f };

    std::shared_ptr<GLTFModel> m_model;

    VkDescriptorSetLayout m_global_layout = VK_NULL_HANDLE;
    GLTFCoreData m_gltf_common_data;

    PushConstantData m_push_constants;

    std::unique_ptr<DescriptorAllocator> m_descriptor_allocator; 

    std::vector<PerFrameData> m_frame_data;
    SceneData m_scene_data;

    DrawContext m_draw_context;

    // std::vector<Chunk> m_chunks;
    std::unique_ptr<ChunkRenderer> m_chunk_renderer;

    double m_network_send_timer = 0.0;
    double m_heartbeat_timer = 0.0;
    const int NETWORK_INPUT_SEND_RATE = 10; // Hz
    const int HEART_RATE = 1; // Hz

    std::vector<Player> m_players;

    glm::vec3 m_current_position;
    float m_current_yaw = 90.0f;
    float m_current_pitch = 0.0f;

    std::unique_ptr<SocketAPI> m_socket_api {};
    std::unique_ptr<NetworkHost> m_client {};
    PeerId m_server {};
    const ChannelId m_basic_channel = 1;
private:
    void InitRenderTargets();
    void DestroyRenderTargets();

    void InitGeometry();
    void DestroyGeometry();

    void InitTextures();
    void DestroyTextures();

    void InitPipelines();
    void DestroyPipelines();

    void InitClientSocket();
    void DestroyClientSocket();

    void UpdateUniforms();

    void ReceiveNetworkPackets();
    void SendNetworkPackets();
};

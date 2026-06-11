#pragma once
// #include "../game.h"
#include "../Application/Application.h"
#include "../Events/EventListener.h"
#include "../Graphics/gpu_structs.h"
#include <glm/glm.hpp>
#include <vector>
#include "../Graphics/GLTFModel.h"
#include "../Graphics/Renderable.h"
#include "ChunkRenderer.h"

// Sockets
#include <WinSock2.h>
#include <WS2tcpip.h>

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
private:
    float m_yaw = 90.0f;
    float m_pitch = 0.0f;
    glm::vec3 m_current_position { 0.0f, 0.0f, 5.0f };
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

    std::unique_ptr<ChunkRenderer> m_chunk_renderer;

    double m_network_send_timer = 0.0;
    const int NETWORK_INPUT_SEND_RATE = 60; // Hz

    SOCKET m_socket;
    addrinfo m_server_addrinfo;
    bool m_connected_server = false;
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

#include "SimpleGame.h"
#include <iostream>

void SimpleGame::Initialize(ClientContext &context) {
    
    m_camera = &context.renderer.GetCamera();
    m_camera->SetPosition(glm::vec3(0.0f, 0.0f, 3.0f));
    m_camera->SetViewDirection(glm::vec3(0.0f, 0.0f, -1.0f));
    m_camera->SetFOV(45.0f);
}

void SimpleGame::ShutDown(ClientContext &context) {
}

void SimpleGame::Update(double delta_time, ClientContext &context) {
    
}

void SimpleGame::Render(double delta_time, ClientContext &context) {
    context.renderer.DrawFrame();
}

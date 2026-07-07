#include "SimpleGame.h"
#include <iostream>

void SimpleGame::Initialize(ClientContext &context) {
}

void SimpleGame::ShutDown(ClientContext &context) {
}

void SimpleGame::Update(double delta_time, ClientContext &context) {
    
}

void SimpleGame::Render(double delta_time, ClientContext &context) {
    context.renderer.DrawFrame();
}

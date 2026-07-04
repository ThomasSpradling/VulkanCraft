#pragma once

#include "Platform/Window/InputHandler.h"
#include "Renderer/Renderer.h"
#include "Network/NetworkHost.h"

struct ClientContext {
    InputHandler &input_handler;
    Renderer &renderer;
    NetworkHost &client_host;
};

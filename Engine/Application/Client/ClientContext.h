#pragma once

class InputHandler;
class Renderer;
class NetworkHost;
class World;
class AssetManager;

struct ClientContext {
    InputHandler &input_handler;
    Renderer &renderer;
    NetworkHost &client_host;
    World &world;
    AssetManager &assets;
};

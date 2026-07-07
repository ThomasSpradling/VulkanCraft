#pragma once

#include <Engine/Client.h>

class SimpleGame : public IClientGame {
public:
    SimpleGame() = default;

    void Initialize(ClientContext &context) override;
    void ShutDown(ClientContext &context) override;
    void Update(double delta_time, ClientContext &context) override;
    void Render(double delta_time, ClientContext &context) override;
};

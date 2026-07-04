#pragma once

#include "Core/NonCopyable.h"
#include "Core/NonMovable.h"
#include "ClientContext.h"

class IClientGame : public NonCopyable, public NonMovable {
public:
    virtual ~IClientGame() = default;

    virtual void Initialize(ClientContext &context) = 0;
    virtual void ShutDown(ClientContext &context) = 0;
    virtual void Update(double delta_time, ClientContext &context) = 0;
    virtual void Render(double delta_time, ClientContext &context) = 0;
};

#pragma once

#include "Core/NonCopyable.h"
#include "Core/NonMovable.h"
#include "ServerContext.h"

class IServerGame : public NonCopyable, public NonMovable {
public:
    virtual ~IServerGame() = default;

    virtual void Initialize(ServerContext &context) = 0;
    virtual void ShutDown(ServerContext &context) = 0;
    virtual void Update(float delta_time, ServerContext &context) = 0;
};

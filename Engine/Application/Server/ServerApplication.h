#pragma once

#include "IServerGame.h"

class ServerApplication {
public:
    ServerApplication(const IServerGame &game);
    ~ServerApplication();

    void Run();
private:
};

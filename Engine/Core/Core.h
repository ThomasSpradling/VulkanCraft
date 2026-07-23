#pragma once

#include <glm/glm.hpp>
// #include <vulkan/vulkan_core.h>
// #include <volk.h>

[[maybe_unused]] constexpr uint32_t EngineProfilerColor_Wait       = 0xFF0000;
[[maybe_unused]] constexpr uint32_t EngineProfilerColor_Submit     = 0x0000FF;
[[maybe_unused]] constexpr uint32_t EngineProfilerColor_Draw       = 0xFF00FF;
[[maybe_unused]] constexpr uint32_t EngineProfilerColor_Transfer   = 0xFFFF00;
[[maybe_unused]] constexpr uint32_t EngineProfilerColor_Present    = 0x00FF00;
[[maybe_unused]] constexpr uint32_t EngineProfilerColor_Create     = 0xFF6600;
[[maybe_unused]] constexpr uint32_t EngineProfilerColor_Destroy    = 0xFFA500;
[[maybe_unused]] constexpr uint32_t EngineProfilerColor_Barrier    = 0xFFFFFF;

#if defined(ENGINE_ENABLE_PROFILING)
    #include "tracy/Tracy.hpp"

    #define ENGINE_PROFILER_FUNCTION() ZoneScoped
    #define ENGINE_PROFILER_FUNCTION_COLOR(color) ZoneScopedC(color)

    #define ENGINE_PROFILER_ZONE(name, color) { \
        ZoneScopedC(color);                     \
        ZoneName(name, strlen(name));

    #define ENGINE_PROFILER_ZONE_END() }

    #define ENGINE_PROFILER_THREAD(name) tracy::SetThreadName(name);
    #define ENGINE_PROFILER_FRAME(name) FrameMarkNamed(name);
#else
    #define ENGINE_PROFILER_FUNCTION()
    #define ENGINE_PROFILER_FUNCTION_COLOR(color)
    #define ENGINE_PROFILER_ZONE(name, color) {
    #define ENGINE_PROFILER_ZONE_END() }
    #define ENGINE_PROFILER_THREAD(name)
    #define ENGINE_PROFILER_FRAME(name)
#endif // ENGINE_ENABLE_PROFILING

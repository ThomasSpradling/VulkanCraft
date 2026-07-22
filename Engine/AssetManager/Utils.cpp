#include "Utils.h"
#include "Bitmap.h"
#include "Core/errors.h"
#include <vector>

// Face IDs are in order +X, -X, +Y, -Y, +Z, -Z
// Convention as usual is -Z being forward
glm::vec3 FaceCoordsToXYZ(glm::vec2 uv, int face_id) {
    // Convert to range [-1, 1]
    float u = 2.0f * uv.x - 1.0f;
    float v = 2.0f * uv.y - 1.0f;

    switch (face_id) {
        case 0: return glm::vec3( 1.0f,     v,    -u);     // +X
        case 1: return glm::vec3(-1.0f,     v,     u);     // -X
        case 2: return glm::vec3(    u,  1.0f,    -v);     // +Y
        case 3: return glm::vec3(    u, -1.0f,     v);     // -Y
        case 4: return glm::vec3(    u,     v,  1.0f);     // +Z
        case 5: return glm::vec3(   -u,     v, -1.0f);     // -Z
        default:
    }
    return glm::vec3(0.0f);
}

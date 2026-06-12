#pragma once

#include <vector>
#include <glm/glm.hpp>

static_assert(sizeof(float) == sizeof(uint32_t), "NetworkBuffer assumes 32-bit floats.");
static_assert(std::numeric_limits<float>::is_iec559);

class NetworkBuffer {
public:
    NetworkBuffer() {};
    NetworkBuffer(size_t size) { m_data.reserve(size); }

    size_t GetSize() { return m_data.size(); }
    char *GetData() { return reinterpret_cast<char *>(m_data.data()); }
    uint8_t *GetRawData() { return m_data.data(); }
    const char *GetData() const { return reinterpret_cast<const char *>(m_data.data()); }

    void Resize(size_t size);
    void Clear();

    uint8_t ReadByte();
    bool ReadBoolean();
    uint16_t ReadShort();
    uint32_t ReadInteger();
    float ReadFloat();
    glm::vec3 ReadVec3();


    void WriteByte(uint8_t value);
    void WriteBoolean(bool value);
    void WriteShort(uint16_t value);
    void WriteInteger(uint32_t value);
    void WriteFloat(float value);
    void WriteVec3(glm::vec3 value);

    void Write(uint8_t value) { WriteByte(value); }
    void Write(uint16_t value) { WriteShort(value); }
    void Write(uint32_t value) { WriteInteger(value); }
    void Write(float value) { WriteFloat(value); }
    void Write(glm::vec3 value) { WriteVec3(value); }
    void Write(bool value) { WriteBoolean(value); }

    void Insert(const NetworkBuffer &buffer);
private:
    std::vector<uint8_t> m_data;
    int m_offset = 0;
    int m_size = 0;
};

#pragma once

#include <array>
#include <vector>
#include <glm/glm.hpp>
#include "Core/errors.h"

class NetworkBuffer {
public:
    NetworkBuffer() = default;
    NetworkBuffer(uint32_t size) { m_data.resize(size); }

    uint32_t GetSize() const { return static_cast<uint32_t>(m_data.size()); }
    char *GetData() { return reinterpret_cast<char *>(m_data.data()); }
    uint8_t *GetRawData() { return m_data.data(); }
    const char *GetData() const { return reinterpret_cast<const char *>(m_data.data()); }

    bool IsAtEnd() { return m_offset == static_cast<uint32_t>(m_data.size()); }

    void Resize(uint32_t size);
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

    void Skip(uint16_t byte_count) { m_offset += byte_count; }
    uint16_t RemainingBytes() { return static_cast<uint16_t>(m_data.size()) - m_offset; }
    
    template <uint16_t MaxByteCount>
    std::array<uint8_t, MaxByteCount> ReadBytes(const uint16_t byte_count) {
        ENGINE_ASSERT(byte_count <= MaxByteCount, "Byte count out of bounds!");
        ENGINE_ASSERT(m_offset + byte_count <= m_data.size(), "Failed to read byte from this buffer!");
        std::array<uint8_t, MaxByteCount> result {};
        std::copy_n(
            m_data.begin() + m_offset,
            byte_count,
            result.begin()
        );
        
        m_offset += byte_count;
        return result;
    }
private:
    std::vector<uint8_t> m_data;
    uint16_t m_offset = 0;
};

#pragma once

#include <array>
#include <vector>
#include <glm/glm.hpp>
#include "../../errors.h"

static_assert(sizeof(float) == sizeof(uint32_t), "NetworkBuffer assumes 32-bit floats.");
static_assert(std::numeric_limits<float>::is_iec559);

class NetworkBuffer {
public:
    NetworkBuffer() {};
    NetworkBuffer(size_t size) { m_data.resize(size); }

    size_t GetSize() const { return m_data.size(); }
    char *GetData() { return reinterpret_cast<char *>(m_data.data()); }
    uint8_t *GetRawData() { return m_data.data(); }
    const char *GetData() const { return reinterpret_cast<const char *>(m_data.data()); }

    bool IsAtEnd() { return m_offset == m_data.size(); }

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

    void Skip(uint32_t byte_count) { m_offset += byte_count; }
    uint16_t RemainingBytes() { return m_data.size() - m_offset; }
    
    template <size_t MaxByteCount>
    std::array<uint8_t, MaxByteCount> ReadBytes(const size_t byte_count) {
        Assert(byte_count <= MaxByteCount, "Byte count out of bounds!");
        Assert(m_offset + byte_count <= m_data.size(), "Failed to read byte from this buffer!");
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
    int m_offset = 0;
};

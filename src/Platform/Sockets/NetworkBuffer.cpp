#include "NetworkBuffer.h"
#include <array>
#include <bit>
#include <cstdint>

#include "../../errors.h"

void NetworkBuffer::Resize(size_t size) {
    m_offset = 0;
    m_data.resize(size);
}

void NetworkBuffer::Clear() {
    m_offset = 0;
    m_data.clear();
    m_data.resize(0);
}

uint8_t NetworkBuffer::ReadByte() {
    Assert(m_offset + 1 <= m_data.size(), "Failed to read byte from this buffer!");
    return static_cast<uint8_t>(m_data[m_offset++]);
}

bool NetworkBuffer::ReadBoolean() {
    return static_cast<uint8_t>(ReadByte());
}

uint16_t NetworkBuffer::ReadShort() {
    Assert(m_offset + 2 <= m_data.size(), "Failed to read short from this buffer!");

    uint16_t byte0 = m_data[m_offset];
    uint16_t byte1 = m_data[m_offset + 1];

    m_offset += 2;
    return (byte0 << 8) | byte1;
}

uint32_t NetworkBuffer::ReadInteger() {
    Assert(m_offset + 4 <= m_data.size(), "Failed to read integer from this buffer!");

    uint32_t byte0 = m_data[m_offset];
    uint32_t byte1 = m_data[m_offset + 1];
    uint32_t byte2 = m_data[m_offset + 2];
    uint32_t byte3 = m_data[m_offset + 3];

    m_offset += 4;
    return (byte0 << 24) | (byte1 << 16) | (byte2 << 8) | byte3;
}

float NetworkBuffer::ReadFloat() {
    Assert(m_offset + 4 <= m_data.size(), "Failed to read float from this buffer!");

    return std::bit_cast<float>(ReadInteger());
}

glm::vec3 NetworkBuffer::ReadVec3() {
    Assert(m_offset + 12 <= m_data.size(), "Failed to read Vec3 from this buffer!");

    float x = ReadFloat();
    float y = ReadFloat();
    float z = ReadFloat();
    return glm::vec3(x, y, z);
}

void NetworkBuffer::WriteByte(uint8_t value) {
    m_data.push_back(value);
}

void NetworkBuffer::WriteBoolean(bool value) {
    WriteByte(static_cast<uint8_t>(value));
}

void NetworkBuffer::WriteShort(uint16_t value) {
    WriteByte((value >> 8) & 0xFF);
    WriteByte(value & 0xFF);
}

void NetworkBuffer::WriteInteger(uint32_t value) {
    WriteByte((value >> 24) & 0xFF);
    WriteByte((value >> 16) & 0xFF);
    WriteByte((value >> 8) & 0xFF);
    WriteByte(value & 0xFF);
}

void NetworkBuffer::WriteFloat(float value) {
    WriteInteger(std::bit_cast<uint32_t>(value));
}

void NetworkBuffer::WriteVec3(glm::vec3 value) {
    WriteFloat(value.x);
    WriteFloat(value.y);
    WriteFloat(value.z);
}

void NetworkBuffer::Insert(const NetworkBuffer &buffer) {
    m_data.insert(m_data.end(), buffer.m_data.begin(), buffer.m_data.end());
}

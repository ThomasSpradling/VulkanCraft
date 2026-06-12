#include "NetworkBuffer.h"
#include <bit>
#include <cstdint>

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
    return static_cast<uint32_t>(m_data[m_offset++]);
}

bool NetworkBuffer::ReadBoolean() {
    return static_cast<uint8_t>(ReadByte());
}

uint16_t NetworkBuffer::ReadShort() {
    uint16_t byte0 = m_data[m_offset];
    uint16_t byte1 = m_data[m_offset + 1];

    m_offset += 2;
    return (byte0 << 8) | byte1;
}

uint32_t NetworkBuffer::ReadInteger() {
    uint32_t byte0 = m_data[m_offset];
    uint32_t byte1 = m_data[m_offset + 1];
    uint32_t byte2 = m_data[m_offset + 2];
    uint32_t byte3 = m_data[m_offset + 3];

    m_offset += 4;
    return (byte0 << 24) | (byte1 << 16) | (byte2 << 8) | byte3;
}

float NetworkBuffer::ReadFloat() {
    return std::bit_cast<float>(ReadInteger());
}

glm::vec3 NetworkBuffer::ReadVec3() {
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

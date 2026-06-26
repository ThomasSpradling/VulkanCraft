-- This file deserializes uncompressed packets from the VkCraft protocol, for use by Wireshark

local UDP_PORT = 8888
local SERIALIZATION_CHECK = 0xBEDEAD
local MAX_FRAGMENT_SIZE = 1024
local MAX_SLICE_SIZE = 1024

vkcraft = Proto("VkCraft", "VkCraft Protocol")

local f = vkcraft.fields;

-- Packets
local PACKET_TYPES = {
    [0] = "INVALID",
    [1] = "FRAGMENT",
    [2] = "SLICE",
    [3] = "SLICE_ACK",

    [4] = "CONNECT_REQUEST",
    [5] = "DISCONNECT_REQUEST",
    [6] = "CONNECT_RESPONSE",
    [7] = "DISCONNECT_RESPONSE",

    [8] = "TEST",
    [9] = "LARGE_TEST",
}

function get_packet_name(type)
    return PACKET_TYPES[type]
end

--------------
--  Header  --
--------------
f.checksum = ProtoField.uint32("vkcraft.checksum", "Checksum", base.HEX)
f.channel = ProtoField.uint8("vkcraft.channel", "Channel", base.DEC)
f.sequence = ProtoField.uint16("vkcraft.sequence", "Sequence", base.DEC)

--------------
--  Footer  --
--------------
f.serialization_check = ProtoField.uint32("vkcraft.serialize", "Serialization Check", base.HEX)

-------------------
--  Packet Data  --
-------------------
f.packet_type = ProtoField.uint8("vkcraft.packet_type", "Packet Type", base.DEC, PACKET_TYPES)

-- Connection Response
f.connection_accepted = ProtoField.uint8("vkcraft.connect_accept", "Connection Accepted", base.DEC)

-- Fragment
f.fragment_id = ProtoField.uint8("vkcraft.fragment_id", "Fragment Index", base.DEC)
f.num_fragments = ProtoField.uint8("vkcraft.num_fragments", "Fragment Count", base.DEC)
f.fragment_bytes = ProtoField.uint16("vkcraft.fragment_bytes", "Fragment Bytes", base.DEC)

-- Slice
f.chunk_id = ProtoField.uint16("vkcraft.chunk_id", "Chunk Id", base.DEC)
f.slice_id = ProtoField.uint8("vkcraft.slice_id", "Slice Index", base.DEC)
f.num_slices = ProtoField.uint8("vkcraft.num_slices", "Slice Count", base.DEC)
f.slice_bytes = ProtoField.uint16("vkcraft.slice_bytes", "Slice Bytes", base.DEC)

-- Slice Ack
f.ack_chunk_id = ProtoField.uint16("vkcraft.ack.chunk_id", "Chunk Id", base.DEC)
f.ack_num_slices = ProtoField.uint8("vkcraft.ack.num_slices", "Slice Count", base.DEC)

-- Test
f.test_value = ProtoField.uint32("vkcraft.test_value", "Value", base.DEC)


function vkcraft.dissector(buffer, pinfo, tree_root)
    length = buffer:len()
    if length == 0 then return end

    local offset = 0

    pinfo.cols.protocol = vkcraft.name
    local subtree = tree_root:add(vkcraft, buffer(), "VkCraft Protocol")

    -- BEGIN Utilities
    local function read_u8(tree, field)
        local range = buffer(offset, 1)
        offset = offset + 1
        tree:add(field, range)
        return range:uint(), range
    end

    local function read_u16(tree, field)
        local range = buffer(offset, 2)
        offset = offset + 2
        tree:add(field, range)
        return range:uint(), range
    end

    local function read_u32(tree, field)
        local range = buffer(offset, 4)
        offset = offset + 4
        tree:add(field, range)
        return range:uint(), range
    end
    -- END Utilities

    -- Header
    read_u32(subtree, f.checksum)
    read_u8(subtree, f.channel)
    read_u16(subtree, f.sequence)

    -- Data
    local footer_start = length - 4
    local packet_data_size = footer_start - offset - 1
    local packet_type = read_u8(subtree, f.packet_type)

    if (packet_data_size > 0) then
        local data_tree = subtree:add(vkcraft, buffer(offset, packet_data_size), "Data")

        if (get_packet_name(packet_type) == "CONNECT_RESPONSE") then
            read_u8(data_tree, f.connection_accepted)
        elseif (get_packet_name(packet_type) == "TEST") then
            read_u32(data_tree, f.test_value)
        elseif (get_packet_name(packet_type) == "FRAGMENT") then
            local frag_id = read_u8(data_tree, f.fragment_id)
            local num_frag = read_u8(data_tree, f.num_fragments)
            if (frag_id == num_frag - 1) then
                read_u16(data_tree, f.fragment_bytes)
            else
                data_tree:add(f.fragment_bytes, MAX_FRAGMENT_SIZE, "Label")
            end

            read_u16(data_tree, f.fragment_bytes)
        elseif (get_packet_name(packet_type) == "SLICE") then
            read_u16(data_tree, f.chunk_id)
            local slice_id = read_u8(data_tree, f.slice_id)
            local num_slice = read_u8(data_tree, f.num_slices)
            if (slice_id == num_slice - 1) then
                read_u16(data_tree, f.fragment_bytes)
            else
                data_tree:add(f.fragment_bytes, MAX_SLICE_SIZE, "Label")
            end

            read_u16(data_tree, f.slice_bytes)
        elseif (get_packet_name(packet_type) == "SLICE_ACK") then
            read_u16(data_tree, f.chunk_id)
            read_u8(data_tree, f.num_slices)
        else
            -- oK
        end
    end

    read_u32(subtree, f.serialization_check)
end

local port = DissectorTable.get("udp.port")
port:add(UDP_PORT, vkcraft)

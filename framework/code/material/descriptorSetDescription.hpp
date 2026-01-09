//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <vector>
#include <string>
#include <algorithm>

/// Describes the layout of a descriptor set.
/// Somewhat api agnostic.
/// @ingroup Material
class DescriptorSetDescription
{
public:
    enum class DescriptorType {
        Unused,                 ///< Use to pad unused 'slot' in descriptor set
        UniformBuffer,
        StorageBuffer,
        ImageSampler,           ///< Texture and Sampler combined
        ImageSampled,           ///< Image/texture alone (no sampler)
        Sampler,                ///< Sampler alone (no image/texture)
        ImageStorage,
        InputAttachment,
        DrawIndirectBuffer,
        AccelerationStructure,  ///< Ray Tracing
        DescriptorTable         ///< Dx12 descriptor table (set) reference
    };
    class StageFlag
    {
    public:
        enum class t {
            None = 0,
            Vertex = 1,
            Fragment = 2,
            Geometry = 4,
            Compute = 8,
            RayGeneration = 16,
            RayClosestHit = 32,
            RayAnyHit = 64,
            RayMiss = 128,
            Task = 256,
            Mesh = 512
        };
        StageFlag(const t _type) : type(_type) {}
        StageFlag& operator=(const StageFlag& other) { type = other.type; return *this; }
        StageFlag operator|(const StageFlag& other) const { return static_cast<t>(static_cast<std::underlying_type_t <t>>(type) | static_cast<std::underlying_type_t <t>>(other.type)); }
        StageFlag operator&(const StageFlag& other) const { return static_cast<t>(static_cast<std::underlying_type_t <t>>(type) & static_cast<std::underlying_type_t <t>>(other.type)); }
        operator bool() const { return type != t::None; }
        constexpr operator t() const { return type; }
    private:
        t type;
    };
    struct DescriptorTypeAndCount {
        DescriptorTypeAndCount(DescriptorTypeAndCount&&) = default;
        DescriptorTypeAndCount& operator=(const DescriptorTypeAndCount&) = delete;
        DescriptorTypeAndCount(const DescriptorTypeAndCount&) = delete;
        DescriptorTypeAndCount& operator=(DescriptorTypeAndCount&&) = delete;

        DescriptorType type;
        StageFlag stages;
        std::vector<std::string> names;
        uint32_t count;
        bool readOnly;
        int descriptorIndex;

        DescriptorTypeAndCount(DescriptorType _type, StageFlag _stages, std::vector<std::string> _names, int _count /*= -1*/ /* if un-set (-1), default to the number of names) */, bool _readOnly /*= false*/, int _descriptorIndex /*= -1*/)
            : type(_type)
            , stages(_stages)
            , names(_names)
            , count(_count)
            , readOnly(_readOnly)
            , descriptorIndex(_descriptorIndex)
        {
        }
    };
    std::string                         m_name;
    uint32_t                            m_setIndex = 0;         ///< numbered from 0 (0 is the root descriptor on DX12)
    std::vector<DescriptorTypeAndCount> m_descriptorTypes;
};

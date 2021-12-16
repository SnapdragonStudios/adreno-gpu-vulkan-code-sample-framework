// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <vector>
#include <string>
#include <cassert>


/// Describes the (platform agnostic) format and layout of a single vertex.
/// Can be constructed at compile time (eg to describe a known hard-coded layout vertex/structure) or populated from data (eg ShaderDescriptionLoader)
/// @ingroup Material
class VertexFormat
{
public:
    struct Element
    {
        class ElementType
        {
        public:
            enum class t {
                Int32,
                Float,
                Vec2,
                Vec3,
                Vec4
            };
            constexpr ElementType(const t _type) : type(_type) {}
            constexpr operator t() const { return type; }
            constexpr int size() const {
                switch(type) {
                    default:
                    case t::Int32:
                        return 4;
                    case t::Float:
                        return 4;
                    case t::Vec2:
                        return 8;
                    case t::Vec3:
                        return 12;
                    case t::Vec4:
                        return 16;
                }
            }
        private:
            const t type;
        };
        uint32_t offset;
        ElementType type;
        bool operator==(const Element& other) const { return offset==other.offset && type==other.type; };
    };
    enum class eInputRate {
        Vertex, Instance
    };
    const uint32_t span;                        ///< span of this vertex in bytes
    const eInputRate inputRate;                 ///< input rate of this vertex (Vertex or Instance rate)
    const std::vector<Element> elements;
    const std::vector<std::string> elementIds;

    const uint32_t& GetUint32(uint8_t* src, uint32_t elementIdx) const
    {
        if (elements[elementIdx].type == Element::ElementType::t::Int32 )
        {
            return * ((uint32_t*) (src+elements[elementIdx].offset));
        }
        assert(false);
        return *((uint32_t*)src);
    }
};

typedef VertexFormat::Element::ElementType::t VertexElementType;

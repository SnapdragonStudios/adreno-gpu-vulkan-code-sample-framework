//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <vector>
#include <string>
#include <cassert>
//#include <bit>
#include <array>
#include <span>
#include "system/glm_common.hpp"

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
                Boolean,
                Vec2,
                Vec3,
                Vec4,
                Int16,
                Float16,
                F16Vec2,
                F16Vec3,
                F16Vec4,
                Null
            };
            constexpr ElementType( const ElementType& ) noexcept = default;
            constexpr ElementType(const t _type) noexcept : type(_type) {}
            constexpr explicit operator t() const { return type; }
            constexpr bool operator==( const ElementType& other ) const = default;
            constexpr bool operator==( const ElementType::t other ) const { return *this == ElementType( other ); }
            constexpr auto get() const { return VertexFormat::Element::ElementType::t::Float; }
            constexpr uint32_t elements() const noexcept {
                switch (type) {
                case t::Int32:
                case t::Float:
                case t::Boolean:
                case t::Int16:
                case t::Float16:
                    return 1;
                case t::Vec2:
                case t::F16Vec2:
                    return 2;
                case t::Vec3:
                case t::F16Vec3:
                    return 3;
                case t::Vec4:
                case t::F16Vec4:
                    return 4;
                default:
                case t::Null:
                    return 0;
                }
            }
            constexpr uint32_t size() const noexcept {
                switch(type) {
                    case t::Int32:
                        return 4;
                    case t::Float:
                        return 4;
                    case t::Boolean:
                        return 4;
                    case t::Vec2:
                        return 8;
                    case t::Vec3:
                        return 12;
                    case t::Vec4:
                        return 16;
                    case t::Int16:
                        return 2;
                    case t::Float16:
                        return 2;
                    case t::F16Vec2:
                        return 4;
                    case t::F16Vec3:
                        return 6;
                    case t::F16Vec4:
                        return 8;
                    default:
                    case t::Null:
                        return 0;
                }
            }
            constexpr uint32_t alignment() const noexcept {
                switch(type) {
                    case t::Int32:
                        return 4;
                    case t::Float:
                        return 4;
                    case t::Boolean:
                        return 4;
                    case t::Vec2:
                        return 8;
                    case t::Vec3:
                        return 16;
                    case t::Vec4:
                        return 16;
                    case t::Int16:
                        return 2;
                    case t::Float16:
                        return 2;
                    case t::F16Vec2:
                        return 4;
                    case t::F16Vec3:
                        return 8;
                    case t::F16Vec4:
                        return 8;
                    default:
                    case t::Null:
                        return 0;
                }
            }
//        private:
            const t type;
        };
        uint32_t offset;
        ElementType type;
        constexpr bool operator==(const Element& other) const { return offset==other.offset && type==other.type; };
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


//constexpr float half_to_float( const uint16_t x ) { // Brute force conversion (does not deal with nan, inf and denorm)
//    uint32_t f = ((x & 0x8000) << 16) | (((x & 0x7c00) + 0x1C000) << 13) | ((x & 0x03FF) << 13);
//    return std::bit_cast<float, uint32_t>(f);
//}
//constexpr uint16_t float_to_half( const float x ) {
//    uint32_t f = std::bit_cast<uint32_t, float>(x);
//    return (uint16_t) (((f >> 16) & 0x8000) | ((((f & 0x7f800000) - 0x38000000) >> 13) & 0x7c00) | ((f >> 13) & 0x03ff));
//}

class VertexElementData
{
public:
    constexpr explicit VertexElementData(const VertexFormat::Element::ElementType::t& _type, const float _data) : type(_type), data32{}
    {
        if (type.type == VertexFormat::Element::ElementType::t::Float)
        {
            data32f[0] = _data;
            return;
        }
        assert(false);
    }

    constexpr explicit VertexElementData( const VertexFormat::Element::ElementType::t& _type, const int _data ) noexcept : type( _type )
    {
        if (type.type == VertexFormat::Element::ElementType::t::Int32)
        {
            data32[0] = _data;
            return;
        }
        assert( false );
    }

    constexpr explicit VertexElementData( const VertexFormat::Element::ElementType::t& _type, const bool _data ) noexcept : type( _type )
    {
        if (type == VertexFormat::Element::ElementType::t::Boolean)
        {
            data32[0] = _data ? 1 : 0;
            return;
        }
        assert( false );
    }

    constexpr VertexElementData() noexcept : type( VertexFormat::Element::ElementType::t::Null )
    {}

    constexpr VertexElementData( const VertexElementData& ) noexcept = default;
    constexpr VertexElementData( VertexElementData&& ) noexcept = default;

    const VertexFormat::Element::ElementType type;
    union {
        std::array<uint32_t, 1> data32;
        std::array<float, 1> data32f;
        std::array<std::byte, 4> data8;
    };

    constexpr auto size() const { return type.size(); }

    template<typename T>
    constexpr T get() const = delete;

    template<>
    constexpr float get<float>() const
    {
        if (type.type == VertexFormat::Element::ElementType::t::Float)
            return data32f[0];
        assert( 0 );
        return {};
    }

    template<>
    constexpr int get<int>() const
    {
        if (type.type == VertexFormat::Element::ElementType::t::Int32)
            return data32[0];
        assert( 0 );
        return {};
    }

    template<>
    constexpr bool get<bool>() const
    {
        if (type.type == VertexFormat::Element::ElementType::t::Boolean)
            return data32[0] ? true : false;
        assert( 0 );
        return {};
    }

    constexpr std::span<const std::byte> getUnsafeData() const
    {
        return std::span<const std::byte>( data8.begin(), data8.begin() + size() );
    }
};

/*
template<typename T>
consteval T get( VertexElementData classy );

template<>
consteval float get<float>( VertexElementData classy )
{
//    if constexpr (true)
    {
        if (classy.type.type == VertexFormat::Element::ElementType::t::Float)
            return classy.f;
    }
}
*/

typedef VertexFormat::Element::ElementType::t VertexElementType;

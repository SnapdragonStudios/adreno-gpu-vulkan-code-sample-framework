//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <vector>
//#include "memory/memoryMapped.hpp"
#include "shader.hpp"

// Forward declarations
template<typename T_GFXAPI> struct ImageInfo;
template<typename T_GFXAPI> class MaterialPass;
template<typename T_GFXAPI, typename tApiBufferType> class MemoryAllocatedBuffer;


/// Base class for ImageInfo (different per graphics api)
/// @ingroup Material
struct ImageInfoBase
{
    ImageInfoBase() {}
    ImageInfoBase( const ImageInfoBase& ) = delete;
    ImageInfoBase& operator=( const ImageInfoBase& ) = delete;
    ImageInfoBase( ImageInfoBase&& ) = default;
    ImageInfoBase& operator=( ImageInfoBase&& ) = default;
    template<typename T_GFXAPI> using tApiDerived = ImageInfo<T_GFXAPI>; // make apiCast work!
};

/// @ingroup Material
template<typename T_GFXAPI>
struct ImageInfo : public ImageInfoBase
{
    ImageInfo() = delete;   // must be specialized
    ~ImageInfo() = delete;  // must be specialized
    ImageInfo( const ImageInfo<T_GFXAPI>& ) = delete;
    ImageInfo<T_GFXAPI>& operator=( const ImageInfo<T_GFXAPI>& ) = delete;
    static_assert(sizeof( ImageInfo<T_GFXAPI> ) != sizeof( ImageInfoBase ));  // static assert if not specialized
};

/// An instance of a ShaderPass material.
/// @ingroup Material
class MaterialPassBase
{
protected:
    MaterialPassBase( const ShaderPassBase& shaderPass ) noexcept 
        : mShaderPass( shaderPass) {}
    MaterialPassBase(MaterialPassBase && other) noexcept
        : mShaderPass( other.mShaderPass ) {}
    ~MaterialPassBase() {}
public:
    template<typename T_GFXAPI> using tApiDerived = MaterialPass<T_GFXAPI>; // make apiCast work!

    const ShaderPassBase&   mShaderPass;
};

/// Templated MaterialPass (by the graphics api)
/// Expected to be specialized by graphics api and for this non-specialized template not be intantiated/used
/// @ingroup Material
template<typename T_GFXAPI>
class MaterialPass : public MaterialPassBase
{
    MaterialPass() = delete;   // must be specialized
    ~MaterialPass() = delete;  // must be specialized
    MaterialPass( const MaterialPass<T_GFXAPI>& ) = delete;
    MaterialPass<T_GFXAPI>& operator=( const MaterialPass<T_GFXAPI>& ) = delete;
    static_assert(sizeof(MaterialPass<T_GFXAPI>) != sizeof(MaterialPassBase));  // static assert if not specialized
};


struct BufferAndOffsetVoid {
    void* _buffer;
    uint32_t _offset = 0;
    constexpr bool operator==( const BufferAndOffsetVoid& other ) const = default;
    void* buffer() const { return _buffer; }
    auto  offset() const { return _offset; }
};

/// Reference to VkBuffer or ID3D12Resource (with an offset).
/// Does not have ownership of the buffer (and so lifetime of the buffer should be longer than the referencing @VkBufferAndOffset)
template<class T_GFXAPI>
struct BufferAndOffset : public BufferAndOffsetVoid {
    using tApiBufferType = typename T_GFXAPI::BufferHandleType;
    constexpr bool operator==( const BufferAndOffset<T_GFXAPI>& other ) const = default;
    tApiBufferType buffer() const {
        return static_cast<tApiBufferType>(_buffer);
    }
};


struct PerFrameBufferBase
{
    /// Reference to VkBuffer or ID3D12Resource (with an offset).
    /// Does not have ownership of the buffer (and so lifetime of the buffer should be longer than the referencing @VkBufferAndOffset)

    PerFrameBufferBase() = default;

    template< typename InputIt> requires (sizeof(std::iterator_traits<InputIt>::value_type)==sizeof(void*))
    PerFrameBufferBase( InputIt first, InputIt last ) noexcept
    {
        while (first != last)
            buffers.push_back( {*first++, 0} );
    }
    template<typename InputContainer, typename ValueType = std::decay_t<decltype(*std::begin( std::declval<InputContainer>() ))>>
    PerFrameBufferBase( const InputContainer& container )
    {
        for (auto item : container)
            buffers.push_back( {item} );
    }

    using tBuffers = std::vector<BufferAndOffsetVoid>;
    tBuffers buffers;
};

/// Reference to an array of BufferAndOffset (VkBuffer or ID3D12Resource, with an offset).
/// Does not have ownership of the buffers (and so lifetime of the buffers should be longer than this container)
template<typename T_GFXAPI>
struct PerFrameBuffer : public PerFrameBufferBase
{
    using tApiBufferType = typename T_GFXAPI::BufferHandleType;
    using tBuffers = std::vector<BufferAndOffset<T_GFXAPI>>;

    const auto& getBuffers() const { return (const tBuffers&)buffers; }
    auto& getBuffers() { return (tBuffers&)buffers; }

    template< typename InputIt> requires ( std::is_same_v<typename std::iterator_traits<InputIt>::value_type, tApiBufferType > )
    PerFrameBuffer( InputIt first, InputIt last ) : PerFrameBufferBase()
    {
        while (first != last)
            getBuffers().push_back({*first++, 0});
    }
    template< typename InputIt > requires (std::is_same_v<typename tBuffers::iterator, InputIt>)
    PerFrameBuffer( InputIt first, InputIt last ) : PerFrameBufferBase( first, last )
    {
    }

    template<typename InputContainer, typename ValueType = std::decay_t<decltype(*std::begin( std::declval<InputContainer>() ))>> requires (std::is_same_v<ValueType, MemoryAllocatedBuffer<T_GFXAPI, tApiBufferType>>)
    PerFrameBuffer( const InputContainer& container ) : PerFrameBufferBase()
    {
        for (auto& item : container)
            buffers.push_back( {item.GetVkBuffer()});
    }

    template<typename InputContainer, typename ValueType = std::decay_t<decltype(*std::begin( std::declval<InputContainer>() ))>>
    PerFrameBuffer( const InputContainer& container ) : PerFrameBufferBase()
    {
        for (auto item : container)
            buffers.push_back( { item } );
    }
    PerFrameBuffer( const BufferAndOffset<T_GFXAPI>& bufferAndOffset ) noexcept : PerFrameBufferBase()
    {
        buffers.push_back( bufferAndOffset );
    }
    PerFrameBuffer( tApiBufferType buffer ) noexcept : PerFrameBufferBase()
    {
        buffers.push_back( { buffer, 0 } );
    }
    explicit PerFrameBuffer( PerFrameBuffer<T_GFXAPI>&& other ) noexcept = default;
    PerFrameBuffer& operator=( PerFrameBuffer<T_GFXAPI>&& other ) noexcept = default;
    PerFrameBuffer( const PerFrameBuffer<T_GFXAPI>& other ) noexcept = default;
    PerFrameBuffer& operator=( const PerFrameBuffer<T_GFXAPI>& other ) noexcept = default;
    PerFrameBuffer() = default;

    constexpr size_t size() const noexcept { return buffers.size(); }
    constexpr typename tBuffers::iterator begin() noexcept { getBuffers().begin(); }
    constexpr typename tBuffers::const_iterator begin() const noexcept { return getBuffers().begin(); }
    constexpr typename tBuffers::const_iterator cbegin() const noexcept { return getBuffers().begin(); }
    constexpr typename tBuffers::iterator end() { return buffers.end(); }
    constexpr typename tBuffers::const_iterator end() const { return getBuffers().end(); }
    constexpr typename tBuffers::const_iterator cend() const { return getBuffers().cend(); }
    constexpr typename tBuffers::value_type& operator[]( const typename tBuffers::size_type x ) noexcept { return buffers[x]; }
    constexpr const typename tBuffers::value_type& operator[]( const typename tBuffers::size_type x ) const noexcept { return(typename tBuffers::value_type&)(buffers[x]); }
};

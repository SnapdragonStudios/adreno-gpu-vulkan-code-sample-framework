//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include "vertexFormat.hpp"
#include "descriptorSetDescription.hpp"
#include "specializationConstantDescription.hpp"
#include <string>
#include <map>
#include <vector>
#include <optional>
#include <array>

// Forward declarations
class AssetManager;

/// Describes a 'ShaderPass'.
/// Is platform agnostic.
/// @ingroup Material
class ShaderPassDescription
{
    ShaderPassDescription(const ShaderPassDescription&) = delete;
    ShaderPassDescription& operator=(const ShaderPassDescription&) = delete;
    ShaderPassDescription& operator=(ShaderPassDescription&&) = delete;
public:
    enum class DepthCompareOp {
        LessEqual,
        Equal,
        Greater
    };
    enum class BlendFactor {
        Zero,
        One,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha
    };
    struct FixedFunctionSettings
    {
        FixedFunctionSettings() = default;
        FixedFunctionSettings(FixedFunctionSettings&&) = default;
        FixedFunctionSettings& operator=(const FixedFunctionSettings&) = delete;
        FixedFunctionSettings(const FixedFunctionSettings&) = delete;
        FixedFunctionSettings& operator=(FixedFunctionSettings&&) = delete;

        // Depth Test/Write
        bool                depthTestEnable = false;
        bool                depthWriteEnable = false;
        DepthCompareOp      depthCompareOp = DepthCompareOp::LessEqual;
        bool                depthClampEnable = false;
        // Depth Bias
        bool                depthBiasEnable = false;
        float               depthBiasConstant = 0.0f;
        float               depthBiasClamp = 0.0f;
        float               depthBiasSlope = 0.0f;
        // Culling
        bool                cullFrontFace = false;
        bool                cullBackFace = false;
    };
    struct SampleShadingSettings
    {
        SampleShadingSettings() = default;
        SampleShadingSettings(SampleShadingSettings&&) = default;
        SampleShadingSettings& operator=(const SampleShadingSettings&) = delete;
        SampleShadingSettings(const SampleShadingSettings&) = delete;
        SampleShadingSettings& operator=(SampleShadingSettings&&) = delete;

        bool sampleShadingEnable = false;
        bool forceCenterSample = false; ///< Set to force all multisample samples to be from the center of the ms area (all mssa samples at same subpixel location)
        uint32_t sampleShadingMask = 0;
    };
    struct Output {
        bool                blendEnable = false;
        BlendFactor         srcColorBlendFactor = BlendFactor::SrcAlpha;
        BlendFactor         dstColorBlendFactor = BlendFactor::OneMinusSrcAlpha;
        BlendFactor         srcAlphaBlendFactor = BlendFactor::One;
        BlendFactor         dstAlphaBlendFactor = BlendFactor::Zero;
        uint32_t            colorWriteMask = 0x7fffffff;
        //VkBlendOp                colorBlendOp;
        //VkBlendOp                alphaBlendOp;
    };
    struct WorkGroupSettings
    {
        std::array<uint32_t, 3> localSize = {};
    };
    struct RayTracingSettings
    {
        uint32_t maxRayRecursionDepth = 1;
    };

    ShaderPassDescription( std::vector<DescriptorSetDescription> sets, std::vector<Output> outputs, std::string computeName, std::string vertexName, std::string fragmentName, std::string rayGenerationName, std::string rayClosestHitName, std::string rayAnyHitName, std::string rayMissName, FixedFunctionSettings fixedFunctionSettings, SampleShadingSettings sampleShadingSettings, WorkGroupSettings workGroupSettings, RayTracingSettings rayTracingSettings, std::vector<uint32_t> vertexFormatBindings, std::vector<SpecializationConstantDescription> specializationConstants);
    ShaderPassDescription( std::vector<DescriptorSetDescription> sets, std::vector<Output> outputs, std::string taskName, std::string meshName, std::string computeName, std::string vertexName, std::string fragmentName, std::string rayGenerationName, std::string rayClosestHitName, std::string rayAnyHitName, std::string rayMissName, FixedFunctionSettings fixedFunctionSettings, SampleShadingSettings sampleShadingSettings, WorkGroupSettings workGroupSettings, RayTracingSettings rayTracingSettings, std::vector<uint32_t> vertexFormatBindings, std::vector<SpecializationConstantDescription> specializationConstants);

    ShaderPassDescription(ShaderPassDescription&&) = default;

    std::vector<DescriptorSetDescription> m_sets;
    std::vector<Output> m_outputs;
    std::string m_taskName;                         ///< Name of the task shader (optional, not valid if m_vertexName or m_fragmentName are set)
    std::string m_meshName;                         ///< Name of the mesh shader (optional, not valid if m_vertexName or m_fragmentName are set), mandatory if task shader is set
    std::string m_computeName;                      ///< Name of the compute shader (optional, not valid if m_vertexName or m_fragmentName are set)
    std::string m_vertexName;                       ///< Name of the vertex shader used by this shader pass (optional, not valid if m_computeName set)
    std::string m_fragmentName;                     ///< Name of the fragment shader used by this shader pass (optional, only valid if m_vertexName set)
    std::string m_rayGenerationName;
    std::string m_rayClosestHitName;
    std::string m_rayAnyHitName;
    std::string m_rayMissName;
    FixedFunctionSettings m_fixedFunctionSettings;
    SampleShadingSettings m_sampleShadingSettings;
    WorkGroupSettings     m_workGroupSettings;
    RayTracingSettings    m_rayTracingSettings;
    std::vector<uint32_t> m_vertexFormatBindings;   //< Indices of the vertex buffers bound by this shader pass (index in to ShaderDescription::m_vertexFormats)
    std::vector<SpecializationConstantDescription> m_constants;
    //std::vector<uint32_t> m_vertexInstanceFormatBindings;  ///TODO: support more than one buffer of instance rate data
};

/// Describes a 'Shader'.
/// May contain multiple shader passes (vector of ShaderPassDescription and a pass name lookup).
/// Also contain description (VertexFormat) of the all the vertex buffers that can be bound to this set of shaders.
/// Is platform agnostic.
/// @ingroup Material
class ShaderDescription
{
    ShaderDescription(const ShaderDescription&) = delete;
    ShaderDescription& operator=(const ShaderDescription) = delete;
public:
    ShaderDescription(ShaderDescription&& other) noexcept
        : m_vertexFormats(std::move(other.m_vertexFormats))
        , m_descriptionPerPass(std::move(other.m_descriptionPerPass))
        , m_passNameToIndex(std::move(other.m_passNameToIndex))
    {}
    ShaderDescription(std::vector<VertexFormat> vertexFormats, std::vector<ShaderPassDescription> passes, const std::vector<std::string>& passNames)
        : m_vertexFormats(std::move(vertexFormats))
        , m_descriptionPerPass(std::move(passes))
    {
        uint32_t passIdx = 0;
        for (const auto& passname : passNames)
        {
            m_passNameToIndex.try_emplace(passname, passIdx);
            ++passIdx;
        }
    }
    std::optional<std::reference_wrapper<const ShaderPassDescription>> GetPassDescription(const std::string& pass)
    {
        const auto it = m_passNameToIndex.find(pass);
        if (it != m_passNameToIndex.end())
            return m_descriptionPerPass[it->second];
        return std::nullopt;
    }
    std::vector<VertexFormat> m_vertexFormats;
    std::vector<ShaderPassDescription> m_descriptionPerPass;
    std::map <std::string, uint32_t> m_passNameToIndex; // index in to m_descriptionPerPass vector
};


/// Helper class for loading ShaderDescription
/// @ingroup Material
class ShaderDescriptionLoader
{
public:
    /// @brief Loads json description of shader (potentially multiple pases)
    /// @param filename json filename (with extension)
    /// @return ShaderDescription object if json load and parse was successful
    static std::optional<ShaderDescription> Load(AssetManager& assetManager, const std::string& filename);
};

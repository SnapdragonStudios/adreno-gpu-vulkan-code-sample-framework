//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "shaderDescription.hpp"
#include "nlohmann/json.hpp"
#include "system/assetManager.hpp"
#include "system/os_common.h"
#include "texture/sampler.hpp"
#include <optional>
using namespace std::string_literals;

using Json = nlohmann::json;

ShaderPassDescription::ShaderPassDescription(std::vector<DescriptorSetDescription> setDescriptions, std::vector<Output> outputs, std::string computeName, std::string vertexName, std::string fragmentName, std::string rayGenerationName, std::string rayClosestHitName, std::string rayAnyHitName, std::string rayMissName, ShaderPassDescription::FixedFunctionSettings fixedFunctionSettings, ShaderPassDescription::SampleShadingSettings sampleShadingSettings, ShaderPassDescription::WorkGroupSettings workGroupSettings, ShaderPassDescription::RayTracingSettings rayTracingSettings, std::vector<uint32_t> vertexFormatBindings, std::vector<SpecializationConstantDescription> specializationConstants, std::vector<CreateSamplerObjectInfo> rootSamplers )
    : m_sets(std::move(setDescriptions))
    , m_outputs(std::move(outputs))
    , m_computeName(std::move(computeName))
    , m_vertexName(std::move(vertexName))
    , m_fragmentName(std::move(fragmentName))
    , m_rayGenerationName(std::move(rayGenerationName))
    , m_rayClosestHitName(std::move(rayClosestHitName))
    , m_rayAnyHitName(std::move(rayAnyHitName))
    , m_rayMissName(std::move(rayMissName))
    , m_fixedFunctionSettings(std::move(fixedFunctionSettings))
    , m_sampleShadingSettings(std::move(sampleShadingSettings))
    , m_workGroupSettings(std::move(workGroupSettings))
    , m_rayTracingSettings(std::move(rayTracingSettings))
    , m_vertexFormatBindings(std::move(vertexFormatBindings))
    , m_constants(std::move(specializationConstants))
    , m_rootSamplers(std::move(rootSamplers))
//    , m_vertexInstanceFormatBindings(std::move(vertexInstanceFormatBindings))
{
}

ShaderPassDescription::ShaderPassDescription(std::vector<DescriptorSetDescription> setDescriptions, std::vector<Output> outputs, std::string taskName, std::string meshName, std::string computeName, std::string vertexName, std::string fragmentName, std::string rayGenerationName, std::string rayClosestHitName, std::string rayAnyHitName, std::string rayMissName, ShaderPassDescription::FixedFunctionSettings fixedFunctionSettings, ShaderPassDescription::SampleShadingSettings sampleShadingSettings, ShaderPassDescription::WorkGroupSettings workGroupSettings, ShaderPassDescription::RayTracingSettings rayTracingSettings, std::vector<uint32_t> vertexFormatBindings, std::vector<SpecializationConstantDescription> specializationConstants, std::vector<CreateSamplerObjectInfo> rootSamplers )
    : m_sets(std::move(setDescriptions))
    , m_outputs(std::move(outputs))
    , m_taskName(std::move(taskName))
    , m_meshName(std::move(meshName))
    , m_computeName(std::move(computeName))
    , m_vertexName(std::move(vertexName))
    , m_fragmentName(std::move(fragmentName))
    , m_rayGenerationName(std::move(rayGenerationName))
    , m_rayClosestHitName(std::move(rayClosestHitName))
    , m_rayAnyHitName(std::move(rayAnyHitName))
    , m_rayMissName(std::move(rayMissName))
    , m_fixedFunctionSettings(std::move(fixedFunctionSettings))
    , m_sampleShadingSettings(std::move(sampleShadingSettings))
    , m_workGroupSettings(std::move(workGroupSettings))
    , m_rayTracingSettings(std::move(rayTracingSettings))
    , m_vertexFormatBindings(std::move(vertexFormatBindings))
    , m_constants(std::move(specializationConstants))
    , m_rootSamplers( std::move( rootSamplers ) )
{
}

const static std::map<std::string, VertexFormat::Element::ElementType> cElementTypeByName{
    {"Int32"s,                  VertexFormat::Element::ElementType::t::Int32},
    {"UInt32"s,                 VertexFormat::Element::ElementType::t::UInt32},
    {"Float"s,                  VertexFormat::Element::ElementType::t::Float},
    {"Boolean"s,                VertexFormat::Element::ElementType::t::Boolean},
    {"Vec2"s,                   VertexFormat::Element::ElementType::t::Vec2},
    {"Vec3"s,                   VertexFormat::Element::ElementType::t::Vec3},
    {"Vec4"s,                   VertexFormat::Element::ElementType::t::Vec4},
    {"IVec2"s,                  VertexFormat::Element::ElementType::t::IVec2},
    {"IVec3"s,                  VertexFormat::Element::ElementType::t::IVec3},
    {"IVec4"s,                  VertexFormat::Element::ElementType::t::IVec4},
    {"UVec2"s,                  VertexFormat::Element::ElementType::t::UVec2},
    {"UVec3"s,                  VertexFormat::Element::ElementType::t::UVec3},
    {"UVec4"s,                  VertexFormat::Element::ElementType::t::UVec4},
    {"Int16"s,                  VertexFormat::Element::ElementType::t::Int16},
    {"UInt16"s,                 VertexFormat::Element::ElementType::t::UInt16},
    {"Float16"s,                VertexFormat::Element::ElementType::t::Float16},
    {"F16Vec2"s,                VertexFormat::Element::ElementType::t::F16Vec2},
    {"F16Vec3"s,                VertexFormat::Element::ElementType::t::F16Vec3},
    {"F16Vec4"s,                VertexFormat::Element::ElementType::t::F16Vec4},
    {"I16Vec2"s,                VertexFormat::Element::ElementType::t::I16Vec2},
    {"I16Vec3"s,                VertexFormat::Element::ElementType::t::I16Vec3},
    {"I16Vec4"s,                VertexFormat::Element::ElementType::t::I16Vec4},
    {"U16Vec2"s,                VertexFormat::Element::ElementType::t::U16Vec2},
    {"U16Vec3"s,                VertexFormat::Element::ElementType::t::U16Vec3},
    {"U16Vec4"s,                VertexFormat::Element::ElementType::t::U16Vec4},
};
const static std::map<std::string, VertexFormat::eInputRate> cBufferRateByName{
    {"Vertex"s,                 VertexFormat::eInputRate::Vertex},
    {"Instance"s,               VertexFormat::eInputRate::Instance}
};

struct DescriptorTypeAndReadOnly {
    DescriptorSetDescription::DescriptorType type;
    bool readOnly = false;
};
const static std::map<std::string, DescriptorTypeAndReadOnly> cBufferTypeByName {
    {"Unused"s,                 {DescriptorSetDescription::DescriptorType::Unused,                  true}},
    {"ImageSampler"s,           {DescriptorSetDescription::DescriptorType::ImageSampler,            true}},
    {"ImageSampled"s,           {DescriptorSetDescription::DescriptorType::ImageSampled,            true}},
    {"Sampler"s,                {DescriptorSetDescription::DescriptorType::Sampler,                 true}},
    {"UniformBuffer"s,          {DescriptorSetDescription::DescriptorType::UniformBuffer,           true}},
    {"StorageBuffer"s,          {DescriptorSetDescription::DescriptorType::StorageBuffer,           false}},
    {"ImageStorage"s,           {DescriptorSetDescription::DescriptorType::ImageStorage,            false}},
    {"InputAttachment"s,        {DescriptorSetDescription::DescriptorType::InputAttachment,         true}},
    {"AccelerationStructure"s,  {DescriptorSetDescription::DescriptorType::AccelerationStructure,   true}},
    {"DescriptorTable"s,        {DescriptorSetDescription::DescriptorType::DescriptorTable,         true}}
};

const static std::map<std::string, DescriptorSetDescription::StageFlag> cStageFlagBitsByName{
    {"Vertex"s,                 DescriptorSetDescription::StageFlag::t::Vertex},
    {"Fragment"s,               DescriptorSetDescription::StageFlag::t::Fragment},
    {"Geometry"s,               DescriptorSetDescription::StageFlag::t::Geometry},
    {"Compute"s,                DescriptorSetDescription::StageFlag::t::Compute},
    {"RayGeneration"s,          DescriptorSetDescription::StageFlag::t::RayGeneration},
    {"RayClosestHit"s,          DescriptorSetDescription::StageFlag::t::RayClosestHit},
    {"RayAnyHit"s,              DescriptorSetDescription::StageFlag::t::RayAnyHit},
    {"RayMiss"s,                DescriptorSetDescription::StageFlag::t::RayMiss},
    {"Task"s,                   DescriptorSetDescription::StageFlag::t::Task},
    {"Mesh"s,                   DescriptorSetDescription::StageFlag::t::Mesh}
};

const static std::map<std::string, ShaderPassDescription::DepthCompareOp> cDepthCompareOpByName{
    {"LessEqual"s,              ShaderPassDescription::DepthCompareOp::LessEqual},
    {"Equal"s,                  ShaderPassDescription::DepthCompareOp::Equal},
    {"Greater"s,                ShaderPassDescription::DepthCompareOp::Greater},
};

const static std::map<std::string, ShaderPassDescription::BlendFactor> cBlendFactorByName{
    {"Zero"s,                   ShaderPassDescription::BlendFactor::Zero},
    {"One"s,                    ShaderPassDescription::BlendFactor::One},
    {"SrcAlpha"s,               ShaderPassDescription::BlendFactor::SrcAlpha},
    {"OneMinusSrcAlpha"s,       ShaderPassDescription::BlendFactor::OneMinusSrcAlpha},
    {"DstAlpha"s,               ShaderPassDescription::BlendFactor::DstAlpha},
    {"OneMinusDstAlpha"s,       ShaderPassDescription::BlendFactor::OneMinusDstAlpha},
};

static void from_json(const Json& j, ShaderPassDescription::DepthCompareOp& op) {
    const auto foundIt = cDepthCompareOpByName.find(j);
    if (foundIt != cDepthCompareOpByName.end()) {
        op = foundIt->second;
    }
    else {
        throw std::invalid_argument("Unknown DepthCompareOp"s);
    }
}

static void from_json(const Json& j, ShaderPassDescription::BlendFactor& op) {
    const auto foundIt = cBlendFactorByName.find(j);
    if (foundIt != cBlendFactorByName.end()) {
        op = foundIt->second;
    }
    else {
        throw std::invalid_argument("Unknown BlendFactor"s);
    }
}

static void from_json(const Json& j, ShaderPassDescription::Output& output) {
    auto it = j.find("BlendEnable"s);
    if (it != j.end()) it->get_to(output.blendEnable);

    it = j.find("SrcColorBlendFactor"s);
    if (it != j.end()) it->get_to(output.srcColorBlendFactor);

    it = j.find("DstColorBlendFactor"s);
    if (it != j.end()) it->get_to(output.dstColorBlendFactor);

    it = j.find("SrcAlphaBlendFactor"s);
    if (it != j.end()) it->get_to(output.srcAlphaBlendFactor);

    it = j.find("DstAlphaBlendFactor"s);
    if (it != j.end()) it->get_to(output.dstAlphaBlendFactor);

    it = j.find("ColorWriteMask"s);
    if (it != j.end()) it->get_to(output.colorWriteMask);
}

static void from_json(const Json& j, ShaderPassDescription::FixedFunctionSettings& ffs) {
    auto it = j.find("DepthTestEnable"s);
    if (it != j.end()) it->get_to(ffs.depthTestEnable);

    it = j.find("DepthWriteEnable"s);
    if (it != j.end()) it->get_to(ffs.depthWriteEnable);

    it = j.find("DepthCompareOp"s);
    if (it != j.end()) it->get_to(ffs.depthCompareOp);

    it = j.find("DepthClampEnable"s);
    if (it != j.end()) it->get_to(ffs.depthClampEnable);

    it = j.find("DepthBiasEnable"s);
    if (it != j.end()) it->get_to(ffs.depthBiasEnable);

    it = j.find("DepthBiasConstant"s);
    if (it != j.end()) it->get_to(ffs.depthBiasConstant);

    it = j.find("DepthBiasClamp"s);
    if (it != j.end()) it->get_to(ffs.depthBiasClamp);

    it = j.find("DepthBiasSlope"s);
    if (it != j.end()) it->get_to(ffs.depthBiasSlope);

    it = j.find("CullBackFace"s);
    if (it != j.end()) it->get_to(ffs.cullBackFace);

    it = j.find("CullFrontFace"s);
    if (it != j.end()) it->get_to(ffs.cullFrontFace);
}

static void from_json(const Json& j, ShaderPassDescription::SampleShadingSettings& sss) {
    auto it = j.find("Enable"s);
    if (it != j.end()) it->get_to(sss.sampleShadingEnable);
    it = j.find("ForceCenterSample"s);
    if (it != j.end()) it->get_to(sss.forceCenterSample);
    it = j.find("Mask"s);
    if (it != j.end()) it->get_to(sss.sampleShadingMask);
}

static void from_json(const Json& j, ShaderPassDescription::WorkGroupSettings& ws) {
    auto it = j.find("LocalSize"s);
    if (it != j.end()) it->get_to(ws.localSize);
    it = j.find("PerTileDispatch"s);
    if (it != j.end()) it->get_to(ws.perTileDispatch );
}

static void from_json(const Json& j, ShaderPassDescription::RayTracingSettings& rts) {
    auto it = j.find("MaxRecursionDepth"s);
    if (it != j.end()) it->get_to(rts.maxRayRecursionDepth);
}

std::optional<ShaderDescription> ShaderDescriptionLoader::Load(AssetManager& assetManager, const std::string& filename)
{
    Json json;
    {
        std::vector<char> data;
        if (!assetManager.LoadFileIntoMemory(filename, data))
        {
            return std::nullopt;
        }
        json = Json::parse(data);
    }

    std::vector<VertexFormat> vertexFormats;
    std::map<std::string, uint32_t> vertexFormatLookup;   // lookup in to the vertexFormats vector (by vertex format name)

    if (json.contains("Vertex"s))
    {
        // Array of 'Vertex" objects
        for (const auto& el : json["Vertex"s])
        {
            std::vector<VertexFormat::Element> elements;
            std::vector<std::string> elementIds;

            VertexFormat::eInputRate rate = VertexFormat::eInputRate::Vertex;
            uint32_t span = el["Span"s];
            uint32_t offset = 0;
            for (auto& ar : el["Elements"s])
            {
                const std::string& elementType = ar["Type"s];
                const auto typeIt = cElementTypeByName.find(elementType);
                assert(typeIt != cElementTypeByName.end());
                const uint32_t elementOffset = ar.contains("Offset"s) ? (uint32_t)ar["Offset"s] : offset;
                elements.emplace_back(VertexFormat::Element{ elementOffset, typeIt->second });
                if (ar.contains("Name"s))
                {
                    elementIds.emplace_back(ar["Name"s]);
                }
                offset = elementOffset + typeIt->second.size();
                if (offset > span)
                {
                    throw std::runtime_error(filename + " : Element outside of span range"s);
                }
            }
            if (elementIds.size() > 0 && elementIds.size() != elements.size())
            {
                throw std::runtime_error(filename + " : all vertex elements must all be named or none named!"s);
            }
            if (el.contains("Name"s))
            {
                vertexFormatLookup.try_emplace(el["Name"s], (uint32_t)vertexFormats.size());   // put in the lookup before vertexFormats.emplace_back so correctly indexed (from 0)
            }
            if (el.contains("Rate"s))
            {
                const auto rateIt = cBufferRateByName.find(el["Rate"s]);
                assert(rateIt != cBufferRateByName.end());
                rate = rateIt->second;
            }
            vertexFormats.emplace_back(VertexFormat{ span, rate, std::move(elements), std::move(elementIds) });
        }
    }
    std::vector<std::string> passNames;
    std::vector<ShaderPassDescription> descriptions;
    auto jtileShading = json["TileShading"s];
    bool tileShading = false;
    if (!jtileShading.is_null())
    {
        jtileShading.get_to( tileShading );
    }
    for(const auto& ar: json["Passes"s] )
    {
        std::string taskShader;
        std::string meshShader;
        std::string computeShader;
        std::string vertexShader;
        std::string fragmentShader;
        std::string rayGenerationShader;
        std::string rayClosestHitShader;
        std::string rayAnyHitShader;
        std::string rayMissShader;
        std::vector<DescriptorSetDescription> sets;
        std::vector<ShaderPassDescription::Output> outputs;
        std::string passname;
        std::vector<uint32_t> vertexFormatBindings;             // index(s) in to the VertexFormat array for vertex rate data (ie per vert)
        std::vector<uint32_t> instanceVertexFormatBindings;     // index(s) in to the VertexFormat array for instance rate data (ie per instance)
        ShaderPassDescription::FixedFunctionSettings fixedFunctionSettings;
        ShaderPassDescription::SampleShadingSettings sampleShadingSettings;
        ShaderPassDescription::WorkGroupSettings workGroupSettings;
        ShaderPassDescription::RayTracingSettings rayTracingSettings;
        std::vector<SpecializationConstantDescription> specializationConstants;
        std::vector<CreateSamplerObjectInfo> rootSamplers;

        for (const auto& el : ar.items())
        {
            if (el.key().compare("Name"s ) == 0)
            {
                passname = el.value();
            }
            else if (el.key().compare("Shaders"s ) == 0)
            {
                // Either Compute or Vertex shader has to be defined, but Fragment is optional.
                // Or none of the above and Ray Generation has to be defined, but any hit, closest hit and miss are optional
                taskShader = el.value().contains( "Task"s) ? (std::string) el.value()["Task"s] : std::string();
                meshShader = el.value().contains( "Mesh"s) ? (std::string) el.value()["Mesh"s] : std::string();
                computeShader = el.value().contains( "Compute"s) ? (std::string) el.value()["Compute"s] : std::string();
                vertexShader = el.value().contains( "Vertex"s) ? (std::string) el.value()["Vertex"s] : std::string();
                fragmentShader = el.value().contains( "Fragment"s) ? (std::string) el.value()["Fragment"s] : std::string();
                rayGenerationShader = el.value().contains( "RayGeneration"s) ? (std::string) el.value()["RayGeneration"s] : std::string();
                rayClosestHitShader = el.value().contains( "RayClosestHit"s) ? (std::string) el.value()["RayClosestHit"s] : std::string();
                rayAnyHitShader = el.value().contains( "RayAnyHit"s) ? (std::string) el.value()["RayAnyHit"s] : std::string();
                rayMissShader = el.value().contains( "RayMiss"s) ? (std::string) el.value()["RayMiss"s] : std::string();

                if (meshShader.empty() && computeShader.empty() && fragmentShader.empty() && vertexShader.empty() && rayGenerationShader.empty())
                {
                    throw std::runtime_error(filename + " : must have a Vertex, Compute, Mesh or RayGeneration shader name!"s);
                }
                if (!taskShader.empty() && meshShader.empty())
                {
                    throw std::runtime_error(filename + " : must have a Mesh shader when a Task is provided !"s);
                }
                if (!fragmentShader.empty() && (meshShader.empty() && vertexShader.empty()))
                {
                    throw std::runtime_error(filename + " : must have a Vertex or Mesh shader when a Fragment is provided !"s);
                }
                const bool rayGen = !rayGenerationShader.empty();
                const bool rayProcess = !(rayClosestHitShader.empty() && rayAnyHitShader.empty() && rayMissShader.empty());
                if (rayGen && !rayProcess)
                {
                    throw std::runtime_error(filename + " : must have a Ray hit, miss or closest hit shader when a RayGeneration is provided!"s);
                }
                else if (!rayGen && rayProcess)
                {
                    throw std::runtime_error(filename + " : must have a RayGeneration shader when a Ray hit, miss or closest hit shader is provided!"s);
                }
            }
            else if (el.key().compare("DescriptorSets"s) == 0)
            {
                for (uint32_t setIndex = 0; const auto& ar : el.value())
                {
                    std::vector<DescriptorSetDescription::DescriptorTypeAndCount> descriptors;
                    if (ar.contains( "Buffers"s))
                    {
                        auto& buffel = ar["Buffers"s];
                        for (const auto& ar : buffel)
                        {
                            const std::string& bufferType = ar["Type"s];
                            uint32_t count = ar.contains( "Count"s) ? (uint32_t) ar["Count"s] : 1;
                            bool readOnly = ar.contains( "ReadOnly"s) ? (bool) ar["ReadOnly"s] : false; // may be overridden if the 'Type' is intrinsically read-only
                            DescriptorSetDescription::StageFlag stageBindingFlags = { DescriptorSetDescription::StageFlag::t::None };
                            for (const auto& ar2 : ar["Stages"s])
                            {
                                stageBindingFlags = stageBindingFlags | cStageFlagBitsByName.find( ar2 )->second;
                            }
                            std::vector<std::string> names;
                            if (ar.contains( "Names"s))
                            {
                                for (auto& ar2 : ar["Names"s])
                                {
                                    names.push_back( ar2 );
                                }
                            }
                            int registerIndex = -1; // default, ascending register indices
                            if (ar.contains( "Register"s))
                            {
                                registerIndex = (int) ar["Register"s];
                            }
                            assert( names.size() <= 1 );  ///TODO: array of names does not currently work (especially since dynmamic 'Count' was added) 
                            //count = std::max(count, (uint32_t)names.size());
                            const auto& bufferTypeData = cBufferTypeByName.find( bufferType )->second;
                            descriptors.emplace_back( DescriptorSetDescription::DescriptorTypeAndCount { bufferTypeData.type, stageBindingFlags, names, (int) count, readOnly || bufferTypeData.readOnly, registerIndex } );
                        }
                    }
                    std::string name;
                    if (ar.contains("Name"s))
                    {
                        name = ar["Name"s];
                    }
                    sets.push_back( { name, setIndex++, std::move( descriptors ) } );
                }
            }
            else if (el.key().compare("Outputs"s) == 0)
            {
                for (const auto& ar2 : el.value())
                {
                    outputs.push_back( ar2 );
                }
            }
            else if (el.key().compare("FixedFunction"s) == 0)
            {
                el.value().get_to( fixedFunctionSettings );// <ShaderPassDescription::FixedFunctionSettings>();
            }
            else if (el.key().compare("SampleShading"s) == 0)
            {
                el.value().get_to( sampleShadingSettings );// <ShaderPassDescription::SampleShadingSettings>();
            }
            else if (el.key().compare("WorkGroup"s) == 0)
            {
                el.value().get_to( workGroupSettings );// <ShaderPassDescription::WorkGroupSettings>();
            }
            else if (el.key().compare("RayTracing"s) == 0)
            {
                el.value().get_to( rayTracingSettings );// <ShaderPassDescription::RayTracingSettings>();
            }
            else if (el.key().compare("VertexBindings"s) == 0)
            {
                // Array of VertexBinding names
                for (const auto& vertexFormatName : el.value())
                {
                    auto it = vertexFormatLookup.find( vertexFormatName );
                    if (it != vertexFormatLookup.end())
                    {
                        vertexFormatBindings.push_back( it->second );
                    }
                    else
                    {
                        throw std::runtime_error(filename + " : VertexBinding does not match a Name in the Vertex array"s);
                    }
                }
            }
            else if (el.key().compare("SpecializationConstants"s) == 0)
            {
                for (const auto& ar : el.value())
                {
                    const std::string& constantName = ar["Name"s];
                    const std::string& constantType = ar["Type"s];
                    const auto typeIt = cElementTypeByName.find( constantType );
                    assert( typeIt != cElementTypeByName.end() );
                    assert( typeIt->second == VertexFormat::Element::ElementType::t::Int32 || typeIt->second == VertexFormat::Element::ElementType::t::Float || typeIt->second == VertexFormat::Element::ElementType::t::Boolean );

                    specializationConstants.push_back( SpecializationConstantDescription { constantName, (uint32_t) specializationConstants.size(), typeIt->second } );
                }   
            } 
            else if (el.key().compare("RootSamplers"s) == 0)
            {
                for (const auto& ar : el.value())
                {
                    CreateSamplerObjectInfo& samplerInfo = rootSamplers.emplace_back( CreateSamplerObjectInfo{} );
                    ar.get_to( samplerInfo );
                }
            }
        }

        descriptions.emplace_back(  std::move(sets),
                                    std::move(outputs), 
                                    std::move(taskShader),
                                    std::move(meshShader),
                                    std::move(computeShader),
                                    std::move(vertexShader),
                                    std::move(fragmentShader),
                                    std::move(rayGenerationShader),
                                    std::move(rayClosestHitShader),
                                    std::move(rayAnyHitShader),
                                    std::move(rayMissShader),
                                    std::move(fixedFunctionSettings),
                                    std::move(sampleShadingSettings),
                                    std::move(workGroupSettings),
                                    std::move(rayTracingSettings),
                                    std::move(vertexFormatBindings), 
                                    std::move(specializationConstants),
                                    std::move(rootSamplers)
        );
        passNames.push_back(passname);
    }

    // Run sanity checks on the shader description to catch common errors.
    bool sane = std::all_of( descriptions.begin(), descriptions.end(), [&filename]( const ShaderPassDescription& desc ) -> bool {
        if (!desc.m_vertexName.empty())
        {
            if (desc.m_outputs.empty())
            {
                //throw std::runtime_error(filename + " : must have outputs (in json) when vertex shader (rasterization) provided !"s);
                //return false;   // not hit
            }
        }
        return true;
    } );
    if (!sane)
    {
        return std::nullopt;
    }

    return std::make_optional(ShaderDescription{ std::move(vertexFormats), std::move(descriptions), passNames, tileShading });
}

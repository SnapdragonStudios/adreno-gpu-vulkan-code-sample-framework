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
#include <fstream>
#include <optional>
#include <iostream>

using Json = nlohmann::json;

ShaderPassDescription::ShaderPassDescription(std::vector<DescriptorSetDescription> setDescriptions, std::vector<Output> outputs, std::string computeName, std::string vertexName, std::string fragmentName, ShaderPassDescription::FixedFunctionSettings fixedFunctionSettings, ShaderPassDescription::SampleShadingSettings sampleShadingSettings, ShaderPassDescription::WorkGroupSettings workGroupSettings, std::vector<uint32_t> vertexFormatBindings)
    : m_sets(std::move(setDescriptions))
    , m_outputs(std::move(outputs))
    , m_computeName(std::move(computeName))
    , m_vertexName(std::move(vertexName))
    , m_fragmentName(std::move(fragmentName))
    , m_fixedFunctionSettings(std::move(fixedFunctionSettings))
    , m_sampleShadingSettings(std::move(sampleShadingSettings))
    , m_workGroupSettings(std::move(workGroupSettings))
    , m_vertexFormatBindings(std::move(vertexFormatBindings))
//    , m_vertexInstanceFormatBindings(std::move(vertexInstanceFormatBindings))
{
}

const static std::map<std::string, VertexFormat::Element::ElementType> cElementTypeByName{
    {"Int32", VertexFormat::Element::ElementType::t::Int32},
    {"Float", VertexFormat::Element::ElementType::t::Float},
    {"Vec2", VertexFormat::Element::ElementType::t::Vec2},
    {"Vec3", VertexFormat::Element::ElementType::t::Vec3},
    {"Vec4", VertexFormat::Element::ElementType::t::Vec4},
    {"Int16", VertexFormat::Element::ElementType::t::Int16},
    {"Float16", VertexFormat::Element::ElementType::t::Float16},
    {"F16Vec2", VertexFormat::Element::ElementType::t::F16Vec2},
    {"F16Vec3", VertexFormat::Element::ElementType::t::F16Vec3},
    {"F16Vec4", VertexFormat::Element::ElementType::t::F16Vec4}
};
const static std::map<std::string, VertexFormat::eInputRate> cBufferRateByName{
    {"Vertex", VertexFormat::eInputRate::Vertex},
    {"Instance", VertexFormat::eInputRate::Instance}
};

struct DescriptorTypeAndReadOnly {
    DescriptorSetDescription::DescriptorType type;
    bool readOnly = false;
};
const static std::map<std::string, DescriptorTypeAndReadOnly> cBufferTypeByName {
    {"ImageSampler",  {DescriptorSetDescription::DescriptorType::ImageSampler, true}},
    {"UniformBuffer", {DescriptorSetDescription::DescriptorType::UniformBuffer, true}},
    {"StorageBuffer", {DescriptorSetDescription::DescriptorType::StorageBuffer, false}},
    {"ImageStorage",  {DescriptorSetDescription::DescriptorType::ImageStorage, false}},
    {"InputAttachment",  {DescriptorSetDescription::DescriptorType::InputAttachment, true}}
};

const static std::map<std::string, DescriptorSetDescription::StageFlag> cStageFlagBitsByName{
    {"Vertex", DescriptorSetDescription::StageFlag::t::Vertex},
    {"Fragment", DescriptorSetDescription::StageFlag::t::Fragment},
    {"Geometry", DescriptorSetDescription::StageFlag::t::Geometry},
    {"Compute", DescriptorSetDescription::StageFlag::t::Compute}
};

const static std::map<std::string, ShaderPassDescription::DepthCompareOp> cDepthCompareOpByName{
    {"LessEqual",  ShaderPassDescription::DepthCompareOp::LessEqual},
    {"Equal", ShaderPassDescription::DepthCompareOp::Equal},
    {"Greater", ShaderPassDescription::DepthCompareOp::Greater},
};

const static std::map<std::string, ShaderPassDescription::BlendFactor> cBlendFactorByName{
    {"Zero",  ShaderPassDescription::BlendFactor::Zero},
    {"One", ShaderPassDescription::BlendFactor::One},
    {"SrcAlpha", ShaderPassDescription::BlendFactor::SrcAlpha},
    {"OneMinusSrcAlpha", ShaderPassDescription::BlendFactor::OneMinusSrcAlpha},
    {"DstAlpha", ShaderPassDescription::BlendFactor::DstAlpha},
    {"OneMinusDstAlpha", ShaderPassDescription::BlendFactor::OneMinusDstAlpha},
};

static void from_json(const Json& j, ShaderPassDescription::DepthCompareOp& op) {
    const auto foundIt = cDepthCompareOpByName.find(j);
    if (foundIt != cDepthCompareOpByName.end()) {
        op = foundIt->second;
    }
    else {
        throw std::invalid_argument("Unknown species");
    }
}

static void from_json(const Json& j, ShaderPassDescription::BlendFactor& op) {
    const auto foundIt = cBlendFactorByName.find(j);
    if (foundIt != cBlendFactorByName.end()) {
        op = foundIt->second;
    }
    else {
        throw std::invalid_argument("Unknown species");
    }
}

static void from_json(const Json& j, ShaderPassDescription::Output& output) {
    auto it = j.find("BlendEnable");
    if (it != j.end()) it->get_to(output.blendEnable);

    it = j.find("SrcColorBlendFactor");
    if (it != j.end()) it->get_to(output.srcColorBlendFactor);

    it = j.find("DstColorBlendFactor");
    if (it != j.end()) it->get_to(output.dstColorBlendFactor);

    it = j.find("SrcAlphaBlendFactor");
    if (it != j.end()) it->get_to(output.srcAlphaBlendFactor);

    it = j.find("DstAlphaBlendFactor");
    if (it != j.end()) it->get_to(output.dstAlphaBlendFactor);

    it = j.find("ColorWriteMask");
    if (it != j.end()) it->get_to(output.colorWriteMask);
}

static void from_json(const Json& j, ShaderPassDescription::FixedFunctionSettings& ffs) {
    auto it = j.find("DepthTestEnable");
    if (it != j.end()) it->get_to(ffs.depthTestEnable);

    it = j.find("DepthWriteEnable");
    if (it != j.end()) it->get_to(ffs.depthWriteEnable);

    it = j.find("DepthCompareOp");
    if (it != j.end()) it->get_to(ffs.depthCompareOp);

    it = j.find("DepthBiasEnable");
    if (it != j.end()) it->get_to(ffs.depthBiasEnable);

    it = j.find("DepthBiasConstant");
    if (it != j.end()) it->get_to(ffs.depthBiasConstant);

    it = j.find("DepthBiasClamp");
    if (it != j.end()) it->get_to(ffs.depthBiasClamp);

    it = j.find("DepthBiasSlope");
    if (it != j.end()) it->get_to(ffs.depthBiasSlope);

    it = j.find("CullBackFace");
    if (it != j.end()) it->get_to(ffs.cullBackFace);

    it = j.find("CullFrontFace");
    if (it != j.end()) it->get_to(ffs.cullFrontFace);
}

static void from_json(const Json& j, ShaderPassDescription::SampleShadingSettings& sss) {
    auto it = j.find("Enable");
    if (it != j.end()) it->get_to(sss.sampleShadingEnable);
    it = j.find("ForceCenterSample");
    if (it != j.end()) it->get_to(sss.forceCenterSample);
    it = j.find("Mask");
    if (it != j.end()) it->get_to(sss.sampleShadingMask);
}

static void from_json(const Json& j, ShaderPassDescription::WorkGroupSettings& ws) {
    auto it = j.find("LocalSize");
    if (it != j.end()) it->get_to(ws.localSize);
}

std::optional<ShaderDescription> ShaderDescriptionLoader::Load(AssetManager& assetManager, const std::string& filename)
{
    Json json;
#if true
    {
        std::vector<char> data;
        if (!assetManager.LoadFileIntoMemory(filename, data))
        {
            return std::nullopt;
        }
        json = Json::parse(data);
    }
#else
    // Load using file streams
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            return std::nullopt;
            //throw std::runtime_error("failed to open file!");
        }
        file >> json;
    }
#endif

    std::vector<VertexFormat> vertexFormats;
    std::map<std::string, uint32_t> vertexFormatLookup;   // lookup in to the vertexFormats vector (by vertex format name)

    if (json.contains("Vertex"))
    {
        // Array of 'Vertex" objects
        for (const auto& el : json["Vertex"])
        {
            std::vector<VertexFormat::Element> elements;
            std::vector<std::string> elementIds;

            VertexFormat::eInputRate rate = VertexFormat::eInputRate::Vertex;
            uint32_t span = el["Span"];
            uint32_t offset = 0;
            for (auto& ar : el["Elements"])
            {
                const std::string& elementType = ar["Type"];
                const auto typeIt = cElementTypeByName.find(elementType);
                assert(typeIt != cElementTypeByName.end());
                const uint32_t elementOffset = ar.contains("Offset") ? (uint32_t)ar["Offset"] : offset;
                elements.emplace_back(VertexFormat::Element{ elementOffset, typeIt->second });
                if (ar.contains("Name"))
                {
                    elementIds.emplace_back(ar["Name"]);
                }
                offset = elementOffset + typeIt->second.size();
                if (offset > span)
                {
                    throw std::runtime_error("Element outside of span range");
                }
            }
            if (elementIds.size() > 0 && elementIds.size() != elements.size())
            {
                throw std::runtime_error("all vertex elements must all be named or none named!");
            }
            if (el.contains("Name"))
            {
                vertexFormatLookup.try_emplace(el["Name"], (uint32_t)vertexFormats.size());   // put in the lookup before vertexFormats.emplace_back so correctly indexed (from 0)
            }
            if (el.contains("Rate"))
            {
                const auto rateIt = cBufferRateByName.find(el["Rate"]);
                assert(rateIt != cBufferRateByName.end());
                rate = rateIt->second;
            }
            vertexFormats.emplace_back(VertexFormat{ span, rate, std::move(elements), std::move(elementIds) });
        }
    }
    std::vector<std::string> passNames;
    std::vector<ShaderPassDescription> descriptions;
    for(const auto& ar: json["Passes"] )
    {
        std::string computeShader;
        std::string vertexShader;
        std::string fragmentShader;
        std::vector<DescriptorSetDescription> sets;
        std::vector<ShaderPassDescription::Output> outputs;
        std::string passname;
        std::vector<uint32_t> vertexFormatBindings;             // index(s) in to the VertexFormat array for vertex rate data (ie per vert)
        std::vector<uint32_t> instanceVertexFormatBindings;     // index(s) in to the VertexFormat array for instance rate data (ie per instance)
        ShaderPassDescription::FixedFunctionSettings fixedFunctionSettings;
        ShaderPassDescription::SampleShadingSettings sampleShadingSettings;
        ShaderPassDescription::WorkGroupSettings workGroupSettings;

        for (const auto& el : ar.items())
        {
            if ( el.key().compare("Name") == 0)
            {
                passname = el.value();
            }
            else if ( el.key().compare("Shaders") == 0)
            {
                // Either Compute or Vertex shader has to be defined, but Fragment is optional.
                computeShader = el.value().contains("Compute") ? (std::string) el.value()["Compute"] : std::string();
                vertexShader = el.value().contains("Vertex") ? (std::string) el.value()["Vertex"] : std::string();
                fragmentShader = el.value().contains("Fragment") ? (std::string) el.value()["Fragment"] : std::string();

                if (computeShader.empty() && vertexShader.empty())
                {
                    throw std::runtime_error("must have a Vertex or Compute shader name!");
                }
            }
            else if ( el.key().compare("DescriptorSets") == 0)
            {
                for (const auto& ar : el.value())
                {
                    std::vector<DescriptorSetDescription::DescriptorTypeAndCount> descriptors;
                    if (ar.contains("Buffers"))
                    {
                        auto& buffel = ar["Buffers"];
                        for (const auto& ar : buffel)
                        {
                            const std::string& bufferType = ar["Type"];
                            uint32_t count = ar.contains("Count") ? (uint32_t)ar["Count"] : 1;
                            bool readOnly = ar.contains("ReadOnly") ? (bool)ar["ReadOnly"] : false; // may be overridden if the 'Type' is intrinsically read-only
                            DescriptorSetDescription::StageFlag stageBindingFlags = { DescriptorSetDescription::StageFlag::t::None };
                            for (const auto& ar2 : ar["Stages"])
                            {
                                stageBindingFlags = stageBindingFlags | cStageFlagBitsByName.find(ar2)->second;
                            }
                            std::vector<std::string> names;
                            if (ar.contains("Names"))
                            {
                                for (auto& ar2 : ar["Names"])
                                {
                                    names.push_back(ar2);
                                }
                            }
                            assert(names.size() <= 1);  ///TODO: array of names does not currently work (especially since dynmamic 'Count' was added) 
                            //count = std::max(count, (uint32_t)names.size());
                            const auto& bufferTypeData = cBufferTypeByName.find(bufferType)->second;
                            descriptors.emplace_back(DescriptorSetDescription::DescriptorTypeAndCount{ bufferTypeData.type, stageBindingFlags, names, (int)count, readOnly || bufferTypeData.readOnly });
                        }
                    }
                    sets.push_back({ std::move(descriptors) });
                }
            }
            else if ( el.key().compare("Outputs") == 0)
            {
                for (const auto& ar2 : el.value())
                {
                    outputs.push_back(ar2);
                }
            }
            else if (el.key().compare("FixedFunction") == 0)
            {
                el.value().get_to(fixedFunctionSettings);// <ShaderPassDescription::FixedFunctionSettings>();
            }
            else if (el.key().compare("SampleShading") == 0)
            {
                el.value().get_to(sampleShadingSettings);// <ShaderPassDescription::SampleShadingSettings>();
            }
            else if (el.key().compare("WorkGroup") == 0)
            {
                el.value().get_to(workGroupSettings);// <ShaderPassDescription::WorkGroupSettings>();
            }
            else if (el.key().compare("VertexBindings") == 0)
            {
                // Array of VertexBinding names
                for (const auto& vertexFormatName : el.value())
                {
                    auto it = vertexFormatLookup.find(vertexFormatName);
                    if (it != vertexFormatLookup.end())
                    {
                        vertexFormatBindings.push_back(it->second);
                    }
                    else
                    {
                        throw std::runtime_error("VertexBinding does not match a Name in the Vertex array");
                    }
                }
            }
        }

        // Generate the 

        // static const VertexFormat format = 
        //     { 32, { VertexFormat::Element{ 0, VertexFormat::Element::ElementType::t::Vec3},
        //                                         VertexFormat::Element{12, VertexFormat::Element::ElementType::t::Vec3},
        //                                         VertexFormat::Element{24, VertexFormat::Element::ElementType::t::Vec2},
        //     } };

        descriptions.emplace_back(ShaderPassDescription( std::move(sets), std::move(outputs), std::move(computeShader), std::move(vertexShader), std::move(fragmentShader), std::move(fixedFunctionSettings), std::move(sampleShadingSettings), std::move(workGroupSettings), std::move(vertexFormatBindings)) );
        passNames.push_back(passname);
    }
    return std::make_optional(ShaderDescription{ std::move(vertexFormats), std::move(descriptions), passNames });
}

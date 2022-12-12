//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <map>
#include <string>
#include <array>
#include "tcb/span.hpp"

// Forward declarations
class AssetManager;
class ShaderModule;
class ShaderDescription;
class Shader;
class Vulkan;

/// Manages Shader loading (and lookup).
/// This is the entry point for applications to register (AddShader) the shaders they want to use and access the loaded data.
/// @ingroup Material
class ShaderManager
{
	ShaderManager() = delete;
	ShaderManager& operator=(const ShaderManager&) = delete;
	ShaderManager(const ShaderManager&) = delete;
public:
	ShaderManager(Vulkan& vulkan);
	~ShaderManager();

	/// User registration of render passes.  Passes must be registered before shaders referencing these passes can be loaded.
	/// Order of pass names is important (passes can be requested by index).
	void RegisterRenderPassNames(const tcb::span<const char*const> passNames);

	/// @brief Load a given shader definition (json file) and load all referenced shaders modules. Makes shader ready for material to be created from it.
	/// @param shaderName name to be given to this shader for lookup within the application code (user determined name).
	/// @param filename name of the (json formatted) shader desciption file to load and parse.  Filename should include extension.
	/// @return true if everything loaded correctly.
	bool AddShader(AssetManager& assetManager, const std::string& shaderName, const std::string& filename);

	/// Get description for the given shaderName
	/// @returns nullptr if shaderName unknown
	const ShaderDescription* GetShaderDescription(const std::string& shaderName) const;
	// Get Shader for the given shader name
	/// @returns nullptr if shaderName unknown
	const Shader* GetShader(const std::string& shaderName) const;

protected:
	bool AddShader(AssetManager& assetManager, const std::string& shaderName, ShaderDescription shaderDescription);

protected:
	Vulkan& m_vulkan;
	std::map<const std::string, ShaderDescription> m_shaderDescriptionsByName;	// Contains descriptions (module names, inputs etc) for all the passes of a given 'shader'
	std::map<std::string, ShaderModule> m_shaderModulesByName;				// Contains the vkShaderModule (one per hardware shader program)
	std::map<std::string, Shader> m_shadersByName;							// Contains all the passes of a given Shader (this contains what is described by m_shaderDescriptionsByName although is not the material)
	std::map < std::string, uint32_t> m_shaderPassIndexByName;				// Contains the registered render pass names (and their indices)
};

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
#include <span>
#include <memory>
#include <filesystem>

// Forward declarations
class AssetManager;
class GraphicsApiBase;
class ShaderDescription;
class ShaderBase;
class ShaderModuleBase;
template<typename T_GFXAPI> class ShaderManager;

/// Manages ShaderBase loading (and lookup).
/// This is the entry point for applications to register (AddShader) the shaders they want to use and access the loaded data.
/// @ingroup Material
class ShaderManagerBase
{
	ShaderManagerBase() = delete;
	ShaderManagerBase& operator=(const ShaderManagerBase&) = delete;
	ShaderManagerBase(const ShaderManagerBase&) = delete;
public:
    template<typename T_GFXAPI> using tApiDerived = ShaderManager<T_GFXAPI>; // make apiCast work!
    ShaderManagerBase(GraphicsApiBase& graphicsApi);
	virtual ~ShaderManagerBase();

	/// User registration of render passes.  Passes must be registered before shaders referencing these passes can be loaded.
	/// Order of pass names is important (passes can be requested by index).
	void RegisterRenderPassNames(const std::span<const char*const> passNames);

	/// @brief Load a given shader definition (json file) and load all referenced shaders modules. Makes shader ready for material to be created from it.
	/// @param shaderName name to be given to this shader for lookup within the application code (user determined name).
	/// @param filename name of the (json formatted) shader desciption file to load and parse.  Filename should include extension.
	/// @return true if everything loaded correctly.
	bool AddShader(AssetManager& assetManager, const std::string& shaderName, const std::string& filename, const std::filesystem::path& source_dir = std::filesystem::path());

	/// @brief Load a given shader definition (json file) and load all referenced shaders modules. Makes shader ready for material to be created from it.
	/// @param shaderName name to be given to this shader for lookup within the application code (user determined name).
	/// @param filename path of the (json formatted) shader desciption file to load and parse.  Filename should include extension.
	/// @return true if everything loaded correctly.
	bool AddShader(AssetManager& assetManager, const std::string& shaderName, const std::filesystem::path& filename, const std::filesystem::path& source_dir = std::filesystem::path());

	// Get ShaderBase for the given shader name
	/// @returns nullptr if shaderName unknown
	const ShaderBase* GetShader(const std::string& shaderName) const;

protected:
	virtual bool AddShader(AssetManager& assetManager, const std::string& shaderName, ShaderDescription shaderDescription) = 0;

protected:
    GraphicsApiBase& m_GraphicsApi;
	std::map<const std::string, ShaderDescription>          m_shaderDescriptionsByName;	// Contains descriptions (module names, inputs etc) for all the passes of a given 'shader'
	std::map<std::string, std::unique_ptr<ShaderModuleBase>>    m_shaderModulesByName;	    // Contains the ShaderModule (one per hardware shader program)
	std::map<std::string, std::unique_ptr<ShaderBase>>          m_shadersByName;			// Contains all the passes of a given ShaderBase (this contains what is described by m_shaderDescriptionsByName although is not the material)
	std::map < std::string, uint32_t>                       m_shaderPassIndexByName;	// Contains the registered render pass names (and their indices)
};

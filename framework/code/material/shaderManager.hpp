//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <map>
#include <string>
#include <span>
#include <memory>

// Forward declarations
class AssetManager;
class ShaderDescription;
class Shader;
class ShaderModule;
//template<typename T_GFXAPI> class ShaderT;
//template<typename T_GFXAPI> class ShaderModuleT;
template<typename T_GFXAPI> class ShaderManagerT;
class GraphicsApiBase;

/// Manages Shader loading (and lookup).
/// This is the entry point for applications to register (AddShader) the shaders they want to use and access the loaded data.
/// @ingroup Material
class ShaderManager
{
	ShaderManager() = delete;
	ShaderManager& operator=(const ShaderManager&) = delete;
	ShaderManager(const ShaderManager&) = delete;
public:
    template<typename T_GFXAPI> using tApiDerived = ShaderManagerT<T_GFXAPI>; // make apiCast work!
    ShaderManager(GraphicsApiBase& graphicsApi);
	virtual ~ShaderManager();

	/// User registration of render passes.  Passes must be registered before shaders referencing these passes can be loaded.
	/// Order of pass names is important (passes can be requested by index).
	void RegisterRenderPassNames(const std::span<const char*const> passNames);

	/// @brief Load a given shader definition (json file) and load all referenced shaders modules. Makes shader ready for material to be created from it.
	/// @param shaderName name to be given to this shader for lookup within the application code (user determined name).
	/// @param filename name of the (json formatted) shader desciption file to load and parse.  Filename should include extension.
	/// @return true if everything loaded correctly.
	bool AddShader(AssetManager& assetManager, const std::string& shaderName, const std::string& filename);

	// Get Shader for the given shader name
	/// @returns nullptr if shaderName unknown
	virtual const Shader* GetShader(const std::string& shaderName) const;

protected:
	virtual bool AddShader(AssetManager& assetManager, const std::string& shaderName, ShaderDescription shaderDescription) = 0;

protected:
    GraphicsApiBase& m_GraphicsApi;
	std::map<const std::string, ShaderDescription>          m_shaderDescriptionsByName;	// Contains descriptions (module names, inputs etc) for all the passes of a given 'shader'
	std::map<std::string, std::unique_ptr<ShaderModule>>    m_shaderModulesByName;	    // Contains the ShaderModule (one per hardware shader program)
	std::map<std::string, std::unique_ptr<Shader>>          m_shadersByName;			// Contains all the passes of a given Shader (this contains what is described by m_shaderDescriptionsByName although is not the material)
	std::map < std::string, uint32_t>                       m_shaderPassIndexByName;	// Contains the registered render pass names (and their indices)
};

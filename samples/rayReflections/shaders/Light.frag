//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#extension GL_EXT_ray_query : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

// Can kill these after final shader
#define PHI_CONST   0.618034
#define PI_CONST    3.14159265359
#define PI_MUL_2    6.28318530718

//*********************************************************
// Structures
//*********************************************************
//-----------------------------------------------------------------------------
struct SceneData
//-----------------------------------------------------------------------------
{
    // WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! 
    // mat4 MUST be on 16 byte boundary or the validation layer complains
    // Therefore, can't start with an "int" value in this structure without padding

    // The transform for this instance
    mat4    Transform;

    // The transform used for normals (Inverse Transpose)
    // See http://www.pbr-book.org/3ed-2018/Geometry_and_Transformations/Applying_Transformations.html#Normals
    mat4    TransformIT;

    // The index into the list of meshes.
    int     MeshIndex;

    // Material information
    int     DiffuseTex;     // -1 means no entry
    int     NormalTex;      // -1 means no entry

    int     Padding;        // So lines up on 16
};

//-----------------------------------------------------------------------------
struct VertData
//-----------------------------------------------------------------------------
{
    vec4 position;
    vec4 normal;
    vec4 uv0;
};

//-----------------------------------------------------------------------------
struct BounceInfo
//-----------------------------------------------------------------------------
{
    vec4 WorldPos;
    vec4 WorldDir;
};

//-----------------------------------------------------------------------------
struct RayPayload
//-----------------------------------------------------------------------------
{
    // Flag for hit or miss
    uint    IsHit;

    // Geometry instance ids
    int     PrimitiveIndex;
    int     InstanceID;
    int     InstCustIndex;
    // in     int   gl_GeometryIndexEXT;

    // World space parameters
    vec3   RayOrigin;
    vec3   RayDirection;

    // Ray hit info
    float  HitDist;
    uint   IsFrontFacing;

    // Barycentric Coordinates
    vec3    BaryCoords;
};

//-----------------------------------------------------------------------------
struct PixelPayload
//-----------------------------------------------------------------------------
{
    // TODO: Set this up for Hit/Miss shader 
    float   HitDist;

    // Vertex Data
    vec4    WorldPos;
    vec4    WorldNorm;
    vec4    Tangent;
    vec4    Bitan;
    vec4    Color;

    // Bump version of normal
    vec4    BumpNorm;

    // Texture Data
    vec4    DiffuseTexValue;
    vec4    NormalTexValue;

    // Results of Ray Trace
    // vec4    RayTraceColor;

    // Misc Data
    // mat4    Transform;
};

//*********************************************************
// Uniform/Texture Bindngs
//*********************************************************
// These binding locations are set by order they are defined in <>.json

layout(std140, binding = 0, set = 0) uniform FragConstantsBuff
{
    // Needed to convert from Depth to World Position
    mat4 ProjectionInv;
    mat4 ViewInv;

    // To apply a tint to the final results
    vec4 ShaderTint;

    // Standard lighting
    vec4 EyePos;
    vec4 LightDir;      // XYZ + Power/Exponent
    vec4 LightColor;    // RBG + Power/Exponent

    // X: Mip Level
    // Y: Reflect Amount
    // Z: Reflect Fresnel Min
    // W: Reflect Fresnel Max
    vec4    ReflectParams;

    // X: Mip Level
    // Y: IBL Amount
    // Z: Not Used
    // W: Not Used
    vec4    IrradianceParams;

    // X: Ray Min Distance
    // Y: Ray Max Distance
    // Z: Max Ray Bounces
    // W: Ray Blend Amount (Mirror Amount)
    vec4    RayParam01;

    // X: Shadow Amount
    // Y: Minumum Normal Dot Light
    // Z: Not Used
    // W: Not Used
    vec4    LightParams;

} FragCB;

#define SHADER_TINT             FragCB.ShaderTint

#define EYE_POS                 FragCB.EyePos
#define LIGHT_DIR               FragCB.LightDir
#define LIGHT_COLOR             FragCB.LightColor.rgb
#define LIGHT_POWER             FragCB.LightColor.w

#define REFLECT_MIP_LEVEL       FragCB.ReflectParams.x
#define REFLECT_AMOUNT          FragCB.ReflectParams.y
#define REFLECT_FRESNEL_MIN     FragCB.ReflectParams.z
#define REFLECT_FRESNEL_MAX     FragCB.ReflectParams.w

#define IRRADIANCE_MIP_LEVEL    FragCB.IrradianceParams.x
#define IRRADIANCE_AMOUNT       FragCB.IrradianceParams.y

#define RAY_MIN_DIST            FragCB.RayParam01.x
#define RAY_MAX_DIST            FragCB.RayParam01.y
#define RAY_MAX_BOUNCES         FragCB.RayParam01.z
#define RAY_BLEND_AMOUNT        FragCB.RayParam01.w

#define SHADOW_AMOUNT           FragCB.LightParams.x
#define MIN_NORM_DOT_LIGHT      FragCB.LightParams.y


layout (binding =  1, set = 0) uniform accelerationStructureEXT rayTraceAS; //set=0 required (compiler issue) https://github.com/KhronosGroup/glslang/issues/2247
layout (binding =  2, set = 0) uniform sampler2D TexArray[];
layout (binding =  3, set = 0, scalar) buffer SceneDataDef { SceneData Data[]; } SceneDataCB;
layout (binding =  4, set = 0, scalar) buffer VertDataDef { VertData Data[]; } VertDataCB[];
layout (binding =  5, set = 0) buffer IndiceDataDef { uint Data[]; } IndiceDataCB[];
layout (binding =  6, set = 0) uniform sampler2D     u_AlbedoTex;
layout (binding =  7, set = 0) uniform sampler2D     u_NormalTex;
layout (binding =  8, set = 0) uniform sampler2D     u_MatPropsTex;
layout (binding =  9, set = 0) uniform samplerCube   u_EnvironmentTex;
layout (binding = 10, set = 0) uniform samplerCube   u_IrradianceTex;

//*********************************************************
// Varying's
//*********************************************************
layout (location = 0) in vec2   v_TexCoord;

//*********************************************************
// Finally, the output color
//*********************************************************
layout (location = 0) out vec4 FragColor;

//*********************************************************
// Functions
//*********************************************************

//-----------------------------------------------------------------------------
vec3 ScreenToWorld(vec2 ScreenCoord/*0-1 range*/, float Depth/*0-1*/)
//-----------------------------------------------------------------------------
{
    vec4 ClipSpacePosition = vec4((ScreenCoord * 2.0) - vec2(1.0), Depth, 1.0);
    ClipSpacePosition.y = -ClipSpacePosition.y;
    vec4 ViewSpacePosition = FragCB.ProjectionInv * ClipSpacePosition;

    // Perspective division
    ViewSpacePosition /= vec4(ViewSpacePosition.w);

    vec4 WorldSpacePosition = FragCB.ViewInv * ViewSpacePosition;
    return WorldSpacePosition.xyz;
}

//-----------------------------------------------------------------------------
float InShadow(vec3 RayStartPos, vec3 RayDirection, vec3 SurfaceNormal)
//-----------------------------------------------------------------------------
{
    // TODO: Make this bias a configurable parameter
    vec3 LocalStart = RayStartPos + 0.01 * SurfaceNormal;

    // Set up the ray query
    rayQueryEXT rayQuery;
    uint RayFlags = gl_RayFlagsTerminateOnFirstHitEXT;
    rayQueryInitializeEXT(rayQuery, rayTraceAS, RayFlags, 0xFF, LocalStart, RAY_MIN_DIST, RayDirection, 100.0 * RAY_MAX_DIST);

    // Traverse until done
    while(rayQueryProceedEXT(rayQuery))
    {
        // Spec does this in the sample:
        // if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT) 
        // {
        //     rayQueryConfirmIntersectionEXT(rayQuery);
        // }
    }

    // What are the results of the traversal?
    float RetVal = 0.0;
    if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT)
    {
        // Didn't hit anything
        RetVal = 0.0;
    }
    else if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
    {
        // Hit something.
        // Lowering this value from 1.0 allows "ambient" in the shadow areas
        RetVal = SHADOW_AMOUNT;
    }

    return RetVal;
}

//-----------------------------------------------------------------------------
vec3 GetShadowMultiplier(vec3 RayStartPos, vec3 RayDirection, vec3 SurfaceNormal)
//-----------------------------------------------------------------------------
{
    // TODO: Make this bias a configurable parameter
    vec3 LocalStart = RayStartPos + 0.01 * SurfaceNormal;

    // Set up the ray query
    rayQueryEXT rayQuery;
    uint RayFlags = gl_RayFlagsTerminateOnFirstHitEXT;
    rayQueryInitializeEXT(rayQuery, rayTraceAS, RayFlags, 0xFF, LocalStart, RAY_MIN_DIST, RayDirection, 100.0 * RAY_MAX_DIST);

    // Traverse until done
    while(rayQueryProceedEXT(rayQuery))
    {
        // Spec does this in the sample:
        // if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT) 
        // {
        //     rayQueryConfirmIntersectionEXT(rayQuery);
        // }
    }

    // What are the results of the traversal?
    vec3 RetVal = vec3(1.0, 1.0, 1.0);
    if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT)
    {
        // Didn't hit anything
        RetVal = vec3(1.0, 1.0, 1.0);
    }
    else if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
    {
        // Hit something.
        // Lowering this value from 1.0 allows "ambient" in the shadow areas
        RetVal = vec3(SHADOW_AMOUNT, SHADOW_AMOUNT, SHADOW_AMOUNT);
    }

    return RetVal;
}

//-----------------------------------------------------------------------------
vec4 ShadePixel(PixelPayload PixelData)
//-----------------------------------------------------------------------------
{
    vec4 FragColor = vec4(0.0, 1.0, 0.0, 1.0);

    vec4 DiffuseColor = PixelData.DiffuseTexValue;
    DiffuseColor.xyzw *= SHADER_TINT;

    vec3 WorldNorm = PixelData.WorldNorm.xyz;
    vec3 WorldPos = PixelData.WorldPos.xyz;

    // ********************************
    // Shadows
    // ********************************
    // Go from pixel position towards the light (hence negative light direction)
    vec3 ShadowAmount = GetShadowMultiplier(WorldPos.xyz, -LIGHT_DIR.xyz, WorldNorm.xyz);

    // ********************************
    // Base Lighting
    // ********************************
    float DiffuseAmount = max(MIN_NORM_DOT_LIGHT, dot(WorldNorm.xyz, -LIGHT_DIR.xyz));
    vec3 LightTotal = DiffuseAmount * DiffuseColor.xyz * LIGHT_COLOR.xyz;

    // For specular, we need Half vector
    vec3 EyeDir = normalize(EYE_POS.xyz - WorldPos.xyz);
    vec3 Half = normalize(EyeDir - LIGHT_DIR.xyz);
    float Specular = max(0.0, dot(WorldNorm.xyz, Half));
    Specular = pow(Specular, LIGHT_POWER); 

    // Adjust specular by shadows (Not using 1.0 as edge in case numerical precision issues)
    // Specular *= (1.0 - step(ShadowAmount.w, 0.95));

    // ********************************
    // Reflection
    // ********************************
    vec3 N = WorldNorm.xyz;
    vec3 V = EyeDir.xyz;
    vec3 ReflectionVec = normalize( reflect(-V, N) );

    float ReflectionFresnel = 0.0;
    // float ReflectionFresnel = clamp(((clamp((1.0f - dot(N, V)), 0.0, 1.0) - REFLECT_FRESNEL_MIN)/(REFLECT_FRESNEL_MAX - REFLECT_FRESNEL_MIN)), 0.0, 1.0);
    ReflectionFresnel = clamp(1.0f - dot(N, V), 0.0, 1.0);
    ReflectionFresnel = (ReflectionFresnel - REFLECT_FRESNEL_MIN)/(REFLECT_FRESNEL_MAX - REFLECT_FRESNEL_MIN);
    ReflectionFresnel = clamp(ReflectionFresnel, 0.0, 1.0);

    vec4 EnvironmentColor = textureLod(u_EnvironmentTex, ReflectionVec.xyz, REFLECT_MIP_LEVEL);

    // ********************************
    // Image Based Lighting
    // ********************************
    // N.z *= -1.0;
    // vec4 IrradianceColor = textureLod(u_IrradianceTex, ReflectionVec.xyz, REFLECT_MIP_LEVEL);
    //  
    // // IBL is added to amount already calculated from dynamic lights
    // LightTotal += IRRADIANCE_AMOUNT * IrradianceColor.xyz;

    // ********************************
    // Start adding in the colors
    // ********************************
    // Adjust by shadows
    LightTotal.rgb *= ShadowAmount.xyz;

    vec3 FinalColor =   REFLECT_AMOUNT * ReflectionFresnel * EnvironmentColor.rgb +
                        LightTotal + 
                        Specular * LIGHT_COLOR;

    FragColor = vec4(FinalColor.rgb, 1.0);

    // DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! 
    // FragColor = vec4(PixelData.DiffuseTexValue.rgb, 1.0);
    // DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! 

    return FragColor;
}

//-----------------------------------------------------------------------------
RayPayload CreateHitPayload(rayQueryEXT rayQuery)
//-----------------------------------------------------------------------------
{
    RayPayload PayloadData;

    const bool IsCommitted = true;

    // Read as much information as we can about the hit
    // (https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GLSL_EXT_ray_tracing.txt)

    // Flag that this is a hit vector
    PayloadData.IsHit = 1;

    // Geometry instance ids
    PayloadData.PrimitiveIndex = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, IsCommitted);
    PayloadData.InstanceID = rayQueryGetIntersectionInstanceIdEXT(rayQuery, IsCommitted);
    PayloadData.InstCustIndex = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, IsCommitted);
    // in     int   gl_GeometryIndexEXT;

    // World space parameters
    PayloadData.RayOrigin  = rayQueryGetWorldRayOriginEXT(rayQuery);
    PayloadData.RayDirection = rayQueryGetWorldRayDirectionEXT(rayQuery);

    // Ray hit info
    // const uint gl_HitKindFrontFacingTriangleEXT = 0xFEU;
    // const uint gl_HitKindBackFacingTriangleEXT = 0xFFU;
    PayloadData.HitDist = rayQueryGetIntersectionTEXT(rayQuery, IsCommitted);
    PayloadData.IsFrontFacing = 1;
    bool IsFrontFacing = rayQueryGetIntersectionFrontFaceEXT(rayQuery, IsCommitted);
    if(!IsFrontFacing)
        PayloadData.IsFrontFacing = 0;

    // Barycentric Coordinates
    // Floating point barycentric coordinates of current intersection of ray.
    // Three Barycentric coordinates are such that their sum is 1.
    // This gives only two and expects us to calculate the third
    vec2 TwoBaryCoords = rayQueryGetIntersectionBarycentricsEXT(rayQuery, IsCommitted);
    PayloadData.BaryCoords = vec3(1.0 - TwoBaryCoords.x - TwoBaryCoords.y, TwoBaryCoords.x, TwoBaryCoords.y);

    return PayloadData;
}

//-----------------------------------------------------------------------------
BounceInfo GetNextBounceInfo(RayPayload OnePayload)
//-----------------------------------------------------------------------------
{
    BounceInfo RetVal;

    PixelPayload PixelData;

    const bool IsCommitted = true;

    // What is the base mesh? 
    int MeshIndx = SceneDataCB.Data[OnePayload.InstCustIndex].MeshIndex;

    // Indices for the triangle that was hit
    ivec3 Indices = ivec3(  IndiceDataCB[nonuniformEXT(MeshIndx)].Data[3 * OnePayload.PrimitiveIndex + 0],
                            IndiceDataCB[nonuniformEXT(MeshIndx)].Data[3 * OnePayload.PrimitiveIndex + 1],
                            IndiceDataCB[nonuniformEXT(MeshIndx)].Data[3 * OnePayload.PrimitiveIndex + 2]);

    // Vertices for the triangle that was hit
    VertData v0 = VertDataCB[nonuniformEXT(MeshIndx)].Data[Indices.x];
    VertData v1 = VertDataCB[nonuniformEXT(MeshIndx)].Data[Indices.y];
    VertData v2 = VertDataCB[nonuniformEXT(MeshIndx)].Data[Indices.z];

    // Should have everything we need to fill in the payload
    // Based on vertex data of triangle and Barycentric coordinates we can figure hit values
    RetVal.WorldPos = (v0.position * OnePayload.BaryCoords.x) + (v1.position * OnePayload.BaryCoords.y) + (v2.position * OnePayload.BaryCoords.z);
    RetVal.WorldPos = SceneDataCB.Data[OnePayload.InstCustIndex].Transform * RetVal.WorldPos;

    vec4 WorldNorm = (v0.normal * OnePayload.BaryCoords.x) + (v1.normal * OnePayload.BaryCoords.y) + (v2.normal * OnePayload.BaryCoords.z);
    WorldNorm = vec4(WorldNorm.xyz, 0.0);
    WorldNorm = SceneDataCB.Data[OnePayload.InstCustIndex].Transform * WorldNorm;
    WorldNorm = normalize(WorldNorm);

    // The new bounce direction is the reflection of incoming around normal
    vec3 ReflectionVec = normalize( reflect(OnePayload.RayDirection, WorldNorm.xyz) );
    RetVal.WorldDir = vec4(ReflectionVec, 0.0);

    // Adjust the starting position 
    // TODO: Make this bias a configurable parameter
    RetVal.WorldPos.xyz = RetVal.WorldPos.xyz + 0.01 * WorldNorm.xyz;

    return RetVal;
}


//-----------------------------------------------------------------------------
vec4 GetRayHitNormal(RayPayload OnePayload)
//-----------------------------------------------------------------------------
{
    const bool IsCommitted = true;

    vec4 RetNorm = vec4(0.0, 0.0, 1.0, 1.0);

    // What is the base mesh? 
    int MeshIndx = SceneDataCB.Data[OnePayload.InstCustIndex].MeshIndex;

    // Indices for the triangle that was hit
    ivec3 Indices = ivec3(  IndiceDataCB[nonuniformEXT(MeshIndx)].Data[3 * OnePayload.PrimitiveIndex + 0],
                            IndiceDataCB[nonuniformEXT(MeshIndx)].Data[3 * OnePayload.PrimitiveIndex + 1],
                            IndiceDataCB[nonuniformEXT(MeshIndx)].Data[3 * OnePayload.PrimitiveIndex + 2]);

    // Vertices for the triangle that was hit
    VertData v0 = VertDataCB[nonuniformEXT(MeshIndx)].Data[Indices.x];
    VertData v1 = VertDataCB[nonuniformEXT(MeshIndx)].Data[Indices.y];
    VertData v2 = VertDataCB[nonuniformEXT(MeshIndx)].Data[Indices.z];

    // Based on vertex data of triangle and Barycentric coordinates we can figure hit values

    RetNorm = (v0.normal * OnePayload.BaryCoords.x) + (v1.normal * OnePayload.BaryCoords.y) + (v2.normal * OnePayload.BaryCoords.z);
    RetNorm = vec4(RetNorm.xyz, 0.0);
    RetNorm = SceneDataCB.Data[OnePayload.InstCustIndex].Transform * RetNorm;
    RetNorm = normalize(RetNorm );

    return RetNorm;
}

//-----------------------------------------------------------------------------
vec4 ProcessRayHit(RayPayload OnePayload)
//-----------------------------------------------------------------------------
{
    PixelPayload PixelData;

    const bool IsCommitted = true;

    vec4 FragColor = vec4(1.0, 1.0, 1.0, 1.0);

    // What is the base mesh? 
    int MeshIndx = SceneDataCB.Data[OnePayload.InstCustIndex].MeshIndex;

    // Indices for the triangle that was hit
    ivec3 Indices = ivec3(  IndiceDataCB[nonuniformEXT(MeshIndx)].Data[3 * OnePayload.PrimitiveIndex + 0],
                            IndiceDataCB[nonuniformEXT(MeshIndx)].Data[3 * OnePayload.PrimitiveIndex + 1],
                            IndiceDataCB[nonuniformEXT(MeshIndx)].Data[3 * OnePayload.PrimitiveIndex + 2]);

    // Vertices for the triangle that was hit
    VertData v0 = VertDataCB[nonuniformEXT(MeshIndx)].Data[Indices.x];
    VertData v1 = VertDataCB[nonuniformEXT(MeshIndx)].Data[Indices.y];
    VertData v2 = VertDataCB[nonuniformEXT(MeshIndx)].Data[Indices.z];

    // Should have everything we need to fill in the payload
    // Based on vertex data of triangle and Barycentric coordinates we can figure hit values
    PixelData.WorldPos = (v0.position * OnePayload.BaryCoords.x) + (v1.position * OnePayload.BaryCoords.y) + (v2.position * OnePayload.BaryCoords.z);
    PixelData.WorldPos = SceneDataCB.Data[OnePayload.InstCustIndex].Transform * PixelData.WorldPos;

    PixelData.WorldNorm = (v0.normal * OnePayload.BaryCoords.x) + (v1.normal * OnePayload.BaryCoords.y) + (v2.normal * OnePayload.BaryCoords.z);
    PixelData.WorldNorm = vec4(PixelData.WorldNorm.xyz, 0.0);
    // PixelData.WorldNorm = SceneDataCB.Data[OnePayload.InstCustIndex].TransformIT * PixelData.WorldNorm;
    PixelData.WorldNorm = SceneDataCB.Data[OnePayload.InstCustIndex].Transform * PixelData.WorldNorm;
    PixelData.WorldNorm = normalize(PixelData.WorldNorm );

    // PixelData.Color = (v0.color * OnePayload.BaryCoords.x) + (v1.color * OnePayload.BaryCoords.y) + (v2.color * OnePayload.BaryCoords.z);
    PixelData.Color = vec4(0.8, 0.8, 0.8, 1.0);

    vec2 UV = (v0.uv0.xy * OnePayload.BaryCoords.x) + (v1.uv0.xy * OnePayload.BaryCoords.y) + (v2.uv0.xy * OnePayload.BaryCoords.z);

    // What are the textures?
    int DiffuseTex = SceneDataCB.Data[OnePayload.InstCustIndex].DiffuseTex;
    int NormalTex = SceneDataCB.Data[OnePayload.InstCustIndex].NormalTex;

    // TODO: Should the default tex color value be passed in?
    PixelData.DiffuseTexValue = vec4(0.8, 0.8, 0.8, 1.0);
    if(DiffuseTex >= 0)
    {
        // Try getting the lowest LOD from the texture
        // Later: Not lowest, but take about half
        // int NumMipLevels = textureQueryLevels(TexArray[nonuniformEXT(DiffuseTex)]);
        // PixelData.DiffuseTexValue = textureLod(TexArray[nonuniformEXT(DiffuseTex)], UV, NumMipLevels / 2);

        // Get whatever mip the driver gives us
        PixelData.DiffuseTexValue = texture(TexArray[nonuniformEXT(DiffuseTex)], UV);
    }

    // Default normal is flat bump map
    PixelData.NormalTexValue = vec4(0.0, 0.0, 1.0, 1.0);
    // if(NormalTex >= 0)
    // {
    //     // Try getting the lowest LOD from the texture
    //     int NumMipLevels = textureQueryLevels(TexArray[nonuniformEXT(NormalTex)]);
    //     PixelData.NormalTexValue= textureLod(TexArray[nonuniformEXT(DiffuseTex)], UV, (NumMipLevels - 1));
    // 
    //     // PixelData.NormalTexValue = texture(TexArray[nonuniformEXT(NormalTex)], UV);
    // }

    // Turn off bump for now
    PixelData.BumpNorm = PixelData.WorldNorm;
    
    // PixelData.Transform = SceneDataCB.Data[OnePayload.InstCustIndex].Transform;
    // PixelData.TransformIT = SceneDataCB.Data[OnePayload.InstCustIndex].TransformIT;

    FragColor = ShadePixel(PixelData);

    return FragColor;
}

//-----------------------------------------------------------------------------
vec4 GetRayColor(vec3 RayStartPos, vec3 RayDirection, vec3 SurfaceNormal, vec4 MaterialProps)
//-----------------------------------------------------------------------------
{
    // TODO: (1,1,1) will create a "white" based on the blend amount.
    //      (0,0,0) "looks" correct, but is it really?
    vec4 RetVal = vec4(0.0, 0.0, 0.0, 1.0);

    // TODO: Make this bias a configurable parameter
    vec3 LocalStart = RayStartPos + 0.01 * SurfaceNormal;
    vec3 LocalDirection = RayDirection;

    BounceInfo NextBounce;
    NextBounce.WorldPos = vec4(LocalStart, 0.0);
    NextBounce.WorldDir = vec4(LocalDirection, 0.0);

    // Set up the ray query
    rayQueryEXT rayQuery;
    uint RayFlags = gl_RayFlagsNoneEXT; // NOT this =>gl_RayFlagsTerminateOnFirstHitEXT;

    int MatRayBounces = int(MaterialProps.x * 255.0);
    int MaxBounces = min(MatRayBounces, int(RAY_MAX_BOUNCES));
    int WhichBounce = 0;
    while(WhichBounce < MaxBounces)
    {
        // Bump my bounce count here (not at bottom) so logic below is correct
        WhichBounce = WhichBounce + 1;

        rayQueryInitializeEXT(rayQuery, rayTraceAS, RayFlags, 0xFF, NextBounce.WorldPos.xyz, RAY_MIN_DIST, NextBounce.WorldDir.xyz, 100.0 * RAY_MAX_DIST);

        // Traverse until done
        while(rayQueryProceedEXT(rayQuery))
        {
            // Spec does this in the sample:
            // if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT) 
            // {
            //     rayQueryConfirmIntersectionEXT(rayQuery);
            // }
        }

        // What are the results of the traversal?
        if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT)
        {
            // Didn't hit anything.
            // Return the skybox value
            WhichBounce = int(RAY_MAX_BOUNCES);
            vec4 EnvironmentColor = textureLod(u_EnvironmentTex, NextBounce.WorldDir.xyz, REFLECT_MIP_LEVEL);
            RetVal = mix(RetVal, EnvironmentColor, RAY_BLEND_AMOUNT);
        }
        else if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT &&
                rayQueryGetIntersectionFrontFaceEXT(rayQuery, true))
        {
            // Hit something.
            RayPayload OnePayload = CreateHitPayload(rayQuery);

            if(WhichBounce < int(RAY_MAX_BOUNCES))
            {
                // Need to keep bouncing
                // TODO: This is wrong!  Need to ACCUMULATE colors!  Testing for now
                NextBounce = GetNextBounceInfo(OnePayload);
            }
            else
            {
                // We always get the color value and accumulate it
                // RetVal = ProcessRayHit(OnePayload);
            }

            // What about shadows? Are they handled?
            vec4 OneBounceColor = ProcessRayHit(OnePayload);
            RetVal = mix(RetVal, OneBounceColor, RAY_BLEND_AMOUNT);
        }
    }   // WhichBounce

    return RetVal;
}

//-----------------------------------------------------------------------------
void main()
//-----------------------------------------------------------------------------
{
    PixelPayload PixelData;

    vec2 LocalTexCoord = vec2(v_TexCoord.xy);

    // ********************************
    // Material Properties
    // ********************************
    vec4 MaterialProps = texture( u_MatPropsTex, LocalTexCoord.xy );


    // ********************************
    // GBuffer Color, Normal, and Position
    // ********************************
    vec4 DiffuseColor = texture( u_AlbedoTex, LocalTexCoord.xy );
    DiffuseColor.xyzw *= SHADER_TINT;

    vec4 NormalWithDepth = texture( u_NormalTex, LocalTexCoord.xy );
    vec3 WorldNorm = NormalWithDepth.xyz;
    float Depth = 1.0 - NormalWithDepth.w;

    vec3 WorldPos = ScreenToWorld( LocalTexCoord, Depth );

    // ********************************
    // Reflection
    // ********************************
    vec3 EyeDir = normalize(EYE_POS.xyz - WorldPos.xyz);
    vec3 N = WorldNorm.xyz;
    vec3 V = EyeDir.xyz;
    vec3 ReflectionVec = normalize( reflect(-V, N) );

    vec4 RayTraceColor = GetRayColor(WorldPos.xyz, ReflectionVec.xyz, WorldNorm.xyz, MaterialProps);

    // ********************************
    // Fill in pixel data and shade
    // ********************************
    PixelData.WorldPos = vec4(WorldPos, 1.0);
    PixelData.WorldNorm = vec4(WorldNorm, 0.0);

    // TODO: These are not filled in for deferred renderer
    PixelData.Tangent = vec4(1.0, 0.0, 0.0, 1.0);
    PixelData.Bitan = vec4(0.0, 1.0, 0.0, 1.0);
    PixelData.Color = vec4(0.8, 0.8, 0.8, 1.0);
    PixelData.BumpNorm = PixelData.WorldNorm;  // This is in bump space

    PixelData.DiffuseTexValue = DiffuseColor;
    PixelData.NormalTexValue = PixelData.WorldNorm;

    FragColor = ShadePixel(PixelData);

    FragColor = mix(FragColor, RayTraceColor, RAY_BLEND_AMOUNT);

    // Debug! Debug! Debug! Debug! Debug! Debug! Debug! Debug! Debug! 
    // Debug! Debug! Debug! Debug! Debug! Debug! Debug! Debug! Debug! 
    // FragColor = RayTraceColor;

    // vec4 MaterialProps = texture( u_MatPropsTex, LocalTexCoord.xy );
    // FragColor = vec4(MaterialProps.rgb, 1.0);

}


#version 410 core

#extension GL_ARB_shading_language_packing : enable

#extension GL_GOOGLE_cpp_style_line_directive : enable

#define TARGET_GL_ENVIRONMENT
#define FILAMENT_OPENGL_SEMANTICS
#define FILAMENT_HAS_FEATURE_TEXTURE_GATHER
#define FILAMENT_HAS_FEATURE_INSTANCING
#define FILAMENT_EFFECTIVE_VERSION __VERSION__
#define FILAMENT_STEREO_INSTANCED
#define FLIP_UV_ATTRIBUTE
#define VARYING out
#define ATTRIBUTE in
#define SHADING_MODEL_LIT
#define FILAMENT_QUALITY_LOW    0
#define FILAMENT_QUALITY_NORMAL 1
#define FILAMENT_QUALITY_HIGH   2
#define FILAMENT_QUALITY FILAMENT_QUALITY_HIGH

precision highp float;
precision highp int;

#ifndef SPIRV_CROSS_CONSTANT_ID_0
#define SPIRV_CROSS_CONSTANT_ID_0 1
#endif
const int BACKEND_FEATURE_LEVEL = SPIRV_CROSS_CONSTANT_ID_0;

#ifndef SPIRV_CROSS_CONSTANT_ID_1
#define SPIRV_CROSS_CONSTANT_ID_1 64
#endif
const int CONFIG_MAX_INSTANCES = SPIRV_CROSS_CONSTANT_ID_1;

#ifndef SPIRV_CROSS_CONSTANT_ID_4
#define SPIRV_CROSS_CONSTANT_ID_4 1024
#endif
const int CONFIG_FROXEL_BUFFER_HEIGHT = SPIRV_CROSS_CONSTANT_ID_4;

#ifndef SPIRV_CROSS_CONSTANT_ID_11
#define SPIRV_CROSS_CONSTANT_ID_11 16384
#endif
const int CONFIG_FROXEL_RECORD_BUFFER_HEIGHT = SPIRV_CROSS_CONSTANT_ID_11;

#ifndef SPIRV_CROSS_CONSTANT_ID_6
#define SPIRV_CROSS_CONSTANT_ID_6 false
#endif
const bool CONFIG_DEBUG_DIRECTIONAL_SHADOWMAP = SPIRV_CROSS_CONSTANT_ID_6;

#ifndef SPIRV_CROSS_CONSTANT_ID_7
#define SPIRV_CROSS_CONSTANT_ID_7 false
#endif
const bool CONFIG_DEBUG_FROXEL_VISUALIZATION = SPIRV_CROSS_CONSTANT_ID_7;

#ifndef SPIRV_CROSS_CONSTANT_ID_2
#define SPIRV_CROSS_CONSTANT_ID_2 false
#endif
const bool CONFIG_STATIC_TEXTURE_TARGET_WORKAROUND = SPIRV_CROSS_CONSTANT_ID_2;

#ifndef SPIRV_CROSS_CONSTANT_ID_5
#define SPIRV_CROSS_CONSTANT_ID_5 false
#endif
const bool CONFIG_POWER_VR_SHADER_WORKAROUNDS = SPIRV_CROSS_CONSTANT_ID_5;

#ifndef SPIRV_CROSS_CONSTANT_ID_8
#define SPIRV_CROSS_CONSTANT_ID_8 2
#endif
const int CONFIG_STEREO_EYE_COUNT = SPIRV_CROSS_CONSTANT_ID_8;

#ifndef SPIRV_CROSS_CONSTANT_ID_9
#define SPIRV_CROSS_CONSTANT_ID_9 3
#endif
const int CONFIG_SH_BANDS_COUNT = SPIRV_CROSS_CONSTANT_ID_9;

#ifndef SPIRV_CROSS_CONSTANT_ID_10
#define SPIRV_CROSS_CONSTANT_ID_10 1
#endif
const int CONFIG_SHADOW_SAMPLING_METHOD = SPIRV_CROSS_CONSTANT_ID_10;

#define CONFIG_MAX_STEREOSCOPIC_EYES 4

#if defined(FILAMENT_VULKAN_SEMANTICS)
#define LAYOUT_LOCATION(x) layout(location = x)
#else
#define LAYOUT_LOCATION(x)
#endif
#define bool2    bvec2
#define bool3    bvec3
#define bool4    bvec4
#define int2     ivec2
#define int3     ivec3
#define int4     ivec4
#define uint2    uvec2
#define uint3    uvec3
#define uint4    uvec4
#define float2   vec2
#define float3   vec3
#define float4   vec4
#define float3x3 mat3
#define float4x4 mat4
#if defined(TARGET_VULKAN_ENVIRONMENT) && defined(TARGET_MOBILE)
#define highp_mat4 highp mat4
#else
#define highp_mat4 const highp mat4
#endif

#define VARIANT_HAS_DIRECTIONAL_LIGHTING

#define MATERIAL_FEATURE_LEVEL 1
#define MATERIAL_HAS_BASE_COLOR
#define MATERIAL_HAS_ROUGHNESS
#define MATERIAL_HAS_METALLIC
#define MATERIAL_HAS_REFLECTANCE
#define SHADING_INTERPOLATION 

#define HAS_ATTRIBUTE_POSITION
#define HAS_ATTRIBUTE_TANGENTS

layout(location = 0) in vec4 mesh_position;
layout(location = 1) in vec4 mesh_tangents;

struct Constants {
int morphingBufferOffset;
};
LAYOUT_LOCATION(32) uniform Constants pushConstants;


#if defined(HAS_ATTRIBUTE_COLOR)
LAYOUT_LOCATION(4) VARYING mediump vec4 vertex_color;
#endif
#if defined(HAS_ATTRIBUTE_UV0) && !defined(HAS_ATTRIBUTE_UV1)
LAYOUT_LOCATION(5) VARYING highp vec2 vertex_uv01;
#elif defined(HAS_ATTRIBUTE_UV1)
LAYOUT_LOCATION(5) VARYING highp vec4 vertex_uv01;
#endif
LAYOUT_LOCATION(6) VARYING highp vec4 vertex_worldPosition;
#if defined(HAS_ATTRIBUTE_TANGENTS)
LAYOUT_LOCATION(7) SHADING_INTERPOLATION VARYING mediump vec3 vertex_worldNormal;
#if defined(MATERIAL_NEEDS_TBN)
LAYOUT_LOCATION(8) SHADING_INTERPOLATION VARYING mediump vec4 vertex_worldTangent;
#endif
#endif
LAYOUT_LOCATION(9) VARYING highp vec4 vertex_position;
#if defined(FILAMENT_HAS_FEATURE_INSTANCING)
LAYOUT_LOCATION(10) flat VARYING highp int instance_index;
highp int logical_instance_index;
#endif
#if defined(VARIANT_HAS_SHADOWING) && defined(VARIANT_HAS_DIRECTIONAL_LIGHTING)
LAYOUT_LOCATION(11) VARYING highp vec4 vertex_lightSpacePosition;
#endif
#if defined(VARIANT_HAS_STEREO) && defined(FILAMENT_STEREO_INSTANCED)
#if defined(GL_ES) && defined(FILAMENT_GLSLANG)
LAYOUT_LOCATION(100) out float filament_gl_ClipDistance[2];
#define FILAMENT_CLIPDISTANCE filament_gl_ClipDistance
#else
#define FILAMENT_CLIPDISTANCE gl_ClipDistance
#endif
#endif


#if defined(VARIANT_HAS_SHADOWING)
struct ShadowData {
highp mat4 lightFromWorldMatrix;
highp vec4 lightFromWorldZ;
highp vec4 scissorNormalized;
mediump float texelSizeAtOneMeter;
mediump float bulbRadiusLs;
mediump float nearOverFarMinusNear;
mediump float normalBias;
bool elvsm;
mediump uint layer;
mediump uint reserved1;
mediump uint reserved2;
};
#endif
#if defined(VARIANT_HAS_SKINNING_OR_MORPHING)
struct BoneData {
highp mat3x4 transform;
highp float3 cof0;
highp float cof1x;
};
#endif
struct PerRenderableData {
highp mat4 worldFromModelMatrix;
highp mat3 worldFromModelNormalMatrix;
highp int morphTargetCount;
highp int flagsChannels;
highp int objectId;
highp float userData;
highp vec4 reserved[8];
};
#define FILAMENT_OBJECT_SKINNING_ENABLED_BIT   0x100
#define FILAMENT_OBJECT_MORPHING_ENABLED_BIT   0x200
#define FILAMENT_OBJECT_CONTACT_SHADOWS_BIT    0x400
#define FILAMENT_OBJECT_INSTANCE_BUFFER_BIT    0x800
#define VERTEX_DOMAIN_OBJECT

layout(std140) uniform FrameUniforms {
    mat4 viewFromWorldMatrix;
    mat4 worldFromViewMatrix;
    mat4 clipFromViewMatrix;
    mat4 viewFromClipMatrix;
    mat4 eyeFromViewMatrix[4];
    mat4 clipFromWorldMatrix[4];
    mat4 worldFromClipMatrix;
    mat4 userWorldFromWorldMatrix;
    vec4 clipTransform;
    vec2 clipControl;
    float time;
    float temporalNoise;
    vec4 userTime;
    vec4 resolution;
    vec2 logicalViewportScale;
    vec2 logicalViewportOffset;
    float lodBias;
    float refractionLodOffset;
    lowp vec2 derivativesScale;
    float oneOverFarMinusNear;
    float nearOverFarMinusNear;
    float cameraFar;
    float exposure;
    float ev100;
    float needsAlphaChannel;
    lowp float aoSamplingQualityAndEdgeDistance;
    lowp float aoBentNormals;
    lowp vec4 zParams;
    lowp uvec3 fParams;
    lowp int lightChannels;
    lowp vec2 froxelCountXY;
    lowp int enableFroxelViz;
    lowp int dynReserved0;
    lowp int dynReserved1;
    lowp int dynReserved2;
    float iblLuminance;
    float iblRoughnessOneLevel;
    lowp vec3 iblSH[9];
    vec3 lightDirection;
    lowp float padding0;
    vec4 lightColorIntensity;
    vec4 sun;
    vec2 shadowFarAttenuationParams;
    lowp int directionalShadows;
    lowp float ssContactShadowDistance;
    vec4 cascadeSplits;
    lowp int cascades;
    lowp float shadowPenumbraRatioScale;
    vec2 lightFarAttenuationParams;
    lowp float vsmExponent;
    lowp float vsmDepthScale;
    lowp float vsmLightBleedReduction;
    lowp uint shadowSamplingType;
    vec3 fogDensity;
    float fogStart;
    float fogMaxOpacity;
    uint fogMinMaxMip;
    float fogHeightFalloff;
    float fogCutOffDistance;
    vec3 fogColor;
    float fogColorFromIbl;
    float fogInscatteringStart;
    float fogInscatteringSize;
    float fogOneOverFarMinusNear;
    float fogNearOverFarMinusNear;
    mat3 fogFromWorldMatrix;
    vec2 fogLinearParams;
    vec2 fogReserved0;
    mat4 ssrReprojection;
    mat4 ssrUvFromViewMatrix;
    lowp float ssrThickness;
    lowp float ssrBias;
    lowp float ssrDistance;
    lowp float ssrStride;
    vec4 custom[4];
    int rec709;
    lowp float es2Reserved0;
    lowp float es2Reserved1;
    lowp float es2Reserved2;
    lowp vec4 reserved[22];
} frameUniforms;

layout(std140) uniform ObjectUniforms {
    PerRenderableData data[CONFIG_MAX_INSTANCES];
} objectUniforms;

layout(std140) uniform MaterialParams {
    vec3 baseColor;
    float metallic;
    float roughness;
    float reflectance;
} materialParams;


#define PI                          3.14159265359
#define HALF_PI                     1.570796327
#define MEDIUMP_FLT_MAX             65504.0
#define MEDIUMP_FLT_MIN             0.00006103515625
#ifdef TARGET_MOBILE
#define FLT_EPS                     MEDIUMP_FLT_MIN
#else
#define FLT_EPS                     1e-5
#endif
#define saturate(x)                 clamp(x, 0.0, 1.0)
#ifdef TARGET_MOBILE
#define PREVENT_DIV0(n, d, magic)   ((n) / max(d, magic))
#else
#define PREVENT_DIV0(n, d, magic)   ((n) / (d))
#endif
float pow5(float x) {
float x2 = x * x;
return x2 * x2 * x;
}
float sq(float x) {
return x * x;
}
float max3(const vec3 v) {
return max(v.x, max(v.y, v.z));
}
float vmax(const vec2 v) {
return max(v.x, v.y);
}
float vmax(const vec3 v) {
return max(v.x, max(v.y, v.z));
}
float vmax(const vec4 v) {
return max(max(v.x, v.y), max(v.z, v.w));
}
float min3(const vec3 v) {
return min(v.x, min(v.y, v.z));
}
float vmin(const vec2 v) {
return min(v.x, v.y);
}
float vmin(const vec3 v) {
return min(v.x, min(v.y, v.z));
}
float vmin(const vec4 v) {
return min(min(v.x, v.y), min(v.z, v.w));
}
float acosFast(float x) {
float y = abs(x);
float p = -0.1565827 * y + 1.570796;
p *= sqrt(1.0 - y);
return x >= 0.0 ? p : PI - p;
}
float acosFastPositive(float x) {
float p = -0.1565827 * x + 1.570796;
return p * sqrt(1.0 - x);
}
highp vec4 mulMat4x4Float3(const highp mat4 m, const highp vec3 v) {
return v.x * m[0] + (v.y * m[1] + (v.z * m[2] + m[3]));
}
highp vec3 mulMat3x3Float3(const highp mat4 m, const highp vec3 v) {
return v.x * m[0].xyz + (v.y * m[1].xyz + (v.z * m[2].xyz));
}
void toTangentFrame(const highp vec4 q, out highp vec3 n) {
n = vec3( 0.0,  0.0,  1.0) +
vec3( 2.0, -2.0, -2.0) * q.x * q.zwx +
vec3( 2.0,  2.0, -2.0) * q.y * q.wzy;
}
void toTangentFrame(const highp vec4 q, out highp vec3 n, out highp vec3 t) {
toTangentFrame(q, n);
t = vec3( 1.0,  0.0,  0.0) +
vec3(-2.0,  2.0, -2.0) * q.y * q.yxw +
vec3(-2.0,  2.0,  2.0) * q.z * q.zwx;
}
highp mat3 cofactor(const highp mat3 m) {
highp float a = m[0][0];
highp float b = m[1][0];
highp float c = m[2][0];
highp float d = m[0][1];
highp float e = m[1][1];
highp float f = m[2][1];
highp float g = m[0][2];
highp float h = m[1][2];
highp float i = m[2][2];
highp mat3 cof;
cof[0][0] = e * i - f * h;
cof[0][1] = c * h - b * i;
cof[0][2] = b * f - c * e;
cof[1][0] = f * g - d * i;
cof[1][1] = a * i - c * g;
cof[1][2] = c * d - a * f;
cof[2][0] = d * h - e * g;
cof[2][1] = b * g - a * h;
cof[2][2] = a * e - b * d;
return cof;
}
float interleavedGradientNoise(highp vec2 w) {
const vec3 m = vec3(0.06711056, 0.00583715, 52.9829189);
return fract(m.z * fract(dot(w, m.xy)));
}

highp mat4 object_uniforms_worldFromModelMatrix;
highp mat3 object_uniforms_worldFromModelNormalMatrix;
highp int object_uniforms_morphTargetCount;
highp int object_uniforms_flagsChannels;
highp int object_uniforms_objectId;
highp float object_uniforms_userData;
void initObjectUniforms() {
#if defined(FILAMENT_HAS_FEATURE_INSTANCING)
highp int i;
#   if defined(MATERIAL_HAS_INSTANCES)
if ((objectUniforms.data[0].flagsChannels & FILAMENT_OBJECT_INSTANCE_BUFFER_BIT) != 0) {
i = logical_instance_index;
} else {
i = 0;
}
#   else
i = logical_instance_index;
#   endif
#else
const int i = 0;
#endif
object_uniforms_worldFromModelMatrix        = objectUniforms.data[i].worldFromModelMatrix;
object_uniforms_worldFromModelNormalMatrix  = objectUniforms.data[i].worldFromModelNormalMatrix;
object_uniforms_morphTargetCount            = objectUniforms.data[i].morphTargetCount;
object_uniforms_flagsChannels               = objectUniforms.data[i].flagsChannels;
object_uniforms_objectId                    = objectUniforms.data[i].objectId;
object_uniforms_userData                    = objectUniforms.data[i].userData;
}
#if defined(FILAMENT_HAS_FEATURE_INSTANCING) && defined(MATERIAL_HAS_INSTANCES)
highp int getInstanceIndex() {
return logical_instance_index;
}
#endif

#if defined(VARIANT_HAS_SHADOWING)
highp vec4 computeLightSpacePosition(highp vec3 p, const highp vec3 n,
const highp vec3 dir, const float b, highp_mat4 lightFromWorldMatrix) {
#if !defined(VARIANT_HAS_VSM)
highp float cosTheta = saturate(dot(n, dir));
highp float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
p += n * (sinTheta * b);
#endif
return mulMat4x4Float3(lightFromWorldMatrix, p);
}
#endif

int getEyeIndex() {
#if defined(VARIANT_HAS_STEREO) && defined(FILAMENT_STEREO_INSTANCED)
return instance_index % CONFIG_STEREO_EYE_COUNT;
#elif defined(VARIANT_HAS_STEREO) && defined(FILAMENT_STEREO_MULTIVIEW)
#   if defined(TARGET_VULKAN_ENVIRONMENT)
return int(gl_ViewIndex);
#   else
return int(gl_ViewID_OVR);
#   endif
#endif
return 0;
}
highp mat4 getViewFromWorldMatrix() {
return frameUniforms.viewFromWorldMatrix;
}
highp mat4 getWorldFromViewMatrix() {
return frameUniforms.worldFromViewMatrix;
}
highp mat4 getClipFromViewMatrix() {
return frameUniforms.clipFromViewMatrix;
}
highp mat4 getViewFromClipMatrix() {
return frameUniforms.viewFromClipMatrix;
}
highp mat4 getEyeFromViewMatrix() {
return frameUniforms.eyeFromViewMatrix[getEyeIndex()];
}
highp mat4 getEyeFromViewMatrix(int eyeIndex) {
return frameUniforms.eyeFromViewMatrix[eyeIndex];
}
highp mat4 getClipFromWorldMatrix() {
return frameUniforms.clipFromWorldMatrix[getEyeIndex()];
}
highp mat4 getClipFromWorldMatrix(int eyeIndex) {
return frameUniforms.clipFromWorldMatrix[eyeIndex];
}
highp mat4 getWorldFromClipMatrix() {
return frameUniforms.worldFromClipMatrix;
}
highp mat4 getUserWorldFromWorldMatrix() {
return frameUniforms.userWorldFromWorldMatrix;
}
float getTime() {
return frameUniforms.time;
}
highp vec4 getUserTime() {
return frameUniforms.userTime;
}
highp float getUserTimeMod(float m) {
return mod(mod(frameUniforms.userTime.x, m) + mod(frameUniforms.userTime.y, m), m);
}
highp vec2 uvToRenderTargetUV(const highp vec2 uv) {
#if defined(TARGET_METAL_ENVIRONMENT) || defined(TARGET_VULKAN_ENVIRONMENT) || defined(TARGET_WEBGPU_ENVIRONMENT)
return vec2(uv.x, 1.0 - uv.y);
#else
return uv;
#endif
}
highp vec4 getResolution() {
return frameUniforms.resolution;
}
highp vec3 getWorldCameraPosition() {
return frameUniforms.worldFromViewMatrix[3].xyz;
}
highp vec3 getWorldOffset() {
return getUserWorldFromWorldMatrix()[3].xyz;
}
float getExposure() {
return frameUniforms.exposure;
}
float getEV100() {
return frameUniforms.ev100;
}
highp vec4 getMaterialGlobal0() {
return frameUniforms.custom[0];
}
highp vec4 getMaterialGlobal1() {
return frameUniforms.custom[1];
}
highp vec4 getMaterialGlobal2() {
return frameUniforms.custom[2];
}
highp vec4 getMaterialGlobal3() {
return frameUniforms.custom[3];
}

mat4 getWorldFromModelMatrix() {
return object_uniforms_worldFromModelMatrix;
}
mat3 getWorldFromModelNormalMatrix() {
return object_uniforms_worldFromModelNormalMatrix;
}
float getObjectUserData() {
return object_uniforms_userData;
}
#if __VERSION__ >= 300
int getVertexIndex() {
#if defined(TARGET_METAL_ENVIRONMENT) || defined(TARGET_VULKAN_ENVIRONMENT) || defined(TARGET_WEBGPU_ENVIRONMENT)
return gl_VertexIndex;
#else
return gl_VertexID;
#endif
}
#endif
#if defined(VARIANT_HAS_SKINNING_OR_MORPHING)
#define MAX_SKINNING_BUFFER_WIDTH 2048u
vec3 mulBoneNormal(vec3 n, uint j) {
highp mat3 cof;
highp float a = bonesUniforms.bones[j].transform[0][0];
highp float b = bonesUniforms.bones[j].transform[0][1];
highp float c = bonesUniforms.bones[j].transform[0][2];
highp float d = bonesUniforms.bones[j].transform[1][0];
highp float e = bonesUniforms.bones[j].transform[1][1];
highp float f = bonesUniforms.bones[j].transform[1][2];
highp float g = bonesUniforms.bones[j].transform[2][0];
highp float h = bonesUniforms.bones[j].transform[2][1];
highp float i = bonesUniforms.bones[j].transform[2][2];
cof[0] = bonesUniforms.bones[j].cof0;
cof[1].xyz = vec3(
bonesUniforms.bones[j].cof1x,
a * i - c * g,
c * d - a * f);
cof[2].xyz = vec3(
d * h - e * g,
b * g - a * h,
a * e - b * d);
return normalize(cof * n);
}
vec3 mulBoneVertex(vec3 v, uint i) {
highp mat4x3 m = transpose(bonesUniforms.bones[i].transform);
return v.x * m[0].xyz + (v.y * m[1].xyz + (v.z * m[2].xyz + m[3].xyz));
}
void skinPosition(inout vec3 p, const uvec4 ids, const vec4 weights) {
if (weights.w >= 0.0) {
p = weights.x * mulBoneVertex(p, uint(ids.x))
+ weights.y * mulBoneVertex(p, uint(ids.y))
+ weights.z * mulBoneVertex(p, uint(ids.z))
+ weights.w * mulBoneVertex(p, uint(ids.w));
return;
}
vec3 posSum = weights.x * mulBoneVertex(p, uint(ids.x));
posSum += weights.y * mulBoneVertex(p, uint(ids.y));
posSum += weights.z * mulBoneVertex(p, uint(ids.z));
uint pairIndex = uint(-weights.w - 1.);
uint pairStop = pairIndex + uint(ids.w - 3u);
for (uint i = pairIndex; i < pairStop; ++i) {
ivec2 texcoord = ivec2(i % MAX_SKINNING_BUFFER_WIDTH, i / MAX_SKINNING_BUFFER_WIDTH);
vec2 indexWeight = texelFetch(sampler1_indicesAndWeights, texcoord, 0).rg;
posSum += mulBoneVertex(p, uint(indexWeight.r)) * indexWeight.g;
}
p = posSum;
}
void skinNormal(inout vec3 n, const uvec4 ids, const vec4 weights) {
if (weights.w >= 0.0) {
n = weights.x * mulBoneNormal(n, uint(ids.x))
+ weights.y * mulBoneNormal(n, uint(ids.y))
+ weights.z * mulBoneNormal(n, uint(ids.z))
+ weights.w * mulBoneNormal(n, uint(ids.w));
return;
}
vec3 normSum = weights.x * mulBoneNormal(n, uint(ids.x));
normSum += weights.y * mulBoneNormal(n, uint(ids.y));
normSum += weights.z * mulBoneNormal(n, uint(ids.z));
uint pairIndex = uint(-weights.w - 1.);
uint pairStop = pairIndex + uint(ids.w - 3u);
for (uint i = pairIndex; i < pairStop; i = i + 1u) {
ivec2 texcoord = ivec2(i % MAX_SKINNING_BUFFER_WIDTH, i / MAX_SKINNING_BUFFER_WIDTH);
vec2 indexWeight = texelFetch(sampler1_indicesAndWeights, texcoord, 0).rg;
normSum += mulBoneNormal(n, uint(indexWeight.r)) * indexWeight.g;
}
n = normSum;
}
void skinNormalTangent(inout vec3 n, inout vec3 t, const uvec4 ids, const vec4 weights) {
if (weights.w >= 0.0) {
n = weights.x * mulBoneNormal(n, uint(ids.x))
+ weights.y * mulBoneNormal(n, uint(ids.y))
+ weights.z * mulBoneNormal(n, uint(ids.z))
+ weights.w * mulBoneNormal(n, uint(ids.w));
t = weights.x * mulBoneNormal(t, uint(ids.x))
+ weights.y * mulBoneNormal(t, uint(ids.y))
+ weights.z * mulBoneNormal(t, uint(ids.z))
+ weights.w * mulBoneNormal(t, uint(ids.w));
return;
}
vec3 normSum = weights.x * mulBoneNormal(n, uint(ids.x));
normSum += weights.y * mulBoneNormal(n, uint(ids.y)) ;
normSum += weights.z * mulBoneNormal(n, uint(ids.z));
vec3 tangSum = weights.x * mulBoneNormal(t, uint(ids.x));
tangSum += weights.y * mulBoneNormal(t, uint(ids.y));
tangSum += weights.z * mulBoneNormal(t, uint(ids.z));
uint pairIndex = uint(-weights.w - 1.);
uint pairStop = pairIndex + uint(ids.w - 3u);
for (uint i = pairIndex; i < pairStop; i = i + 1u) {
ivec2 texcoord = ivec2(i % MAX_SKINNING_BUFFER_WIDTH, i / MAX_SKINNING_BUFFER_WIDTH);
vec2 indexWeight = texelFetch(sampler1_indicesAndWeights, texcoord, 0).rg;
normSum += mulBoneNormal(n, uint(indexWeight.r)) * indexWeight.g;
tangSum += mulBoneNormal(t, uint(indexWeight.r)) * indexWeight.g;
}
n = normSum;
t = tangSum;
}
#define MAX_MORPH_TARGET_BUFFER_WIDTH 2048
void morphPosition(inout vec4 p) {
int index = getVertexIndex() + pushConstants.morphingBufferOffset;
ivec3 texcoord = ivec3(index % MAX_MORPH_TARGET_BUFFER_WIDTH, index / MAX_MORPH_TARGET_BUFFER_WIDTH, 0);
int c = object_uniforms_morphTargetCount;
for (int i = 0; i < c; ++i) {
float w = morphingUniforms.weights[i][0];
if (w != 0.0) {
texcoord.z = i;
p += w * texelFetch(sampler1_positions, texcoord, 0);
}
}
}
void morphNormal(inout vec3 n) {
vec3 baseNormal = n;
int index = getVertexIndex() + pushConstants.morphingBufferOffset;
ivec3 texcoord = ivec3(index % MAX_MORPH_TARGET_BUFFER_WIDTH, index / MAX_MORPH_TARGET_BUFFER_WIDTH, 0);
int c = object_uniforms_morphTargetCount;
for (int i = 0; i < c; ++i) {
float w = morphingUniforms.weights[i][0];
if (w != 0.0) {
texcoord.z = i;
ivec4 tangent = texelFetch(sampler1_tangents, texcoord, 0);
vec3 normal;
toTangentFrame(float4(tangent) * (1.0 / 32767.0), normal);
n += w * (normal - baseNormal);
}
}
}
#endif
vec4 getPosition() {
vec4 pos = mesh_position;
#if defined(VARIANT_HAS_SKINNING_OR_MORPHING)
if ((object_uniforms_flagsChannels & FILAMENT_OBJECT_MORPHING_ENABLED_BIT) != 0) {
#if defined(LEGACY_MORPHING)
pos += morphingUniforms.weights[0] * mesh_custom0;
pos += morphingUniforms.weights[1] * mesh_custom1;
pos += morphingUniforms.weights[2] * mesh_custom2;
pos += morphingUniforms.weights[3] * mesh_custom3;
#else
morphPosition(pos);
#endif
}
if ((object_uniforms_flagsChannels & FILAMENT_OBJECT_SKINNING_ENABLED_BIT) != 0) {
skinPosition(pos.xyz, mesh_bone_indices, mesh_bone_weights);
}
#endif
return pos;
}
#if defined(HAS_ATTRIBUTE_CUSTOM0)
vec4 getCustom0() { return mesh_custom0; }
#endif
#if defined(HAS_ATTRIBUTE_CUSTOM1)
vec4 getCustom1() { return mesh_custom1; }
#endif
#if defined(HAS_ATTRIBUTE_CUSTOM2)
vec4 getCustom2() { return mesh_custom2; }
#endif
#if defined(HAS_ATTRIBUTE_CUSTOM3)
vec4 getCustom3() { return mesh_custom3; }
#endif
#if defined(HAS_ATTRIBUTE_CUSTOM4)
vec4 getCustom4() { return mesh_custom4; }
#endif
#if defined(HAS_ATTRIBUTE_CUSTOM5)
vec4 getCustom5() { return mesh_custom5; }
#endif
#if defined(HAS_ATTRIBUTE_CUSTOM6)
vec4 getCustom6() { return mesh_custom6; }
#endif
#if defined(HAS_ATTRIBUTE_CUSTOM7)
vec4 getCustom7() { return mesh_custom7; }
#endif
vec4 computeWorldPosition() {
#if defined(VERTEX_DOMAIN_OBJECT)
mat4 transform = getWorldFromModelMatrix();
vec3 position = getPosition().xyz;
return mulMat4x4Float3(transform, position);
#elif defined(VERTEX_DOMAIN_WORLD)
return vec4(getPosition().xyz, 1.0);
#elif defined(VERTEX_DOMAIN_VIEW)
mat4 transform = getWorldFromViewMatrix();
vec3 position = getPosition().xyz;
return mulMat4x4Float3(transform, position);
#elif defined(VERTEX_DOMAIN_DEVICE)
mat4 transform = getWorldFromClipMatrix();
vec4 p = getPosition();
p.z = p.z * -0.5 + 0.5;
vec4 position = transform * p;
const highp float ALMOST_ZERO_FLT = 1.08420217249e-19;
if (abs(position.w) < ALMOST_ZERO_FLT) {
position.w = position.w < 0.0 ? -ALMOST_ZERO_FLT : ALMOST_ZERO_FLT;
}
return position * (1.0 / position.w);
#else
#error Unknown Vertex Domain
#endif
}
struct MaterialVertexInputs {
#ifdef HAS_ATTRIBUTE_COLOR
vec4 color;
#endif
#ifdef HAS_ATTRIBUTE_UV0
vec2 uv0;
#endif
#ifdef HAS_ATTRIBUTE_UV1
vec2 uv1;
#endif
#ifdef VARIABLE_CUSTOM0
vec4 VARIABLE_CUSTOM0;
#endif
#ifdef VARIABLE_CUSTOM1
vec4 VARIABLE_CUSTOM1;
#endif
#ifdef VARIABLE_CUSTOM2
vec4 VARIABLE_CUSTOM2;
#endif
#ifdef VARIABLE_CUSTOM3
vec4 VARIABLE_CUSTOM3;
#endif
#if defined(VARIABLE_CUSTOM4) && !defined(HAS_ATTRIBUTE_COLOR)
vec4 VARIABLE_CUSTOM4;
#endif
#ifdef HAS_ATTRIBUTE_TANGENTS
vec3 worldNormal;
#endif
vec4 worldPosition;
#ifdef VERTEX_DOMAIN_DEVICE
#ifdef MATERIAL_HAS_CLIP_SPACE_TRANSFORM
mat4 clipSpaceTransform;
#endif
#endif
};
vec4 getWorldPosition(const MaterialVertexInputs material) {
return material.worldPosition;
}
#ifdef VERTEX_DOMAIN_DEVICE
#ifdef MATERIAL_HAS_CLIP_SPACE_TRANSFORM
mat4 getMaterialClipSpaceTransform(const MaterialVertexInputs material) {
return material.clipSpaceTransform;
}
#endif
#endif
void initMaterialVertex(out MaterialVertexInputs material) {
#ifdef HAS_ATTRIBUTE_COLOR
material.color = mesh_color;
#endif
#ifdef HAS_ATTRIBUTE_UV0
#ifdef FLIP_UV_ATTRIBUTE
material.uv0 = vec2(mesh_uv0.x, 1.0 - mesh_uv0.y);
#else
material.uv0 = mesh_uv0;
#endif
#endif
#ifdef HAS_ATTRIBUTE_UV1
#ifdef FLIP_UV_ATTRIBUTE
material.uv1 = vec2(mesh_uv1.x, 1.0 - mesh_uv1.y);
#else
material.uv1 = mesh_uv1;
#endif
#endif
#ifdef VARIABLE_CUSTOM0
material.VARIABLE_CUSTOM0 = vec4(0.0);
#endif
#ifdef VARIABLE_CUSTOM1
material.VARIABLE_CUSTOM1 = vec4(0.0);
#endif
#ifdef VARIABLE_CUSTOM2
material.VARIABLE_CUSTOM2 = vec4(0.0);
#endif
#ifdef VARIABLE_CUSTOM3
material.VARIABLE_CUSTOM3 = vec4(0.0);
#endif
#if defined(VARIABLE_CUSTOM4) && !defined(HAS_ATTRIBUTE_COLOR)
material.VARIABLE_CUSTOM4 = vec4(0.0);
#endif
material.worldPosition = computeWorldPosition();
#ifdef VERTEX_DOMAIN_DEVICE
#ifdef MATERIAL_HAS_CLIP_SPACE_TRANSFORM
material.clipSpaceTransform = mat4(1.0);
#endif
#endif
}
#line 1
void materialVertex(inout MaterialVertexInputs m) {
}
#line 826

void main() {
#if defined(FILAMENT_HAS_FEATURE_INSTANCING)
#   if defined(TARGET_METAL_ENVIRONMENT) || defined(TARGET_VULKAN_ENVIRONMENT) || defined(TARGET_WEBGPU_ENVIRONMENT)
instance_index = gl_InstanceIndex;
#   else
if (CONFIG_POWER_VR_SHADER_WORKAROUNDS) {
instance_index = (1 + gl_InstanceID) - 1;
} else {
instance_index = gl_InstanceID;
}
#   endif
logical_instance_index = instance_index;
#endif
#if defined(VARIANT_HAS_STEREO) && defined(FILAMENT_STEREO_INSTANCED)
#if !defined(FILAMENT_HAS_FEATURE_INSTANCING)
#error Instanced stereo not supported at this feature level
#endif
logical_instance_index = instance_index / CONFIG_STEREO_EYE_COUNT;
#endif
initObjectUniforms();
#if defined(USE_OPTIMIZED_DEPTH_VERTEX_SHADER)
#if !defined(VERTEX_DOMAIN_DEVICE) || defined(VARIANT_HAS_VSM)
MaterialVertexInputs material;
initMaterialVertex(material);
materialVertex(material);
#endif
#else
MaterialVertexInputs material;
initMaterialVertex(material);
#if defined(HAS_ATTRIBUTE_TANGENTS)
#if defined(MATERIAL_NEEDS_TBN)
toTangentFrame(mesh_tangents, material.worldNormal, vertex_worldTangent.xyz);
#if defined(VARIANT_HAS_SKINNING_OR_MORPHING)
if ((object_uniforms_flagsChannels & FILAMENT_OBJECT_MORPHING_ENABLED_BIT) != 0) {
#if defined(LEGACY_MORPHING)
vec3 normal0, normal1, normal2, normal3;
toTangentFrame(mesh_custom4, normal0);
toTangentFrame(mesh_custom5, normal1);
toTangentFrame(mesh_custom6, normal2);
toTangentFrame(mesh_custom7, normal3);
vec3 baseNormal = material.worldNormal;
material.worldNormal += morphingUniforms.weights[0].xyz * (normal0 - baseNormal);
material.worldNormal += morphingUniforms.weights[1].xyz * (normal1 - baseNormal);
material.worldNormal += morphingUniforms.weights[2].xyz * (normal2 - baseNormal);
material.worldNormal += morphingUniforms.weights[3].xyz * (normal3 - baseNormal);
#else
morphNormal(material.worldNormal);
material.worldNormal = normalize(material.worldNormal);
#endif
}
if ((object_uniforms_flagsChannels & FILAMENT_OBJECT_SKINNING_ENABLED_BIT) != 0) {
skinNormalTangent(material.worldNormal, vertex_worldTangent.xyz, mesh_bone_indices, mesh_bone_weights);
}
#endif
vertex_worldTangent.xyz = getWorldFromModelNormalMatrix() * vertex_worldTangent.xyz;
vertex_worldTangent.w = mesh_tangents.w;
material.worldNormal = getWorldFromModelNormalMatrix() * material.worldNormal;
#else
toTangentFrame(mesh_tangents, material.worldNormal);
#if defined(VARIANT_HAS_SKINNING_OR_MORPHING)
if ((object_uniforms_flagsChannels & FILAMENT_OBJECT_MORPHING_ENABLED_BIT) != 0) {
#if defined(LEGACY_MORPHING)
vec3 normal0, normal1, normal2, normal3;
toTangentFrame(mesh_custom4, normal0);
toTangentFrame(mesh_custom5, normal1);
toTangentFrame(mesh_custom6, normal2);
toTangentFrame(mesh_custom7, normal3);
vec3 baseNormal = material.worldNormal;
material.worldNormal += morphingUniforms.weights[0].xyz * (normal0 - baseNormal);
material.worldNormal += morphingUniforms.weights[1].xyz * (normal1 - baseNormal);
material.worldNormal += morphingUniforms.weights[2].xyz * (normal2 - baseNormal);
material.worldNormal += morphingUniforms.weights[3].xyz * (normal3 - baseNormal);
#else
morphNormal(material.worldNormal);
material.worldNormal = normalize(material.worldNormal);
#endif
}
if ((object_uniforms_flagsChannels & FILAMENT_OBJECT_SKINNING_ENABLED_BIT) != 0) {
skinNormal(material.worldNormal, mesh_bone_indices, mesh_bone_weights);
}
#endif
material.worldNormal = getWorldFromModelNormalMatrix() * material.worldNormal;
#endif
#endif
materialVertex(material);
#if defined(HAS_ATTRIBUTE_COLOR)
vertex_color = material.color;
#endif
#if defined(HAS_ATTRIBUTE_UV0)
vertex_uv01.xy = material.uv0;
#endif
#if defined(HAS_ATTRIBUTE_UV1)
vertex_uv01.zw = material.uv1;
#endif
#if defined(VARIABLE_CUSTOM0)
VARIABLE_CUSTOM_AT0 = material.VARIABLE_CUSTOM0;
#endif
#if defined(VARIABLE_CUSTOM1)
VARIABLE_CUSTOM_AT1 = material.VARIABLE_CUSTOM1;
#endif
#if defined(VARIABLE_CUSTOM2)
VARIABLE_CUSTOM_AT2 = material.VARIABLE_CUSTOM2;
#endif
#if defined(VARIABLE_CUSTOM3)
VARIABLE_CUSTOM_AT3 = material.VARIABLE_CUSTOM3;
#endif
#if defined(VARIABLE_CUSTOM4) && !defined(HAS_ATTRIBUTE_COLOR)
VARIABLE_CUSTOM_AT4 = material.VARIABLE_CUSTOM4;
#endif
vertex_worldPosition.xyz = material.worldPosition.xyz;
#ifdef HAS_ATTRIBUTE_TANGENTS
vertex_worldNormal = material.worldNormal;
#endif
#if defined(VARIANT_HAS_SHADOWING) && defined(VARIANT_HAS_DIRECTIONAL_LIGHTING)
vertex_lightSpacePosition = computeLightSpacePosition(
vertex_worldPosition.xyz, vertex_worldNormal,
frameUniforms.lightDirection,
shadowUniforms.shadows[0].normalBias,
shadowUniforms.shadows[0].lightFromWorldMatrix);
#endif
#endif
vec4 position;
#if defined(VERTEX_DOMAIN_DEVICE)
position = getPosition();
#if !defined(USE_OPTIMIZED_DEPTH_VERTEX_SHADER)
#if defined(MATERIAL_HAS_CLIP_SPACE_TRANSFORM)
position = getMaterialClipSpaceTransform(material) * position;
#endif
#endif
#if defined(MATERIAL_HAS_VERTEX_DOMAIN_DEVICE_JITTERED)
position.xy = position.xy * frameUniforms.clipTransform.xy + (position.w * frameUniforms.clipTransform.zw);
#endif
#else
position = getClipFromWorldMatrix() * getWorldPosition(material);
#endif
#if defined(VERTEX_DOMAIN_DEVICE)
position.z = position.z * -0.5 + 0.5;
#endif
#if defined(VARIANT_HAS_VSM)
highp float z = (getViewFromWorldMatrix() * getWorldPosition(material)).z;
highp float depth = -z * frameUniforms.oneOverFarMinusNear - frameUniforms.nearOverFarMinusNear;
depth = depth * 2.0 - 1.0;
vertex_worldPosition.w = depth;
#endif
vertex_position = position;
#if defined(VARIANT_HAS_STEREO) && defined(FILAMENT_STEREO_INSTANCED)
int eyeIndex = instance_index % CONFIG_STEREO_EYE_COUNT;
float ndcViewportWidth = 2.0 / float(CONFIG_STEREO_EYE_COUNT);
float eyeZeroMidpoint = -1.0f + ndcViewportWidth / 2.0;
float transform = eyeZeroMidpoint + ndcViewportWidth * float(eyeIndex);
position.x *= 1.0 / float(CONFIG_STEREO_EYE_COUNT);
position.x += transform * position.w;
float leftClip  = position.x +
(1.0 - ndcViewportWidth * float(eyeIndex)) * position.w;
float rightClip = position.x +
(1.0 - ndcViewportWidth * float(eyeIndex + 1)) * position.w;
FILAMENT_CLIPDISTANCE[0] =  leftClip;
FILAMENT_CLIPDISTANCE[1] = -rightClip;
#endif
#if defined(TARGET_VULKAN_ENVIRONMENT)
position.y = -position.y;
#endif
#if !defined(TARGET_VULKAN_ENVIRONMENT) && !defined(TARGET_METAL_ENVIRONMENT) && !defined(TARGET_WEBGPU_ENVIRONMENT)
position.z = position.z * frameUniforms.clipControl.x + position.w * frameUniforms.clipControl.y;
#endif
gl_Position = position;
#if defined(VARIANT_HAS_STEREO) && defined(FILAMENT_STEREO_INSTANCED)
instance_index = logical_instance_index;
#endif
}

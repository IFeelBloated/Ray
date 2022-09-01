/**
 * @file SceneData.h
 *
 * Header file containing scene data structures.
 */

#ifndef __SCENE_DATA__
#define __SCENE_DATA__

#include <vector>
#include <string>

#include "glm/glm.hpp"

enum class LightType {
    LIGHT_POINT, LIGHT_DIRECTIONAL, LIGHT_SPOT, LIGHT_AREA
};

enum class PrimitiveType {
    PRIMITIVE_CUBE,
    PRIMITIVE_CONE,
    PRIMITIVE_CYLINDER,
    PRIMITIVE_TORUS,
    PRIMITIVE_SPHERE,
    PRIMITIVE_MESH
};

// Enumeration for types of transformations that can be applied to objects, lights, and cameras.
enum TransformationType {
    TRANSFORMATION_TRANSLATE, TRANSFORMATION_SCALE, TRANSFORMATION_ROTATE, TRANSFORMATION_MATRIX
};

template <typename Enumeration>
auto as_integer(Enumeration const value)
    -> typename std::underlying_type< Enumeration >::type
{
    return static_cast<typename std::underlying_type<Enumeration>::type>(value);
}

// Struct to store a RGBA color in floats [0,1]
using SceneColor = glm::vec4;

// Scene global color coefficients
struct SceneGlobalData  {
    float ka;  // global ambient coefficient
    float kd;  // global diffuse coefficient
    float ks;  // global specular coefficient
    float kt;  // global transparency coefficient
};

// Data for a single light
struct SceneLightData {
    int id;
    LightType type;

    SceneColor color;
    glm::vec3 function;  // Attenuation function

    glm::vec4 pos;       // Not applicable to directional lights
    glm::vec4 dir;       // Not applicable to point lights

    float penumbra;      // Only applicable to spot lights
    float angle;         // Only applicable to spot lights

    float width, height; // Only applicable to area lights
};

// Data for scene camera
struct SceneCameraData {
    glm::vec4 pos;
    glm::vec4 look;
    glm::vec4 up;

    float heightAngle;
    float aspectRatio;

    float aperture;      // Only applicable for depth of field
    float focalLength;   // Only applicable for depth of field
};

// Data for file maps (ie: texture maps)
struct SceneFileMap {
    SceneFileMap() : isUsed(false) {}

    bool isUsed;
    std::string filename;

    float repeatU;
    float repeatV;

    void clear() {
       isUsed = false;
       repeatU = 0.0f;
       repeatV = 0.0f;
       filename = std::string();
    }
};

// Data for scene materials
struct SceneMaterial {
   // This field specifies the diffuse color of the object. This is the color you need to use for
   // the object in sceneview. You can get away with ignoring the other color values until
   // intersect and ray.
   SceneColor cDiffuse;
   SceneColor cAmbient;
   SceneColor cReflective;
   SceneColor cSpecular;
   SceneColor cTransparent;
   SceneColor cEmissive;

   SceneFileMap textureMap;
   float blend;

   SceneFileMap bumpMap;

   float shininess;

   float ior; // index of refraction

   void clear() {
       cAmbient.r = 0.0f; cAmbient.g = 0.0f; cAmbient.b = 0.0f; cAmbient.a = 0.0f;
       cDiffuse.r = 0.0f; cDiffuse.g = 0.0f; cDiffuse.b = 0.0f; cDiffuse.a = 0.0f;
       cSpecular.r = 0.0f; cSpecular.g = 0.0f; cSpecular.b = 0.0f; cSpecular.a = 0.0f;
       cReflective.r = 0.0f; cReflective.g = 0.0f; cReflective.b = 0.0f; cReflective.a = 0.0f;
       cTransparent.r = 0.0f; cTransparent.g = 0.0f; cTransparent.b = 0.0f; cTransparent.a = 0.0f;
       cEmissive.r = 0.0f; cEmissive.g = 0.0f; cEmissive.b = 0.0f; cEmissive.a = 0.0f;
       textureMap.clear();
       bumpMap.clear();
       blend = 0.0f;
       shininess = 0.0f;
       ior = 0.0;
   }
};

struct ScenePrimitive {
   PrimitiveType type;
   std::string meshfile;     // Only applicable to meshes
   SceneMaterial material;
};

// Data for transforming a scene object. Aside from the TransformationType, the remaining of the
// data in the struct is mutually exclusive.
struct SceneTransformation {
    TransformationType type;

    // The translation vector. Only valid if transformation is a translation.
    glm::vec3 translate;
    // The scale vector. Only valid if transformation is a scale.
    glm::vec3 scale;
    // The axis of rotation. Only valid if the transformation is a rotation.
    glm::vec3 rotate;
    // The rotation angle in RADIANS. Only valid if transformation is a rotation.
    float angle;
    // The matrix for the transformation.
    // Only valid if the transformation is a custom matrix.
    glm::mat4x4 matrix;
};

// Structure for non-primitive scene objects
struct SceneNode {
   std::vector<SceneTransformation*> transformations;

   std::vector<ScenePrimitive*> primitives;

   std::vector<SceneNode*> children;
};

#endif


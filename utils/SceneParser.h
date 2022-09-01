#ifndef SCENEPARSER_H
#define SCENEPARSER_H

#include "SceneData.h"
#include <vector>
#include <string>

struct RenderShapeData {
    // The primitive description structure
    ScenePrimitive primitive;
    // The cumulative transformation matrix
    glm::mat4 ctm;
};

struct RenderData {
    SceneGlobalData globalData;
    SceneCameraData cameraData;

    std::vector<SceneLightData> lights;
    std::vector<RenderShapeData> shapes;
};

class SceneParser
{
public:
    // Parse the scene and store the results in oMetaData.
    // @param filepath The path of the scene file to load.
    // @param oMetaData On return, this will contain the metadata of the loaded scene.
    // @return A boolean value indicating whether the load is successful.
    static bool parse(std::string filepath, RenderData &oMetaData);
};

#endif // SCENEPARSER_H

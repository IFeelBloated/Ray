#include "SceneParser.h"
#include "ScenefileReader.h"
#include "glm/gtx/transform.hpp"

#include <iostream>
#include "../Infrastructure.hxx"

using namespace std;

auto DFSTraversal(auto& Shapes, auto&& ParentTransformation, auto Node)->void {
    auto FusedTransformation = ParentTransformation;
    for (auto x : Node->transformations)
        FusedTransformation *= [&] {
            if (x->type == TRANSFORMATION_TRANSLATE)
                return glm::translate(x->translate);
            else if (x->type == TRANSFORMATION_SCALE)
                return glm::scale(x->scale);
            else if (x->type == TRANSFORMATION_ROTATE)
                return glm::rotate(x->angle, x->rotate);
            else if (x->type == TRANSFORMATION_MATRIX)
                return x->matrix;
            throw std::runtime_error{ "Unrecognized transformation type detected!" };
        }();
    for (auto x : Node->primitives)
        Shapes.push_back({ .primitive = *x, .ctm = FusedTransformation });
    for (auto x : Node->children)
        DFSTraversal(Shapes, FusedTransformation, x);
}

bool SceneParser::parse(std::string filepath, RenderData &renderData) {
    shared_ptr<ScenefileReader> fileReader = make_shared<ScenefileReader>(filepath);
    bool success = fileReader->readXML();
    if (!success) {
        return false;
    }

    fileReader->getGlobalData(renderData.globalData);
    fileReader->getCameraData(renderData.cameraData);
    renderData.lights.resize(fileReader->getNumLights());
    for (auto x : Range{ fileReader->getNumLights() })
        fileReader->getLightData(x, renderData.lights[x]);
    DFSTraversal(renderData.shapes, glm::mat4{ 1 }, fileReader->getRootNode());

    return true;
}

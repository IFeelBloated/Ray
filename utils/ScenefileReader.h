#ifndef __ScenefileReader__
#define __ScenefileReader__

#include "SceneData.h"

#include <vector>
#include <map>

#include <QDomDocument>

/**
 * @class ScenefileReader
 *
 * This class parses the scene graph specified by the CS123 Xml file format.
 *
 * The parser is designed to replace the TinyXML parser that was in turn designed to replace the
 * Flex/Yacc/Bison parser.
 */

class ScenefileReader {

public:
    // Create a ScenefileReader, passing it the scene file.
    ScenefileReader(const std::string& filename);

    // Clean up all data for the scene
    ~ScenefileReader();

    // Parse the XML scene file. Returns false if scene is invalid.
    bool readXML();

    void getGlobalData(SceneGlobalData& data) const;

    void getCameraData(SceneCameraData& data) const;

    int getNumLights() const;

    // Returns the ith light data
    void getLightData(int i, SceneLightData& data) const;

    SceneNode* getRootNode() const;

private:
    // The filename should be contained within this parser implementation.
    // If you want to parse a new file, instantiate a different parser.
    bool parseGlobalData(const QDomElement &globaldata);
    bool parseCameraData(const QDomElement &cameradata);
    bool parseLightData(const QDomElement &lightdata);
    bool parseObjectData(const QDomElement &object);
    bool parseTransBlock(const QDomElement &transblock, SceneNode* node);
    bool parsePrimitive(const QDomElement &prim, SceneNode* node);

    std::string file_name;
    mutable std::map<std::string, SceneNode*> m_objects;
    SceneGlobalData m_globalData;
    SceneCameraData m_cameraData;
    std::vector<SceneLightData*> m_lights;
    std::vector<SceneNode*> m_nodes;
};

#endif


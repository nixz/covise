/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#ifndef VR_SCENE_GRAPH_H
#define VR_SCENE_GRAPH_H

/*! \file
 \brief  scene graph class

 \author Daniela Rainer
 \author (C) 1996
         Computer Centre University of Stuttgart,
         Allmandring 30,
         D-70550 Stuttgart,
         Germany

 \date   05.09.1996
 \date   10.07.1998 (Performer c++ interface)
 */

#include <util/common.h>
#include <OpenVRUI/osg/mathUtils.h>
#include <osg/Node>
#include <osg/Matrix>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/ref_ptr>
#include <osg/Multisample>
#include <osg/Material>
#include <osg/ClipNode>

namespace osg
{
class StateSet;
class ClipNode;
}

namespace opencover
{
class coVRLabel;
class buttonSpecCell;
class RenderObject;
class coVRStatsDisplay;

class COVEREXPORT VRSceneGraph
{
public:
    VRSceneGraph();
    virtual ~VRSceneGraph();
    static VRSceneGraph *instance();

    static void resetviewCallback(void *sceneGraph, buttonSpecCell *spec);
    static void viewallCallback(void *sceneGraph, buttonSpecCell *spec);
    static void coordAxisCallback(void *sceneGraph, buttonSpecCell *spec);
    static void storeCallback(void *sceneGraph, buttonSpecCell *spec);
    static void reloadFileCallback(void *sceneGraph, buttonSpecCell *spec);
    static void sliderCallback(void *sceneGraph, buttonSpecCell *spec);
    static void manipulateCallback(void *sceneGraph, buttonSpecCell *spec);
    static void scalePlusCallback(void *sceneGraph, buttonSpecCell *spec);
    static void scaleMinusCallback(void *sceneGraph, buttonSpecCell *spec);

    osg::MatrixTransform *loadAxisGeode(float scale);
    osg::Node *loadHandIcon(const char *name);
    osg::Node *loadHandLine();

    void addMenuItem(osg::Group *itemGroup);
    osg::Group *getMenuGroup()
    {
        return m_menuGroupNode.get();
    };
    void toggleMenu();
    void setMenu(bool state);
    void setMenuMode(bool state);
    void applyMenuModeToMenus();
    void manipulate(buttonSpecCell *spec);

    // rotate world
    int numFrames;
    float frameAngle;
    osg::Vec3 rotationAxis;
    osg::Vec3 rotationPoint;

    // process key events
    bool keyEvent(int type, int keySym, int mod);
    osg::Group *getScene()
    {
        return m_scene.get();
    };

    void config();
    void init();
    void update();

    osg::MatrixTransform *getTransform()
    {
        return (m_objectsTransform);
    }
    osg::MatrixTransform *getScaleTransform()
    {
        return (m_scaleTransform);
    }
    osg::MatrixTransform *getHandTransform()
    {
        return (m_handTransform.get());
    }
    osg::Vec3 getWorldPointOfInterest();
    void getHandWorldPosition(float *, float *, float *);
    void addPointerIcon(osg::Node *node);
    void removePointerIcon(osg::Node *node);

    osg::StateSet *loadDefaultGeostate(osg::Material::ColorMode mode = osg::Material::OFF);
    osg::StateSet *loadGlobalGeostate();
    osg::StateSet *loadUnlightedGeostate(osg::Material::ColorMode mode = osg::Material::OFF);
    osg::StateSet *loadTransparentGeostate(osg::Material::ColorMode mode = osg::Material::OFF);

    void setWireframe(bool wf);
    void setPointerType(int pointerType);
    int getPointerType()
    {
        return (m_pointerType);
    }

    osg::BoundingSphere getBoundingSphere();

    void addNode(osg::Node *node, osg::Group *parent, RenderObject *ro);
    void addNode(osg::Node *node, const char *parentName, RenderObject *ro);
    void deleteNode(const char *nodeName, bool isGroup);
    osg::Node *findNode(const std::string &name);
    void nodeHasSpecialBounds(osg::Node *node, const osg::BoundingSphere &bs);

    template <class T>
    T *findFirstNode(const char *name, bool startsWith = false, osg::Node * rootNode = NULL)
    {
        if (!rootNode)
            rootNode = m_objectsRoot;

        if (!rootNode)
            return NULL;

        if (!name)
            return NULL;

        if (!(startsWith ? strncmp(rootNode->getName().c_str(), name, strlen(name)) : strcmp(rootNode->getName().c_str(), name)))
        {
            T *node = dynamic_cast<T *>(rootNode);
            if (node)
                return node;
        }

        osg::Group *group = dynamic_cast<osg::Group *>(rootNode);
        if (group)
        {
            for (unsigned int i = 0; i < group->getNumChildren(); i++)
            {
                T *node = findFirstNode<T>(name, startsWith, group->getChild(i));
                if (node)
                {
                    return node;
                }
            }
            return NULL;
        }
        else
        {
            return NULL;
        }
    }

    // attach a node to another (the attached node will be deleted with the other node)
    void attachNode(const char *attacheeName, osg::Node *attached);
    void attachLabel(const char *attacheeName, const char *label);

    float scaleMode()
    {
        return m_scaleMode;
    }
    void setScaleMode(float scaleMode)
    {
        m_scaleMode = scaleMode;
    }
    float scaleFactor()
    {
        return m_scaleFactor;
    }
    void setScaleFactor(float scaleFactor, bool sync = true);
    void scaleAllObjects(bool resetView = false);
    bool isScalingAllObjects() const
    {
        return m_scalingAllObjects;
    }
    void boundingSphereToMatrices(const osg::BoundingSphere &boundingSphere,
                                  bool resetView, osg::Matrix *currentMatrix, float *scaleFactor) const;
    void adjustScale();

    void toggleAxis(bool state);
    void viewAll(bool resetView = false);
    float &joyStickX()
    {
        return m_joyStickX;
    }
    float &joyStickY()
    {
        return m_joyStickY;
    }
    float floorHeight()
    {
        return m_floorHeight;
    }
    osg::ClipNode *objectsRoot()
    {
        return m_objectsRoot;
    }

    osg::ref_ptr<osg::Node> getHandSphere()
    {
        return m_handSphere;
    }

    typedef std::map<std::string, osg::Node *> NodeList;
    typedef std::map<std::string, coVRLabel *> LabelList;
    void setMultisampling(osg::Multisample::Mode);

    void setScaleFromButton(float direction);
    void setScaleFactorButton(float f);
    void setScaleRestrictFactor(float min, float max);

    void setRestrictBox(float minX, float maxX, float minY, float maxY, float minZ, float maxZ);

    //Coloring
    void setColor(const char *nodeName, int *color, float transparency);
    void setColor(osg::Geode *geode, int *color, float transparency);
    void setTransparency(const char *nodeName, float transparency);
    void setTransparency(osg::Geode *geode, float transparency);
    void setShader(const char *nodeName, const char *shaderName, const char *paraFloat, const char *paraVec2, const char *paraVec3, const char *paraVec4, const char *paraInt, const char *paraBool, const char *paraMat2, const char *paraMat3, const char *paraMat4);
    void setShader(osg::Geode *geode, const char *shaderName, const char *paraFloat, const char *paraVec2, const char *paraVec3, const char *paraVec4, const char *paraInt, const char *paraBool, const char *paraMat2, const char *paraMat3, const char *paraMat4);
    void setMaterial(const char *nodeName, int *ambient, int *diffuse, int *specular, float shininess, float transparency);
    void setMaterial(osg::Geode *geode, int *ambient, int *diffuse, int *specular, float shininess, float transparency);

    // disables "store scenegraph"
    void protectScenegraph();

    int m_vectorInteractor; //< don't use - for COVISE plugin only
    bool m_oldHandLocked; //< don't use - for COVISE plugin only

    bool KeyButton[4];

private:
    int readConfigFile();
    void initAxis();
    void initHandDeviceGeometry();
    void initMatrices();
    void initSceneGraph();

#ifdef PHANTOM_TRACKER
    int m_forceFeedbackON;
    int m_forceFeedbackMode;
    float m_forceScale;
#endif

    osg::ref_ptr<osg::Group> m_scene;
    osg::ref_ptr<osg::MatrixTransform> m_handTransform;
    osg::ref_ptr<osg::MatrixTransform> m_handAxisTransform, m_viewerAxisTransform, m_smallSceneAxisTransform;
    osg::ref_ptr<osg::MatrixTransform> m_worldAxis, m_handAxis, m_viewerAxis, m_objectAxis, m_smallSceneAxis;
    osg::ref_ptr<osg::Group> m_menuGroupNode;
    osg::ref_ptr<osg::MatrixTransform> m_pointerDepthTransform;
    float m_pointerDepth;
    osg::ref_ptr<osg::Node> m_handPlane;
    osg::ref_ptr<osg::Node> m_handLine;
    osg::ref_ptr<osg::Node> m_handNormal;
    osg::ref_ptr<osg::Node> m_handCube;
    osg::ref_ptr<osg::Node> m_handFly;
    osg::ref_ptr<osg::Node> m_handDrive;
    osg::ref_ptr<osg::Node> m_handWalk;
    osg::ref_ptr<osg::Node> m_handPyramid;
    osg::ref_ptr<osg::Node> m_handProbe;
    osg::ref_ptr<osg::Node> m_handAnchor;
    osg::ref_ptr<osg::Node> m_handSphere;

    bool showSmallSceneAxis_;
    bool transparentPointer_;
    NodeList m_addedNodeList, m_attachedNodeList;
    LabelList m_attachedLabelList;

    osg::StateSet *m_rootStateSet;
    osg::ClipNode *m_objectsRoot;

    float m_floorHeight;
    bool m_handLocked; /* =true: no hand input is accepted until button is released */
    bool m_wireFrame; /* =true: scene is in wireframe mode */
    bool m_textured; /* =true: textures are drawn as intended */
    bool m_coordAxis; /* =true: coord Axis will be drawn */
    bool m_showMenu;

    osg::Matrix m_invBaseMatrix;
    osg::Matrix m_oldInvBaseMatrix;

    int m_pointerType;
    float m_joyStickX, m_joyStickY; //philip: allow access to anology x and y movement

    // do we use one input device only for world transformation?
    bool m_worldTransformer;
    // is transforming the world enabled?
    bool m_worldTransformerEnabled;

    // attribute SCALE attached to PerformerScene objects:
    // SCALE viewAll                        : scaleMode=1.0
    // SCALE keep                           : scaleMode=0.0
    // SCALE <pos number eg. 2.0 or 0.5>    : scaleMode=<number>
    // no SCALE attribute                   : scaleMode=-1.0
    float m_scaleMode, m_scaleFactor;
    float m_scaleFactorButton;
    float m_scaleRestrictFactorMin, m_scaleRestrictFactorMax;
    float m_transRestrictMinX, m_transRestrictMinY, m_transRestrictMinZ;
    float m_transRestrictMaxX, m_transRestrictMaxY, m_transRestrictMaxZ;
    bool m_scaleAllOn;
    bool m_scalingAllObjects;
    osg::MatrixTransform *m_scaleTransform, *m_handIconScaleTransform, *m_handAxisScaleTransform;
    float m_handIconSize;
    float m_handIconOffset;

    float wiiPos;

    osg::MatrixTransform *m_objectsTransform;
    osg::ref_ptr<osg::Multisample> m_Multisample;
    coVRStatsDisplay *statsDisplay;
    typedef std::map<osg::Node *, osg::Node *> SpecialBoundsNodeList;
    SpecialBoundsNodeList m_specialBoundsNodeList;
    void dirtySpecialBounds();

    bool menusAreHidden;
    osg::ref_ptr<osg::Program> emptyProgram_;

    osg::Vec3 transTraversingInteractors;
    bool isFirstTraversal;

    bool sgDebug_; /// scenegraph debug prints
    const char *hostName_;
    bool storeWithMenu;
    bool isScenegraphProtected_;

    typedef std::map<osg::Drawable *, osg::ref_ptr<osg::Material> > StoredMaterialsMap;
    StoredMaterialsMap storedMaterials;
    void storeMaterial(osg::Drawable *drawable);
};
}
#endif

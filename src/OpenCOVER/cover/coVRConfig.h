/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#ifndef CO_VR_CONFIG_H
#define CO_VR_CONFIG_H

/*! \file
 \brief  manage static and dynamic OpenCOVER configuration

 \author
 \author (C)
         Computer Centre University of Stuttgart,
         Allmandring 30,
         D-70550 Stuttgart,
         Germany

 \date
 */

#include <util/coExport.h>
#include <osg/Vec3>
#include <osg/ref_ptr>
#include <osg/Camera>
#include <osg/Multisample>
#include <osg/Texture2D>
#include <osgUtil/SceneView>
#include <string>

namespace osg
{
class DisplaySettings;
}

namespace osgViewer
{
class GraphicsWindow;
}

namespace opencover
{

//! describes a physical screen, such as one wall of a CAVE
struct screenStruct
{
    float hsize; // horizontal size in mm
    float vsize; // vertical size in mm
    osg::Vec3 xyz; // screen center in mm
    osg::Vec3 hpr; // screen orientation in degree euler angles
    std::string name;
    bool render;
    float lTan; // left, right, top bottom field of views, default/not set is -1
    float rTan;
    float tTan;
    float bTan;

    screenStruct()
    : hsize(0)
    , vsize(0)
    , xyz(0, 0, 0)
    , hpr(0, 0, 0)
    , name("UninitializedScreen")
    , render(false)
    , lTan(-1)
    , rTan(-1)
    , tTan(-1)
    , bTan(-1)
    {}
};

//! describes a render Channel which renders to a PBO or viewport
struct channelStruct
{
    std::string name;
    
    int PBONum; // destination PBO or -1 if rendering to a viewport directly
    int viewportNum; // destination viewport or -1 if rendering to a PBO
    int screenNum; // screen index

    osg::ref_ptr<osg::Camera> camera;
    osg::ref_ptr<osgUtil::SceneView> sceneView;
    osg::DisplaySettings *ds;
    int stereoMode;
    osg::Matrixd leftView, rightView;
    osg::Matrixd leftProj, rightProj;

    channelStruct()
    : name("UninitializedChannel")
    , PBONum(-1)
    , viewportNum(-1)
    , screenNum(-1)
    , ds(NULL)
    {}
};

//! describes a PBO
struct PBOStruct
{
    int PBOsx, PBOsy; // PBO size
    int windowNum; // pipe to render to
    osg::ref_ptr<osg::Texture2D> renderTargetTexture;

    PBOStruct()
    : PBOsx(-1)
    , PBOsy(-1)
    , windowNum(-1)
    {}
};


class COVEREXPORT angleStruct
{
public:
    int analogInput; // number of analog port at Cereal Box
    float cmin, cmax; // min and max values that analog port delivers
    float minangle, maxangle; // minimum and maximum angle in real world
    int screen; // screen number for these values
    float *value; // actual value (is set by VRPolhemusTracker::initCereal)
    int hpr; // which angle: hue, pitch or roll?
};

//! describes one window of the windowing system
struct windowStruct
{
    int ox, oy;
    int sx, sy;
    osgViewer::GraphicsWindow *window;
    int pipeNum;
    std::string name;
    bool decoration;
    bool resize;
    bool stereo;
    bool embedded;

    windowStruct()
    : ox(-1)
    , oy(-1)
    , sx(-1)
    , sy(-1)
    , window(NULL)
    , pipeNum(-1)
    , name("UninitializedWindow")
    , decoration(true)
    , resize(true)
    , stereo(false)
    , embedded(false)
    {}
};

struct viewportStruct // describes an OpenGL Viewport
{  
    int window;
    int PBOnum;
    float sourceXMin;
    float sourceYMin;
    float sourceXMax;
    float sourceYMax;

    float viewportXMin;
    float viewportYMin;
    float viewportXMax;
    float viewportYMax;
    
    std::string distortMeshName;
    std::string blendingTextureName;

    viewportStruct()
    : window(-1)
    , PBOnum(-1)
    , distortMeshName("NoDistortMesh")
    , blendingTextureName("NoBlendingTexture")
    {}
};

struct blendingTextureStruct // describes a blending Texture
{  
    int window;
    float viewportXMin;
    float viewportYMin;
    float viewportXMax;
    float viewportYMax;
    
    std::string blendingTextureName;
    
    blendingTextureStruct()
    : window(-1)
    , blendingTextureName("UninitialzedBlendingTexture")
    {}
};

//! describes what is responsible for rendering the  window
struct pipeStruct
{
    int x11DisplayNum;
    int x11ScreenNum;
    //const char *display;

    pipeStruct()
    : x11DisplayNum(-1)
    , x11ScreenNum(-1)
    {}
};

class COVEREXPORT coVRConfig
{
    friend class coVRPluginSupport;
    friend class VRWindow;
    friend class VRSceneGraph;
    friend class OpenCOVER;
    friend class VRViewer;

public:
    static coVRConfig *instance();

    enum MonoViews
    {
        MONO_MIDDLE,
        MONO_LEFT,
        MONO_RIGHT,
        MONO_NONE
    };

    // mono rendering mode
    MonoViews monoView() const;

    enum
    {
        FIXED_TO_VIEWER,
        FIXED_TO_OBJROOT,
        FIXED_TO_VIEWER_FRONT,
        FIXED_TO_OBJROOT_FRONT
    } envMapModes;
    /*
            //my
            enum MarkNodes
            {
               MOVE,
               SHOWHIDE,
               SELECTION,
               ANNOTATION
            };
      */
    
    int numScreens() const;
    int numChannels() const;
    int numPBOs() const;
    int numViewports() const;
    int numBlendingTextures() const;
    int numWindows() const;
    int numPipes() const;
    int lockToCPU() const;

    // get the scene size defined in covise.config
    float getSceneSize() const;

    int stereoMode() const;
    int parseStereoMode(const char *modeName);
    // have all the screens the same orientation?
    bool haveFlatDisplay() const;

    // if mouse needs to be checked
    bool mouseNav() const;

    // if mouse needs to be checked
    bool mouseTracking() const;

    // if WiiMote needs to be checked
    bool useWiiMote() const;
    // wii navigation
    bool useWiiNavigationVisenso() const;

    // menu mode on
    bool isMenuModeOn() const;
    // color scene in menu mode
    bool colorSceneInMenuMode() const;

    // optional Collaborative configuration file (-c option)
    char *collaborativeOptionsFile;

    // optional Collaborative configuration file (-c option)
    char *viewpointsFile;

    // returns true if level <= debugLevel
    // debug levels should be used like this
    // 0 no output
    // 1 covise.config entries, coVRInit
    // 2 constructors, destructors
    // 3 all functions which are not called continously
    // 4
    // 5 all functions which are called continously
    bool debugLevel(int level) const;

    // return true, if OpenCOVER is configured for stereo rendering
    bool stereoState() const;

    float stereoSeparation() const;

    // get number of requested stencil bits (default = 1)
    int numStencilBits() const;

    // current configured angle of workbench
    float worldAngle() const;

    //! return true if the position of the tracked head is not taken into account
    bool frozen() const;

    //! true: stop head tracking, false: enable head tracking
    void setFrozen(bool state);

    //! return true if projection is orthographic
    bool orthographic() const;

    //! true: projection is orthographic
    void setOrthographic(bool state);

    //! return true, if configured for display lists
    bool useDisplayLists() const;

    //! return true, if configured for vertex buffer objects
    bool useVBOs() const;

    //! distance of near clipping plane
    float nearClip() const;

    //! distance of far clipping plane
    float farClip() const;

    //! scale factor for level of detail computation
    float getLODScale() const;
    void setLODScale(float);

    bool doMultisample()
    {
        return multisample;
    }
    bool getMultisampleInvert()
    {
        return multisampleInvert;
    }
    float getMultisampleCoverage()
    {
        return multisampleCoverage;
    }
    int getMultisampleSamples()
    {
        return multisampleSamples;
    }
    int getMultisampleSampleBuffers()
    {
        return multisampleSampleBuffers;
    }
    osg::Multisample::Mode getMultisampleMode()
    {
        return multisampleMode;
    }

    bool stencil() const;

    void setFrameRate(float fr);
    float frameRate() const;
    
    std::vector<screenStruct> screens; // list of physical screens
    std::vector<channelStruct> channels; // list of physical screens
    std::vector<PBOStruct> PBOs; // list of physical screens
    std::vector<pipeStruct> pipes; // list of pipes (X11: identified by display and screen)
    std::vector<windowStruct> windows; // list of windows
    std::vector<viewportStruct> viewports; // list of PixelBufferObjects
    std::vector<blendingTextureStruct> blendingTextures; // list of blendingTextures

    bool useDisplayVariable() const
    {
        return m_useDISPLAY;
    }

    bool restrictOn() const
    {
        return m_restrict;
    }

    void setNearFar(float nearC, float farC)
    {
        if (nearC > 0 && farC > 0)
        {
            m_farClip = farC;
            m_nearClip = nearC;
        }
    }

    int getLanguage() const
    {
        return m_language;
    }
    enum Languages
    {
        ENGLISH,
        GERMAN
    };
    float HMDViewingAngle;
    float HMDDistance;
    std::string glVersion;

private:
    coVRConfig();
    ~coVRConfig();

    // configuration entries

    bool m_useDisplayLists;
    bool m_useVBOs;

    bool m_useDISPLAY;
#if 0
    int m_numWindows;
    int m_numViewports;
    int m_numBlendingTextures;
    int m_numScreens;
    int m_numChannels;
    int m_numPBOs;
    int m_numPipes;
#endif
    int m_stencilBits;
    float m_sceneSize;

    float m_nearClip;
    float m_farClip;
    float m_LODScale;
    float m_worldAngle;

    bool m_stereoState;
    int m_stereoMode;
    float m_stereoSeparation;
    MonoViews m_monoView; // MONO_MIDDLE MONO_LEFT MONO_RIGHT
    int m_envMapMode;
    bool m_freeze;
    bool m_passiveStereo;
    bool m_orthographic;
    bool m_stencil; //< user stencil buffers
    bool HMDMode;

    bool trackedHMD;

    bool drawStatistics;

    // multi-sampling
    int multisampleSamples;
    int multisampleSampleBuffers;
    float multisampleCoverage;
    bool multisampleInvert;
    bool multisample;
    osg::Multisample::Mode multisampleMode;

    int m_dLevel; // debugLevel
    bool m_mouseNav;
    bool m_useWiiMote;
    bool m_useWiiNavVisenso;

    bool m_flatDisplay;
    bool m_doRender;

    bool constantFrameRate;
    float constFrameTime;

    bool m_restrict;

    int m_language;

    bool m_menuModeOn;
    bool m_coloringSceneInMenuMode;
    int m_lockToCPU;
};
}
#endif

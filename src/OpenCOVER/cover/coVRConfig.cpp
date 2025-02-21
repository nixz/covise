/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#include <iostream>
#include <config/CoviseConfig.h>
#include "coVRNavigationManager.h"
#include "coCoverConfig.h"
#include "coVRConfig.h"
#include "input/input.h"

using std::cerr;
using std::endl;
using namespace covise;
using namespace opencover;

coVRConfig *coVRConfig::instance()
{
    static coVRConfig *singleton = NULL;
    if (!singleton)
        singleton = new coVRConfig;
    return singleton;
}

float coVRConfig::getSceneSize() const
{
    return m_sceneSize;
}

int coVRConfig::parseStereoMode(const char *modeName)
{

    int stereoMode = osg::DisplaySettings::ANAGLYPHIC;
    if (modeName)
    {
        if (strcasecmp(modeName, "ANAGLYPHIC") == 0)
            stereoMode = osg::DisplaySettings::ANAGLYPHIC;
        else if (strcasecmp(modeName, "QUAD_BUFFER") == 0)
            stereoMode = osg::DisplaySettings::QUAD_BUFFER;
        else if (strcasecmp(modeName, "HORIZONTAL_SPLIT") == 0)
            stereoMode = osg::DisplaySettings::HORIZONTAL_SPLIT;
        else if (strcasecmp(modeName, "VERTICAL_SPLIT") == 0)
            stereoMode = osg::DisplaySettings::VERTICAL_SPLIT;
        else if (strcasecmp(modeName, "RIGHT_EYE") == 0)
            stereoMode = osg::DisplaySettings::RIGHT_EYE;
        else if (strcasecmp(modeName, "RIGHT") == 0)
            stereoMode = osg::DisplaySettings::RIGHT_EYE;
        else if (strcasecmp(modeName, "LEFT") == 0)
            stereoMode = osg::DisplaySettings::LEFT_EYE;
        else if (strcasecmp(modeName, "LEFT_EYE") == 0)
            stereoMode = osg::DisplaySettings::LEFT_EYE;
        else if (strcasecmp(modeName, "STIPPLE") == 0)
            stereoMode = osg::DisplaySettings::VERTICAL_INTERLACE;
        else if (strcasecmp(modeName, "VERTICAL_INTERLACE") == 0)
            stereoMode = osg::DisplaySettings::VERTICAL_INTERLACE;
        else if (strcasecmp(modeName, "HORIZONTAL_INTERLACE") == 0)
            stereoMode = osg::DisplaySettings::HORIZONTAL_INTERLACE;
        else if (strcasecmp(modeName, "CHECKERBOARD") == 0)
            stereoMode = osg::DisplaySettings::CHECKERBOARD;
        else if (strcasecmp(modeName, "MONO") == 0)
        {
            stereoMode = osg::DisplaySettings::LEFT_EYE;
            m_stereoState = false;
            m_stereoSeparation = 0.f;
        }
        else if (strcasecmp(modeName, "NONE") == 0)
            stereoMode = osg::DisplaySettings::ANAGLYPHIC;
        else if (modeName[0] == '\0')
            stereoMode = osg::DisplaySettings::ANAGLYPHIC;
        else
            cerr << "Unknown stereo mode \"" << modeName << "\"" << endl;
    }
    return stereoMode;
}

coVRConfig::coVRConfig()
    : m_useDISPLAY(false)
    , m_orthographic(false)
    , m_mouseNav(true)
    , m_useWiiMote(false)
    , m_useWiiNavVisenso(false)
    , m_flatDisplay(false)
{
    /// path for the viewpoint file: initialized by 1st param() call

    m_dLevel = coCoviseConfig::getInt("COVER.DebugLevel", 0);
    
    collaborativeOptionsFile = NULL;
    viewpointsFile = NULL;
    int hsize, vsize, x, y, z;
    m_passiveStereo = false;

    m_mouseNav = coCoviseConfig::isOn("COVER.Input.MouseNav", m_mouseNav);

    constFrameTime = 0.1;
    constantFrameRate = false;
    float frameRate = coCoviseConfig::getInt("COVER.ConstantFrameRate", 0);
    if (frameRate > 0)
    {
        constantFrameRate = true;
        constFrameTime = 1.0f / frameRate;
    }
    m_lockToCPU = coCoviseConfig::getInt("COVER.LockToCPU", -1);
    m_freeze = coCoviseConfig::isOn("COVER.Freeze", true);
    m_sceneSize = coCoviseConfig::getFloat("COVER.SceneSize", 2000.0);
    m_farClip = coCoviseConfig::getFloat("COVER.Far", 10000000);
    m_nearClip = coCoviseConfig::getFloat("COVER.Near", 10.0f);
    const int numScreens = coCoviseConfig::getInt("COVER.NumScreens", 1);
    if (numScreens < 1)
    {
	std::cerr << "COVER.NumScreens cannot be < 1" << std::endl;
	exit(1);
    }
    if (numScreens > 50)
    {
	std::cerr << "COVER.NumScreens cannot be > 50" << std::endl;
	exit(1);
    }
    screens.resize(numScreens);

    const int numChannels = coCoviseConfig::getInt("COVER.NumChannels", numScreens); // normally numChannels == numScreens, only if we use PBOs, it might be equal to the number of PBOs
    if (numChannels < numScreens)
    {
	std::cerr << "COVER.NumChannels cannot be < COVER.NumScreens" << std::endl;
	exit(1);
    }
    channels.resize(numChannels);
    
    const int numWindows = coCoviseConfig::getInt("COVER.NumWindows", numScreens);
    if (numWindows < 1)
    {
	std::cerr << "COVER.NumWindows cannot be < 1" << std::endl;
	exit(1);
    }
    if (numWindows > 50)
    {
	std::cerr << "COVER.NumWindows cannot be > 50" << std::endl;
	exit(1);
    }
    windows.resize(numWindows);

    const int numViewports = coCoviseConfig::getInt("COVER.NumViewports", numChannels); // normally this is equal to the number of Channels
    if (numViewports < 0)
    {
	std::cerr << "COVER.NumViewports cannot be negative" << std::endl;
	exit(1);
    }
    viewports.resize(numViewports);

    const int numBlendingTextures = coCoviseConfig::getInt("COVER.NumBlendingTextures", 0); 
    if (numBlendingTextures < 0)
    {
	std::cerr << "COVER.NumBlendingTextures cannot be negative" << std::endl;
	exit(1);
    }
    blendingTextures.resize(numBlendingTextures);

    const int numPBOs = coCoviseConfig::getInt("COVER.NumPBOs", numChannels);
    if (numPBOs < 0)
    {
	std::cerr << "COVER.NumPBOs cannot be negative" << std::endl;
	exit(1);
    }
    PBOs.resize(numPBOs);

    const int numPipes = coCoviseConfig::getInt("COVER.NumPipes", 1);
    if (numPipes < 1)
    {
	std::cerr << "COVER.NumPipes cannot be < 1" << std::endl;
	exit(1);
    }
    if (numPipes > 50)
    {
	std::cerr << "COVER.NumPipes cannot be > 50" << std::endl;
	exit(1);
    }
    pipes.resize(numPipes);

    m_stencil = coCoviseConfig::isOn("COVER.Stencil", true);
    glVersion = coCoviseConfig::getEntry("COVER.GLVersion", "");
    m_stencilBits = coCoviseConfig::getInt("COVER.StencilBits", 1);
    m_stereoSeparation = 64.0f;
    string line = coCoviseConfig::getEntry("separation", "COVER.Stereo");
    if (!line.empty())
    {
        if (strncmp(line.c_str(), "AUTO", 4) == 0)
        {
            m_stereoSeparation = 1000;
        }
        else
        {
            if (sscanf(line.c_str(), "%f", &m_stereoSeparation) != 1)
            {
                cerr << "CoVRConfig sscanf failed stereosep" << endl;
            }
        }
    }
    m_stereoState = coCoviseConfig::isOn("COVER.Stereo", false);
    if (!m_stereoState)
        m_stereoSeparation = 0.f;

    m_monoView = MONO_MIDDLE;

    m_useDisplayLists = coCoviseConfig::isOn("COVER.UseDisplayLists", false);
    m_useVBOs = coCoviseConfig::isOn("COVER.UseVertexBufferObjects", !m_useDisplayLists);

    multisample = coCoviseConfig::isOn("COVER.Multisample", false);
    multisampleInvert = coCoviseConfig::isOn(std::string("invert"), std::string("COVER.Multisample"), false);
    multisampleSamples = coCoviseConfig::getInt("numSamples", "COVER.Multisample", 2);
    multisampleSampleBuffers = coCoviseConfig::getInt("numBuffers", "COVER.Multisample", 2);
    multisampleCoverage = coCoviseConfig::getFloat("sampleCoverage", "COVER.Multisample", 1.0);

    std::string msMode = coCoviseConfig::getEntry("mode", "COVER.Multisample", "FASTEST");

    std::string lang = coCoviseConfig::getEntry("value", "COVER.Menu.Language", "ENGLISH");
    if (lang == "GERMAN")
        m_language = GERMAN;
    else
        m_language = ENGLISH;

    m_restrict = coCoviseConfig::isOn("COVER.Restrict", false);
    if (msMode == "FASTEST")
    {
        multisampleMode = osg::Multisample::FASTEST;
    }
    else if (msMode == "NICEST")
    {
        multisampleMode = osg::Multisample::NICEST;
    }
    else
    {
        multisampleMode = osg::Multisample::DONT_CARE;
    }

    m_useWiiMote = coCoviseConfig::isOn("COVER.Input.WiiMote", false);
    m_useWiiNavVisenso = coCoviseConfig::isOn("COVER.Input.WiiNavigationVisenso", false);
    m_menuModeOn = coCoviseConfig::isOn("COVER.MenuMode", false);
    m_coloringSceneInMenuMode = coCoviseConfig::isOn("COVER.MenuMode.Coloring", true);

    std::string entry = coCoviseConfig::getEntry("COVER.MonoView");
    if (!entry.empty())
    {
        if (strcasecmp(entry.c_str(), "LEFT") == 0)
            m_monoView = MONO_LEFT;
        if (strcasecmp(entry.c_str(), "RIGHT") == 0)
            m_monoView = MONO_RIGHT;
        if (strcasecmp(entry.c_str(), "NONE") == 0)
            m_monoView = MONO_NONE;
    }
    entry = coCoviseConfig::getEntry("COVER.StereoMode");
    m_stereoMode = parseStereoMode(entry.c_str());

    m_envMapMode = FIXED_TO_VIEWER;
    entry = coCoviseConfig::getEntry("COVER.EnvMapMode");
    if (!entry.empty())
    {
        if (strcasecmp(entry.c_str(), "fixedToViewer") == 0)
            m_envMapMode = FIXED_TO_VIEWER;
        if (strcasecmp(entry.c_str(), "fixedToObjectsRoot") == 0)
            m_envMapMode = FIXED_TO_OBJROOT;
        if (strcasecmp(entry.c_str(), "fixedToViewerFront") == 0)
            m_envMapMode = FIXED_TO_VIEWER_FRONT;
        if (strcasecmp(entry.c_str(), "fixedToObjectsRootFront") == 0)
            m_envMapMode = FIXED_TO_OBJROOT_FRONT;
    }

    m_LODScale = coCoviseConfig::getFloat("COVER.LODScale", 1.0);
    m_worldAngle = coCoviseConfig::getFloat("COVER.WorldAngle", 0.);

    drawStatistics = coCoviseConfig::isOn("COVER.Statistics", false);
    HMDMode = coCoviseConfig::isOn("mode", std::string("COVER.HMD"), false);
    HMDViewingAngle = coCoviseConfig::getFloat("angle", "COVER.HMD", 60.0f);

    // tracked HMD
    HMDDistance = coCoviseConfig::getFloat("distance", "COVER.HMD", 0.0f);
    trackedHMD = coCoviseConfig::isOn("tracked", std::string("COVER.HMD"), false);

    if (debugLevel(2))
        fprintf(stderr, "\nnew coVRConfig\n");

    //bool isMaster = coVRMSController::instance()->isMaster();

    windows[0].window = NULL;

    m_passiveStereo = false;
    m_flatDisplay = true;
    for (size_t i = 0; i < screens.size(); i++)
    {

        float h, p, r;
        
        char str[200];
        sprintf(str, "COVER.ScreenConfig.Screen:%d", (int)i);
        bool state = coCoverConfig::getScreenConfigEntry(i, screens[i].name, &hsize, &vsize, &x, &y, &z, &h, &p, &r);
        if (!state)
        {
            cerr << "Exiting because of erroneous ScreenConfig entry." << endl;
            exit(-1);
        }
        else
        {
            
            screens[i].render = coCoviseConfig::isOn("render", str, true);
            screens[i].hsize = (float)hsize;
            screens[i].vsize = (float)vsize;
            screens[i].xyz.set((float)x, (float)y, (float)z);
            screens[i].hpr.set(h, p, r);
        }
        if ((i == 1) && (screens[0].name == screens[1].name))
        {
            m_passiveStereo = true;
        }

        // this check is too simple
        if (screens[i].hpr != screens[0].hpr)
        {
            m_flatDisplay = false;
        }
        screens[i].lTan = -1;
        screens[i].rTan = -1;
        screens[i].tTan = -1;
        screens[i].bTan = -1; // left, right, top bottom field of views, default/not set is -1
    }

    for (size_t i = 0; i < pipes.size(); i++)
    {
        char str[200];
        sprintf(str, "COVER.PipeConfig.Pipe:%d", (int)i);
        pipes[i].x11DisplayNum = coCoviseConfig::getInt("server", str, 0);
        pipes[i].x11ScreenNum = coCoviseConfig::getInt("screen", str, 0);
    }

    for (size_t i = 0; i < windows.size(); i++)
    {
        bool state = coCoverConfig::getWindowConfigEntry(i, windows[i].name,
                                                         &windows[i].pipeNum, &windows[i].ox, &windows[i].oy,
                                                         &windows[i].sx, &windows[i].sy, &windows[i].decoration,
                                                         &windows[i].stereo, &windows[i].resize, &windows[i].embedded);
        if (!state)
        {
            cerr << "Exit because of erroneous WindowConfig entry." << endl;
            exit(-1);
        }
    }

    for (size_t i = 0; i < channels.size(); i++)
    {
        std::string stereoM;

        char str[200];
        sprintf(str, "COVER.ChannelConfig.Channel:%d", (int)i);
        std::string s = coCoviseConfig::getEntry("comment", str, "NoNameChannel");
        channels[i].name = s;
        stereoM = coCoviseConfig::getEntry("stereoMode", str);
        if (!stereoM.empty())
        {
            channels[i].stereoMode = parseStereoMode(stereoM.c_str());
        }
        else
        {
            if (m_passiveStereo)
            {
                if (i % 2)
                {
                    channels[i].stereoMode = osg::DisplaySettings::RIGHT_EYE;
                }
                else
                {
                    channels[i].stereoMode = osg::DisplaySettings::LEFT_EYE;
                }
            }
            else
            {
                channels[i].stereoMode = m_stereoMode;
            }
        }

        if (channels[i].stereoMode == osg::DisplaySettings::VERTICAL_INTERLACE || channels[i].stereoMode == osg::DisplaySettings::HORIZONTAL_INTERLACE || channels[i].stereoMode == osg::DisplaySettings::CHECKERBOARD)
        {
            m_stencil = true;
        }
        
        channels[i].PBONum = coCoviseConfig::getInt("PBOIndex", str, -1);
        if(channels[i].PBONum == -1)
        {
            channels[i].viewportNum = coCoviseConfig::getInt("viewportIndex", str, i);
        }
        else
        {
            channels[i].viewportNum = -1;
        }
        channels[i].screenNum = coCoviseConfig::getInt("screenIndex", str, i);
        if (channels[i].screenNum >= screens.size())
        {
            std::cerr << "screenIndex " << channels[i].screenNum << " for channel " << i << " out of range (max: " << screens.size()-1 << ")" << std::endl;
            exit(1);
        }
        
    }
    for (size_t i = 0; i < PBOs.size(); i++)
    {
        std::string stereoM;

        char str[200];
        sprintf(str, "COVER.PBOConfig.PBO:%d", (int)i);
        
        PBOs[i].PBOsx = coCoviseConfig::getInt("PBOSizeX", str, -1);
        PBOs[i].PBOsy = coCoviseConfig::getInt("PBOSizeY", str, -1);
        PBOs[i].windowNum = coCoviseConfig::getInt("windowIndex", str, -1);
    }
    for (size_t i = 0; i < viewports.size(); i++)
    {
        std::string stereoM;

        char str[200];
        sprintf(str, "COVER.ViewportConfig.Viewport:%d", (int)i);
        viewportStruct &vp = viewports[i];
        bool exists=false;
        vp.window = coCoviseConfig::getInt("windowIndex", str, -1,&exists);
        if(!exists)
        {
            // no viewport config, check for values in channelConfig for backward compatibility
            
            sprintf(str, "COVER.ChannelConfig.Channel:%d", (int)i);
            vp.window = coCoviseConfig::getInt("windowIndex", str, -1,&exists);
            if (!exists)
            {
                std::cerr << "no ChannelConfig for channel " << i << std::endl;
                exit(1);
            }
            vp.PBOnum = i;
        }
        else
        {
            vp.PBOnum = coCoviseConfig::getInt("PBOIndex", str, -1);
        }


        vp.viewportXMin = coCoviseConfig::getFloat("left", str, 0);
        vp.viewportYMin = coCoviseConfig::getFloat("bottom", str, 0);

	if (vp.window >= 0)
        {
            if (vp.viewportXMin > 1.0)
            {
                vp.viewportXMin = vp.viewportXMin / ((float)(windows[vp.window].sx));
            }
            if (vp.viewportYMin > 1.0)
            {
                vp.viewportYMin = vp.viewportYMin / ((float)(windows[vp.window].sy));
            }
        }

        vp.viewportXMax = coCoviseConfig::getFloat("right", str, -1);
        vp.viewportYMax = coCoviseConfig::getFloat("top", str, -1);
        if (vp.viewportXMax < 0)
        {
            float w,h;
            w = coCoviseConfig::getFloat("width", str, 1024);
            h = coCoviseConfig::getFloat("height", str, 768);
            if (vp.window >= 0)
            {
                if (w > 1.0)
                    vp.viewportXMax = vp.viewportXMin + (w / ((float)(windows[vp.window].sx)));
                else
                    vp.viewportXMax = vp.viewportXMin + w;
                if (h > 1.0)
                    vp.viewportYMax = vp.viewportYMin + (h / ((float)(windows[vp.window].sy)));
                else
                    vp.viewportYMax = vp.viewportYMin + h;
            }
        }
        else
        {
            if (vp.window >= 0)
            {
                if (vp.viewportXMax > 1.0)
                    vp.viewportXMax = vp.viewportXMax / ((float)(windows[vp.window].sx));
                if (vp.viewportYMax > 1.0)
                    vp.viewportYMax = vp.viewportYMax / ((float)(windows[vp.window].sy));
            }
        }

        vp.sourceXMin = coCoviseConfig::getFloat("sourceLeft", str, 0);
        vp.sourceYMin = coCoviseConfig::getFloat("sourceBottom", str, 0);

        if (vp.PBOnum >= 0)
        {
            if (vp.sourceXMin > 1.0)
            {
                vp.sourceXMin = vp.sourceXMin / ((float)(PBOs[vp.PBOnum].PBOsx));
            }
            if (vp.sourceYMin > 1.0)
            {
                vp.sourceYMin = vp.sourceYMin / ((float)(PBOs[vp.PBOnum].PBOsy));
            }
        }

        vp.sourceXMax = coCoviseConfig::getFloat("sourceRight", str, -1);
        vp.sourceYMax = coCoviseConfig::getFloat("sourceTop", str, -1);
        if (vp.sourceXMax < 0)
        {
            float w,h;
            w = coCoviseConfig::getFloat("sourceWidth", str, 1.0);
            h = coCoviseConfig::getFloat("sourceHeight", str, 1.0);
            if (vp.PBOnum >= 0)
            {
                if (w > 1.0)
                    vp.sourceXMax = vp.sourceXMin + (w / ((float)(PBOs[vp.PBOnum].PBOsx)));
                else
                    vp.sourceXMax = vp.sourceXMin + w;
                if (h > 1.0)
                    vp.sourceYMax = vp.sourceYMin + (h / ((float)(PBOs[vp.PBOnum].PBOsy)));
                else
                    vp.sourceYMax = vp.sourceYMin + h;
            }
        }
        else
        {
            if (vp.PBOnum >= 0)
            {
                if (vp.sourceXMax > 1.0)
                    vp.sourceXMax = vp.sourceXMax / ((float)(PBOs[vp.PBOnum].PBOsx));
                if (vp.sourceYMax > 1.0)
                    vp.sourceYMax = vp.sourceYMax / ((float)(PBOs[vp.PBOnum].PBOsy));
            }
        }
        
        vp.distortMeshName = coCoviseConfig::getEntry("distortMesh", str, "");
        vp.blendingTextureName = coCoviseConfig::getEntry("blendingTexture", str, "");

    }
    
    for (size_t i = 0; i < blendingTextures.size(); i++)
    {
        char str[200];
        sprintf(str, "COVER.BlendingTextureConfig.BlendingTexture:%d", (int)i);
        blendingTextureStruct &bt = blendingTextures[i];
        bool exists=false;
        bt.window = coCoviseConfig::getInt("windowIndex", str, -1,&exists);
        
        bt.viewportXMin = coCoviseConfig::getFloat("left", str, 0);
        bt.viewportYMin = coCoviseConfig::getFloat("bottom", str, 0);

        if (bt.viewportXMin > 1.0)
        {
            bt.viewportXMin = bt.viewportXMin / ((float)(windows[bt.window].sx));
        }
        if (bt.viewportYMin > 1.0)
        {
            bt.viewportYMin = bt.viewportYMin / ((float)(windows[bt.window].sy));
        }

        bt.viewportXMax = coCoviseConfig::getFloat("right", str, -1);
        bt.viewportYMax = coCoviseConfig::getFloat("top", str, -1);
        if (bt.viewportXMax < 0)
        {
            float w,h;
            w = coCoviseConfig::getFloat("width", str, 1024);
            h = coCoviseConfig::getFloat("height", str, 768);
            if (w > 1.0)
                bt.viewportXMax = bt.viewportXMin + (w / ((float)(windows[bt.window].sx)));
            else
                bt.viewportXMax = bt.viewportXMin + w;
            if (h > 1.0)
                bt.viewportYMax = bt.viewportYMin + (h / ((float)(windows[bt.window].sy)));
            else
                bt.viewportYMax = bt.viewportYMin + h;
        }
        else
        {
            if (bt.viewportXMax > 1.0)
                bt.viewportXMax = bt.viewportXMax / ((float)(windows[bt.window].sx));
            if (bt.viewportYMax > 1.0)
                bt.viewportYMax = bt.viewportYMax / ((float)(windows[bt.window].sy));
        }

        bt.blendingTextureName = coCoviseConfig::getEntry("blendingTexture", str, "");

    }
}

coVRConfig::~coVRConfig()
{
    if (debugLevel(2))
        fprintf(stderr, "delete coVRConfig\n");
}

bool
coVRConfig::debugLevel(int level) const
{
    if (level <= m_dLevel)
        return true;
    else
        return false;
}

bool coVRConfig::mouseNav() const
{
    return m_mouseNav;
}

bool coVRConfig::mouseTracking() const
{
    return !Input::instance()->isTrackingOn() && mouseNav();
}

bool coVRConfig::useWiiMote() const
{
    return m_useWiiMote;
}

bool coVRConfig::useWiiNavigationVisenso() const
{
    return m_useWiiNavVisenso;
}

bool coVRConfig::isMenuModeOn() const
{
    return m_menuModeOn;
}

bool coVRConfig::colorSceneInMenuMode() const
{
    return m_coloringSceneInMenuMode;
}

bool coVRConfig::haveFlatDisplay() const
{
    return m_flatDisplay;
}

bool coVRConfig::useDisplayLists() const
{
    return m_useDisplayLists;
}

bool coVRConfig::useVBOs() const
{
    return m_useVBOs;
}

float coVRConfig::worldAngle() const
{
    return m_worldAngle;
}

int coVRConfig::lockToCPU() const
{
    return m_lockToCPU;
}
int coVRConfig::numScreens() const
{
    return screens.size();
}
int coVRConfig::numViewports() const
{
    return viewports.size();
}
int coVRConfig::numBlendingTextures() const
{
    return blendingTextures.size();
}
int coVRConfig::numChannels() const
{
    return channels.size();
}

int coVRConfig::numWindows() const
{
    return windows.size();
}

bool coVRConfig::frozen() const
{
    return m_freeze;
}

void coVRConfig::setFrozen(bool state)
{
    m_freeze = state;
}

bool coVRConfig::orthographic() const
{
    return m_orthographic;
}

void coVRConfig::setOrthographic(bool state)
{
    m_orthographic = state;
}

coVRConfig::MonoViews coVRConfig::monoView() const
{
    return m_monoView;
}

bool coVRConfig::stereoState() const
{
    return m_stereoState;
}

float coVRConfig::stereoSeparation() const
{
    return m_stereoSeparation;
}

// get number of requested stencil bits (default = 1)
int coVRConfig::numStencilBits() const
{
    return m_stencilBits;
}

float coVRConfig::nearClip() const
{
    return m_nearClip;
}

float coVRConfig::farClip() const
{
    return m_farClip;
}

float coVRConfig::getLODScale() const
{
    return m_LODScale;
}

void coVRConfig::setLODScale(float s)
{
    m_LODScale = s;
}

bool coVRConfig::stencil() const
{
    return m_stencil;
}

int coVRConfig::stereoMode() const
{
    return m_stereoMode;
}

void coVRConfig::setFrameRate(float fr)
{
    if (fr > 0)
    {
        constantFrameRate = true;
        constFrameTime = 1.0f / fr;
    }
    else
        constantFrameRate = false;
}

float coVRConfig::frameRate() const
{
    if (constantFrameRate)
        return 1.0f / constFrameTime;
    else
        return 0.f;
}

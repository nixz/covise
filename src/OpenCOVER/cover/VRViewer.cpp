/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

/************************************************************************
 *									*
 *          								*
 *                            (C) 1996					*
 *              Computer Centre University of Stuttgart			*
 *                         Allmandring 30				*
 *                       D-70550 Stuttgart				*
 *                            Germany					*
 *									*
 *									*
 *	File			VRViewer.C (Performer 2.0)		*
 *									*
 *	Description		stereo viewer class			*
 *									*
 *	Author			D. Rainer				*
 *									*
 *	Date			20.08.97				*
 *				09.01.97 general viewing frustum	*
 *									*
 *	Status			in dev					*
 *									*
 ************************************************************************/

#include <util/common.h>

#include <osgViewer/View>
#include "coVRRenderer.h"

#include "coVRSceneView.h"
#include <config/CoviseConfig.h>
#include "VRViewer.h"
#include "VRWindow.h"
#include "OpenCOVER.h"
#include <OpenVRUI/osg/mathUtils.h>
#include "coVRPluginList.h"
#include "coVRPluginSupport.h"
#include "coVRConfig.h"
#include "coCullVisitor.h"
#include "ARToolKit.h"
#include "EnableGLDebugOperation.h"
#include "input/input.h"

#include <osg/LightSource>
#include <osg/ApplicationUsage>
#include <osg/StateSet>
#include <osg/BlendFunc>
#include <osg/Camera>
#include <osg/Material>
#include <osg/CameraView>
#include <osg/DeleteHandler>
#include <osgDB/DatabasePager>
#include <osg/CullStack>
#include <osgText/Text>
#include <osgUtil/Statistics>
#include <osgUtil/UpdateVisitor>
#include <osgUtil/Optimizer>
#include <osgUtil/GLObjectsVisitor>
#include <osgDB/Registry>
#include <osgDB/ReadFile>

#include <osgGA/AnimationPathManipulator>
#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/DriveManipulator>
#include <osgGA/StateSetManipulator>

#include "coVRMSController.h"
#include "MSEventHandler.h"

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif

#define ANIMATIONSPEED 1

#ifndef OSG_NOTICE
#define OSG_NOTICE std::cerr
#endif

#include <cassert>

using namespace opencover;
using namespace covise;

int VRViewer::unsyncedFrames = 0;
// Draw operation, that does a draw on the scene graph.
struct RemoteSyncOp : public osg::Operation
{
    RemoteSyncOp()
        : osg::Operation("RemoteSyncOp", true)
    {
    }

    virtual void operator()(osg::Object *object)
    {
        osg::GraphicsContext *context = dynamic_cast<osg::GraphicsContext *>(object);
        if (!context)
            return;

        //do sync here before swapBuffers

        // sync multipc instances
        if (VRViewer::unsyncedFrames <= 0)
        {
            coVRMSController::instance()->syncDraw();
        }
        else
            VRViewer::unsyncedFrames--;
    }
};

// call plugins before syncing
struct PreSwapBuffersOp : public osg::Operation
{
    PreSwapBuffersOp(int WindowNumber)
        : osg::Operation("PreSwapBuffersOp", true)
    {
        windowNumber = WindowNumber;
    }

    virtual void operator()(osg::Object *object)
    {
        osg::GraphicsContext *context = dynamic_cast<osg::GraphicsContext *>(object);
        if (!context)
            return;

        if (OpenCOVER::instance()->initDone())
            coVRPluginList::instance()->preSwapBuffers(windowNumber);
    }
    int windowNumber;
};

// call plugins after syncing
struct PostSwapBuffersOp : public osg::Operation
{
    PostSwapBuffersOp(int WindowNumber)
        : osg::Operation("PostSwapBuffersOp", true)
    {
        windowNumber = WindowNumber;
    }

    virtual void operator()(osg::Object *object)
    {
        osg::GraphicsContext *context = dynamic_cast<osg::GraphicsContext *>(object);
        if (!context)
            return;

        if (OpenCOVER::instance()->initDone())
            coVRPluginList::instance()->postSwapBuffers(windowNumber);
    }
    int windowNumber;
};

// Compile operation, that compile OpenGL objects.
// nothing changed
struct ViewerCompileOperation : public osg::Operation
{
    ViewerCompileOperation(osg::Node *scene)
        : osg::Operation("Compile", false)
        , _scene(scene)
    {
    }

    virtual void operator()(osg::Object *object)
    {
        osg::GraphicsContext *context = dynamic_cast<osg::GraphicsContext *>(object);
        if (!context)
            return;

        // OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
        // osg::notify(osg::NOTICE)<<"Compile "<<context<<" "<<OpenThreads::Thread::CurrentThread()<<std::endl;

        // context->makeCurrent();

        osgUtil::GLObjectsVisitor compileVisitor;
        compileVisitor.setState(context->getState());

        // do the compile traversal
        if (_scene.valid())
            _scene->accept(compileVisitor);

        // osg::notify(osg::NOTICE)<<"Done Compile "<<context<<" "<<OpenThreads::Thread::CurrentThread()<<std::endl;
    }

    osg::ref_ptr<osg::Node> _scene;
};

// Draw operation, that does a draw on the scene graph.
struct ViewerRunOperations : public osg::Operation
{
    ViewerRunOperations()
        : osg::Operation("RunOperation", true)
    {
    }

    virtual void operator()(osg::Object *object)
    {
        static int numClears = 0; // OpenCOVER
        osg::GraphicsContext *context = dynamic_cast<osg::GraphicsContext *>(object);
        if (!context)
            return;
        // OpenCOVER begin
        // clear the whole window, if channels are smaller than window
        if (VRViewer::instance()->clearWindow || numClears > 0)
        {
            VRViewer::instance()->clearWindow = false;
            if (numClears == 0)
            {
                numClears = coVRConfig::instance()->numWindows() * 2;
            }
            numClears--;
            glDrawBuffer(GL_BACK);
            glViewport(0, 0, 1, 1);
            glScissor(0, 0, 1, 1);
            glClearColor(0.0, 0.0, 0.0, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawBuffer(GL_FRONT);
            glViewport(0, 0, 1, 1);
            glScissor(0, 0, 1, 1);
            glClearColor(0.0, 0.0, 0.0, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);
            context->makeCurrent();
        } // OpenCOVER end

        context->runOperations();
    }
};

//OpenCOVER
void VRViewer::handleEvents()
{
    myeh->update();
}

//OpenCOVER
void VRViewer::update()
{
    if (cover->debugLevel(5))
        fprintf(stderr, "VRViewer::update\n");

    if (animateSeparation)
    {
        if (animateSeparation == 1)
        {
            separation += ANIMATIONSPEED;
            if (separation > requestedSeparation)
            {
                separation = requestedSeparation;
                animateSeparation = 0;
            }
        }
        else if (animateSeparation == 2)
        {
            separation -= ANIMATIONSPEED;
            if (separation < 0.0)
            {
                separation = 0.0;
                animateSeparation = 0;
            }
        }
    }
    if (arTracking)
    {
        if (vpMarker->isVisible())
        {
            //osg::Matrix vm = vpMarker->getCameraTrans();
            //osg::Vec3 vp;

            //vm.getTrans( viewPos);
            //viewMat.setTrans( viewPos);
            viewMat = vpMarker->getCameraTrans();
        }
    }

    // compute viewer position

    viewPos = viewMat.getTrans();

    if (coVRConfig::instance()->stereoState())
    {
        rightViewPos.set(separation / 2.0f, 0.0f, 0.0f);
        leftViewPos.set(-(separation / 2.0f), 0.0f, 0.0f);
    }
    else
    {
        if (coVRConfig::instance()->monoView() == coVRConfig::MONO_LEFT)
        {
            rightViewPos.set(-(separation / 2.0f), 0.0f, 0.0f);
            leftViewPos.set(-(separation / 2.0f), 0.0f, 0.0f);
        }
        else if (coVRConfig::instance()->monoView() == coVRConfig::MONO_RIGHT)
        {
            rightViewPos.set(separation / 2.0f, 0.0f, 0.0f);
            leftViewPos.set(separation / 2.0f, 0.0f, 0.0f);
        }
        else
        {
            rightViewPos.set(0.0f, 0.0f, 0.0f);
            leftViewPos.set(0.0f, 0.0f, 0.0f);
        }
    }

    // get the current position of the eyes
    rightViewPos = viewMat.preMult(rightViewPos);
    leftViewPos = viewMat.preMult(leftViewPos);

    if (cover->debugLevel(5))
    {
        fprintf(stderr, "\t viewPos=[%f %f %f]\n", viewPos[0], viewPos[1], viewPos[2]);
        fprintf(stderr, "\t rightViewPos=[%f %f %f]\n", rightViewPos[0], rightViewPos[1], rightViewPos[2]);
        fprintf(stderr, "\t leftViewPos=[%f %f %f]\n", leftViewPos[0], leftViewPos[1], leftViewPos[2]);
        fprintf(stderr, "\n");
    }

    for (int i = 0; i < coVRConfig::instance()->numChannels(); i++)
    {
        setFrustumAndView(i);
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// osgViewer::Viewer implemention
//

VRViewer *VRViewer::instance()
{
    static VRViewer *singleton = NULL;
    if (!singleton)
        singleton = new VRViewer;
    return singleton;
}

//OpenCOVER
VRViewer::VRViewer()
: animateSeparation(0)
, stereoOn(true)
{
    if (cover->debugLevel(2))
        fprintf(stderr, "\nnew VRViewer\n");
    reEnableCulling = false;

    RenderToTexture = false;
    const bool glDebug = coCoviseConfig::isOn("GLDebug", false);
    if (glDebug) {
        setRealizeOperation(new EnableGLDebugOperation());
    }

    unsyncedFrames = 0;
    lastFrameTime = 0.0;

    myeh = new MSEventHandler;
    _eventHandlers.push_front(myeh);
    setKeyEventSetsDone(false);

    requestedSeparation = 0;

    // automatic angle
    screen_angle = NULL;

    // clear once at startup
    clearWindow = true;

    readConfigFile();

    viewDir.set(0.0f, 0.0f, 0.0f);

    //initial view position
    float xp = coCoviseConfig::getFloat("x", "COVER.ViewerPosition", 0.0f);
    float yp = coCoviseConfig::getFloat("y", "COVER.ViewerPosition", -2000.0f);
    float zp = coCoviseConfig::getFloat("z", "COVER.ViewerPosition", 30.0f);
    viewPos.set(xp, yp, zp);

    initialViewPos = viewPos;
    // set initial view
    viewMat.makeTranslate(viewPos);

    separation = Input::instance()->eyeDistance();
    if (coVRConfig::instance()->stereoState())
    {
        rightViewPos.set(separation / 2.0f, 0.0f, 0.0f);
        leftViewPos.set(-(separation / 2.0f), 0.0f, 0.0f);
    }
    else
    {
        if (coVRConfig::instance()->monoView() == coVRConfig::MONO_LEFT)
        {
            rightViewPos.set(-(separation / 2.0f), 0.0f, 0.0f);
            leftViewPos.set(-(separation / 2.0f), 0.0f, 0.0f);
        }
        else if (coVRConfig::instance()->monoView() == coVRConfig::MONO_RIGHT)
        {
            rightViewPos.set(separation / 2.0f, 0.0f, 0.0f);
            leftViewPos.set(separation / 2.0f, 0.0f, 0.0f);
        }
        else
        {
            rightViewPos.set(0.0f, 0.0f, 0.0f);
            leftViewPos.set(0.0f, 0.0f, 0.0f);
        }
    }
    rightViewPos = viewMat.preMult(rightViewPos);
    leftViewPos = viewMat.preMult(leftViewPos);
    arTracking = false;
    if (coCoviseConfig::isOn("COVER.Plugin.ARToolKit.TrackViewpoint", false))
    {
        arTracking = true;
        vpMarker = new ARToolKitMarker("ViewpointMarker");
    }
    overwritePAndV = false;
}

//OpenCOVER
VRViewer::~VRViewer()
{
    if (cover->debugLevel(2))
        fprintf(stderr, "\ndelete VRViewer\n");

#ifndef _WIN32
    if (strlen(monoCommand) > 0)
    {
        if (system(monoCommand) == -1)
        {
            cerr << "exec " << monoCommand << " failed" << endl;
        }
    }
#endif
}



void VRViewer::createViewportCameras(int i)
{
    viewportStruct &vp = coVRConfig::instance()->viewports[i];
    if(vp.distortMeshName.length() > 0)
    {

        int PBOnum = vp.PBOnum;
        osg::GraphicsContext *gc = coVRConfig::instance()->channels[PBOnum].camera->getGraphicsContext();

        osg::ref_ptr<osg::Camera> cameraWarp = new osg::Camera;
        cameraWarp->setClearColor(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
        cameraWarp->setClearMask(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        cameraWarp->setRenderOrder(osg::Camera::POST_RENDER);
        cameraWarp->setAllowEventFocus(false);

        cameraWarp->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        cameraWarp->setProjectionMatrix(osg::Matrix::ortho2D(0, 1, 0, 1));
        cameraWarp->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

        if (gc)
        {
            cameraWarp->setGraphicsContext(gc);
        }


        cameraWarp->setName("WarpOrtho");
        cameraWarp->setViewport(new osg::Viewport(vp.viewportXMin * coVRConfig::instance()->windows[coVRConfig::instance()->PBOs[PBOnum].windowNum].sx,
            vp.viewportYMin * coVRConfig::instance()->windows[coVRConfig::instance()->PBOs[PBOnum].windowNum].sy,
            (vp.viewportXMax - vp.viewportXMin) * coVRConfig::instance()->windows[coVRConfig::instance()->PBOs[PBOnum].windowNum].sx,
            (vp.viewportYMax - vp.viewportYMin) * coVRConfig::instance()->windows[coVRConfig::instance()->PBOs[PBOnum].windowNum].sy));


        osg::Geode *geode = distortionMesh(vp.distortMeshName.c_str());
        osg::ref_ptr<osg::StateSet> state = geode->getOrCreateStateSet();
        state->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        state->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
        state->setTextureAttributeAndModes(0, coVRConfig::instance()->PBOs[PBOnum].renderTargetTexture, osg::StateAttribute::ON);
        if(vp.blendingTextureName.length()>0)
        {
            osg::Image *blendTexImage = osgDB::readImageFile(vp.blendingTextureName.c_str());
            osg::Texture2D *blendTex = new osg::Texture2D;
            blendTex->ref();
            blendTex->setWrap(osg::Texture2D::WRAP_S, osg::Texture::CLAMP);
            blendTex->setWrap(osg::Texture2D::WRAP_T, osg::Texture::CLAMP);
            if (blendTexImage)
            {
                blendTex->setImage(blendTexImage);
            }
            state->setTextureAttributeAndModes(1, blendTex);
        }

        geode->setStateSet(state.get());

        cameraWarp->addChild(geode);

        cover->getScene()->addChild(cameraWarp.get());
    }
}

void VRViewer::createBlendingCameras(int i)
{
    blendingTextureStruct &bt = coVRConfig::instance()->blendingTextures[i];
        osg::GraphicsContext *gc = coVRConfig::instance()->windows[bt.window].window;

        osg::ref_ptr<osg::Camera> cameraBlend = new osg::Camera;
        cameraBlend->setClearColor(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
        cameraBlend->setClearMask(0);
        cameraBlend->setRenderOrder(osg::Camera::POST_RENDER);
        cameraBlend->setAllowEventFocus(false);

        cameraBlend->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        cameraBlend->setProjectionMatrix(osg::Matrix::ortho2D(0, 1, 0, 1));
        cameraBlend->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

        if (gc)
        {
            cameraBlend->setGraphicsContext(gc);
        }


        cameraBlend->setName("BlendingCamera");
        cameraBlend->setViewport(new osg::Viewport(bt.viewportXMin * coVRConfig::instance()->windows[bt.window].sx,
            bt.viewportYMin * coVRConfig::instance()->windows[bt.window].sy,
            (bt.viewportXMax - bt.viewportXMin) * coVRConfig::instance()->windows[bt.window].sx,
            (bt.viewportYMax - bt.viewportYMin) * coVRConfig::instance()->windows[bt.window].sy));


        osg::Vec3Array *coord = new osg::Vec3Array(4);
        (*coord)[0].set(0, 0, 0);
        (*coord)[1].set(1, 0, 0);
        (*coord)[2].set(1, 1, 0);
        (*coord)[3].set(0, 1, 0);

        osg::Vec2Array *texcoord = new osg::Vec2Array(4);

        (*texcoord)[0].set(0.0, 0.0);
        (*texcoord)[1].set(1.0, 0.0);
        (*texcoord)[2].set(1.0, 1.0);
        (*texcoord)[3].set(0.0, 1.0);

        osg::Geode *geode = new osg::Geode();
        osg::Geometry *geometry = new osg::Geometry();

        ushort *vertices = new ushort[4];
        vertices[0] = 0;
        vertices[1] = 1;
        vertices[2] = 2;
        vertices[3] = 3;
        osg::DrawElementsUShort *plane = new osg::DrawElementsUShort(osg::PrimitiveSet::QUADS, 4, vertices);

        geometry->setVertexArray(coord);
        geometry->addPrimitiveSet(plane);
        geometry->setTexCoordArray(0, texcoord);

        osg::ref_ptr<osg::StateSet> state = geode->getOrCreateStateSet();
        state->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        state->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
        state->setMode(GL_BLEND, osg::StateAttribute::ON);
        osg::BlendFunc *blendFunc = new osg::BlendFunc();
        blendFunc->setFunction(osg::BlendFunc::ZERO, osg::BlendFunc::SRC_COLOR);
        state->setAttributeAndModes(blendFunc, osg::StateAttribute::ON);
        if(bt.blendingTextureName.length()>0)
        {
            osg::Image *blendTexImage = osgDB::readImageFile(bt.blendingTextureName.c_str());
            osg::Texture2D *blendTex = new osg::Texture2D;
            blendTex->ref();
            blendTex->setWrap(osg::Texture2D::WRAP_S, osg::Texture::CLAMP);
            blendTex->setWrap(osg::Texture2D::WRAP_T, osg::Texture::CLAMP);
            if (blendTexImage)
            {
                blendTex->setImage(blendTexImage);
            }
            state->setTextureAttributeAndModes(0, blendTex);
        }
        
    geode->addDrawable(geometry);
        geode->setStateSet(state.get());

        cameraBlend->addChild(geode);

        cover->getScene()->addChild(cameraBlend.get());
}

osg::Geode *VRViewer::distortionMesh(const char *fileName)
{
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;


    osg::Vec3Array *positionArray = new osg::Vec3Array;
    osg::Vec4Array *colorArray = new osg::Vec4Array;
    osg::Vec2Array *textureArray = new osg::Vec2Array;
    osg::UShortArray *indexArray = new osg::UShortArray;

    
    osg::ref_ptr<osg::DrawElementsUShort> drawElement;
    // Get triangle indicies

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);

    FILE *fp = fopen(fileName,"r");
    if(fp!=NULL)
    {
        char buf[501];
        int numVert;
        int numFaces;
        int NATIVEYRES;
        int NATIVEXRES;
	float ortholeft=0;
	float orthoright=1;
	float sh = 1.0;
        while(!feof(fp))
        {
            fgets(buf,500,fp);
            if(strncmp(buf,"ORTHO_LEFT",10)==0)
            {
                sscanf(buf+10,"%f", &ortholeft);
	     sh = 1.0 / (orthoright - ortholeft);
            };
            if(strncmp(buf,"ORTHO_RIGHT",11)==0)
            {
                sscanf(buf+11,"%f", &orthoright);
	     sh = 1.0 / (orthoright - ortholeft);
            }
            if(strncmp(buf,"NATIVEXRES",10)==0)
            {
                sscanf(buf+10,"%d", &NATIVEXRES);
            }
            if(strncmp(buf,"NATIVEYRES",10)==0)
            {
                sscanf(buf+10,"%d", &NATIVEYRES);
                float vx,vy,brightness,tx,ty;
                for(int i=0;i<numVert;i++)
                {
                    fgets(buf,500,fp);
                    sscanf(buf,"%f %f %f %f %f",&vx,&vy,&brightness,&tx, &ty);
                    positionArray->push_back(osg::Vec3f(vx/NATIVEXRES,1.0-(vy/NATIVEYRES),0));
                    textureArray->push_back(osg::Vec2f((tx-ortholeft)*sh,ty));
		    colorArray->push_back(osg::Vec4f(brightness/255.0,brightness/255.0,brightness/255.0,1));
                }
                int tr[3];
                for(int i=0;i<numFaces;i++)
                {
                    fgets(buf,500,fp);
                    sscanf(buf,"[ %d %d %d",&tr[0],&tr[1],&tr[2]);
                    indexArray->push_back(tr[0]);
                    indexArray->push_back(tr[1]);
                    indexArray->push_back(tr[2]);
                }
            }
            else if(strncmp(buf,"VERTICES",8)==0)
            {
                sscanf(buf+8,"%d", &numVert);
            }
            else if(strncmp(buf,"FACES",5)==0)
            {
                sscanf(buf+5,"%d", &numFaces);
            }
        }
        drawElement = new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES, indexArray->size(), (GLushort *)indexArray->getDataPointer());
   
    }
    else
    {
        drawElement = new osg::DrawElementsUShort(osg::PrimitiveSet::QUADS, indexArray->size(), (GLushort *)indexArray->getDataPointer());
   
        positionArray->push_back(osg::Vec3f(0,0,0));
        positionArray->push_back(osg::Vec3f(1,0,0));
        positionArray->push_back(osg::Vec3f(1,1,0));
        positionArray->push_back(osg::Vec3f(0,1,0));
        textureArray->push_back(osg::Vec2f(0,0));
        textureArray->push_back(osg::Vec2f(1,0));
        textureArray->push_back(osg::Vec2f(1,1));
        textureArray->push_back(osg::Vec2f(0,1));

        indexArray->push_back(0);
        indexArray->push_back(1);
        indexArray->push_back(2);
        indexArray->push_back(3);
	colorArray->push_back(osg::Vec4f(1,1,1,1));
	colorArray->push_back(osg::Vec4f(1,1,1,1));
	colorArray->push_back(osg::Vec4f(1,1,1,1));
	colorArray->push_back(osg::Vec4f(1,1,1,1));
    }

     geometry->addPrimitiveSet(drawElement);
    geometry->setTexCoordArray(0,textureArray,osg::Array::BIND_PER_VERTEX);
    geometry->setTexCoordArray(1,textureArray,osg::Array::BIND_PER_VERTEX);
    geometry->setVertexArray(positionArray);
    geometry->setColorArray(colorArray);
    geometry->setColorBinding(osg::Geometry::BIND_PER_VERTEX); // colors per vertex for blending
    geode->addDrawable(geometry);


    return geode.release();
}

//OpenCOVER
void
VRViewer::config()
{
    if (cover->debugLevel(3))
        fprintf(stderr, "VRViewer::config\n");
    statsHandler = new osgViewer::StatsHandler;
    addEventHandler(statsHandler);
    scene = NULL;
    //scene = VRSceneGraph::sg->getScene();
    for (int i = 0; i < coVRConfig::instance()->numChannels(); i++)
    {
        createChannels(i);
    }
    for (int i = 0; i < coVRConfig::instance()->numViewports(); i++)
    {
        createViewportCameras(i);
    }
    for (int i = 0; i < coVRConfig::instance()->numBlendingTextures(); i++)
    {
        createBlendingCameras(i);
    }
    

    float r = coCoviseConfig::getFloat("r", "COVER.Background", 0.0f);
    float g = coCoviseConfig::getFloat("g", "COVER.Background", 0.0f);
    float b = coCoviseConfig::getFloat("b", "COVER.Background", 0.0f);
    backgroundColor = osg::Vec4(r, g, b, 1.0f);

    setClearColor(backgroundColor);

    // create the windows and run the threads.
    if (coCoviseConfig::isOn("COVER.MultiThreaded", false))
    {
        cerr << "VRViewer: using one thread per camera" << endl;
        osg::Referenced::setThreadSafeReferenceCounting(true);
        setThreadingModel(CullThreadPerCameraDrawThreadPerContext);
    }
    else
    {
        std::string threadingMode = coCoviseConfig::getEntry("COVER.MultiThreaded");
        if (threadingMode == "CullDrawThreadPerContext")
        {
            setThreadingModel(CullDrawThreadPerContext);
        }
        else if (threadingMode == "ThreadPerContext")
        {
            setThreadingModel(ThreadPerContext);
        }
        else if (threadingMode == "DrawThreadPerContext")
        {
            setThreadingModel(DrawThreadPerContext);
        }
        else if (threadingMode == "CullThreadPerCameraDrawThreadPerContext")
        {
            setThreadingModel(CullThreadPerCameraDrawThreadPerContext);
        }
        else if (threadingMode == "ThreadPerCamera")
        {
            setThreadingModel(ThreadPerCamera);
        }
        else if (threadingMode == "AutomaticSelection")
        {
            setThreadingModel(AutomaticSelection);
        }
        else
            setThreadingModel(SingleThreaded);
    }
    realize();

#ifdef __linux__
    if(coVRConfig::instance()->lockToCPU()>=0)
    {
	cpu_set_t cpumask;
	CPU_ZERO(&cpumask);
	
        CPU_SET(coVRConfig::instance()->lockToCPU(), &cpumask);
	
	pthread_setaffinity_np(pthread_self(), sizeof(cpumask), &cpumask);
    }
    else
    {
	cpu_set_t cpumask;
	CPU_ZERO(&cpumask);
	for (int i = 0; i < CPU_SETSIZE; i++)
	{
	    CPU_SET(i, &cpumask);
	}

	pthread_setaffinity_np(pthread_self(), sizeof(cpumask), &cpumask);
    }
//#elif defined(HAVE_THREE_PARAM_SCHED_SETAFFINITY)
//           sched_setaffinity( 0, sizeof(cpumask), &cpumask );
//#elif defined(HAVE_TWO_PARAM_SCHED_SETAFFINITY)
//            sched_setaffinity( 0, &cpumask );
#endif

    for (int i = 0; i < coVRConfig::instance()->numChannels(); i++)
    {
        setFrustumAndView(i);
    }
    assignSceneDataToCameras();
}

//OpenCOVER
void VRViewer::updateViewerMat(const osg::Matrix &mat)
{

    if (cover->debugLevel(5))
    {
        fprintf(stderr, "VRViewer::updateViewerMat\n");
        //coCoord coord;
        //mat.getOrthoCoord(&coord);
        //fprintf(stderr,"hpr=[%f %f %f]\n", coord.hpr[0], coord.hpr[1], coord.hpr[2]);
    }

    if (!coVRConfig::instance()->frozen())
    {
        viewMat = mat;
    }
}

//OpenCOVER
void
VRViewer::setSeparation(float sep)
{
    requestedSeparation = stereoOn ? sep : 0.;

    if (requestedSeparation > separation)
    {
        animateSeparation = 1;
    }
    else if (requestedSeparation == 0)
    {
        animateSeparation = 2;
    }
    else
    {
        separation = sep;
        animateSeparation = 0;
    }
}

void
VRViewer::flipStereo()
{
    separation = -separation;
}
void
VRViewer::setRenderToTexture(bool b)
{
    RenderToTexture = b;
}

//OpenCOVER
void
VRViewer::createChannels(int i)
{
    if (cover->debugLevel(3))
        fprintf(stderr, "VRViewer::createChannels\n");

    osg::GraphicsContext::WindowingSystemInterface *wsi = osg::GraphicsContext::getWindowingSystemInterface();
    if (!wsi)
    {
        osg::notify(osg::NOTICE) << "VRViewer : Error, no WindowSystemInterface available, cannot create windows." << std::endl;
        return;
    }
    osg::ref_ptr<osgViewer::GraphicsWindow> gw = NULL;
    const int vp = coVRConfig::instance()->channels[i].viewportNum;
    if (vp >= coVRConfig::instance()->numViewports())
    {
        fprintf(stderr, "VRViewer:: error creating channel %d\n", i);
        fprintf(stderr, "viewportNum %d is out of range (viewports are counted starting from 0)\n", vp);
        return;
    }
    if(vp >= 0)
    {
        const int win = coVRConfig::instance()->viewports[vp].window;
        if (win < 0 || win >= coVRConfig::instance()->numWindows())
        {
            fprintf(stderr, "VRViewer:: error creating channel %d\n", i);
            fprintf(stderr, "windowIndex %d is out of range (windows are counted starting from 0)\n", win);
            return;
        }
        gw = coVRConfig::instance()->windows[win].window;
    }
    else
    {
        gw = coVRConfig::instance()->windows[coVRConfig::instance()->PBOs[i].windowNum].window;
        RenderToTexture = true;
    }
    if (i == 0)
    {
        coVRConfig::instance()->channels[i].camera = _camera;
    }
    else
    {
        coVRConfig::instance()->channels[i].camera = new osg::Camera;
        coVRConfig::instance()->channels[i].camera->setView(this);

        coVRConfig::instance()->channels[i].camera->addChild(this->getScene()->getSceneData());
    }
    if (RenderToTexture)
    {
        osg::Camera::RenderTargetImplementation renderImplementation;

        // Voreingestellte Option f�r Render-Target aus Config auslesen
        std::string buf = coCoviseConfig::getEntry("COVER.Plugin.Vrml97.RTTImplementation");
        if (!buf.empty())
        {
            cerr << "renderImplementation: " << buf << endl;
            if (strcasecmp(buf.c_str(), "fbo") == 0)
                renderImplementation = osg::Camera::FRAME_BUFFER_OBJECT;
            if (strcasecmp(buf.c_str(), "pbuffer") == 0)
                renderImplementation = osg::Camera::PIXEL_BUFFER;
            if (strcasecmp(buf.c_str(), "pbuffer-rtt") == 0)
                renderImplementation = osg::Camera::PIXEL_BUFFER_RTT;
            if (strcasecmp(buf.c_str(), "fb") == 0)
                renderImplementation = osg::Camera::FRAME_BUFFER;
            if (strcasecmp(buf.c_str(), "window") == 0)
                renderImplementation = osg::Camera::SEPERATE_WINDOW;
        }
        else
            renderImplementation = osg::Camera::FRAME_BUFFER_OBJECT;

        renderTargetTexture = new osg::Texture2D;
        const osg::GraphicsContext *cg = coVRConfig::instance()->windows[coVRConfig::instance()->PBOs[i].windowNum].window;
        if (!cg)
        {
            fprintf(stderr, "no graphics context for cover screen %d\n", i);
            return;
        }

        renderTargetTexture->setTextureSize(coVRConfig::instance()->PBOs[i].PBOsx, coVRConfig::instance()->PBOs[i].PBOsy);
        renderTargetTexture->setInternalFormat(GL_RGBA);
        //renderTargetTexture->setFilter(osg::Texture2D::MIN_FILTER,osg::Texture2D::NEAREST);
        //	   renderTargetTexture->setFilter(osg::Texture2D::MAG_FILTER,osg::Texture2D::NEAREST);
        renderTargetTexture->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR);
        renderTargetTexture->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
        coVRConfig::instance()->PBOs[i].renderTargetTexture = renderTargetTexture;
        coVRConfig::instance()->channels[i].camera->attach(osg::Camera::COLOR_BUFFER, renderTargetTexture);
        coVRConfig::instance()->channels[i].camera->setRenderTargetImplementation(renderImplementation);


        coVRConfig::instance()->channels[i].camera->setViewport(new osg::Viewport(0, 0,coVRConfig::instance()->PBOs[i].PBOsx,
                                                                                       coVRConfig::instance()->PBOs[i].PBOsy));

        

    }
    if (gw.get())
    {
        coVRConfig::instance()->channels[i].camera->setGraphicsContext(gw.get());
        const osg::GraphicsContext::Traits *traits = gw->getTraits();
        gw->getEventQueue()->getCurrentEventState()->setWindowRectangle(traits->x, traits->y, traits->width, traits->height);
        if(!RenderToTexture)
        {
            // set viewport
            int viewportNumber = coVRConfig::instance()->channels[i].viewportNum;
            coVRConfig::instance()->channels[i].camera->setViewport(new osg::Viewport(coVRConfig::instance()->viewports[viewportNumber].viewportXMin * traits->width,
                coVRConfig::instance()->viewports[viewportNumber].viewportYMin * traits->height,
                (coVRConfig::instance()->viewports[viewportNumber].viewportXMax - coVRConfig::instance()->viewports[viewportNumber].viewportXMin) * traits->width,
                (coVRConfig::instance()->viewports[viewportNumber].viewportYMax - coVRConfig::instance()->viewports[viewportNumber].viewportYMin) * traits->height));
        }


        coVRConfig::instance()->channels[i].camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        coVRConfig::instance()->channels[i].camera->setClearColor(osg::Vec4(0.0, 0.4, 0.5, 0.0));
        coVRConfig::instance()->channels[i].camera->setClearStencil(0);

        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        coVRConfig::instance()->channels[i].camera->setDrawBuffer(buffer);
        coVRConfig::instance()->channels[i].camera->setReadBuffer(buffer);
        coVRConfig::instance()->channels[i].camera->setCullMask(Isect::Visible | Isect::OsgEarthSecondary); // also cull secondary geometry in osgEarth
        coVRConfig::instance()->channels[i].camera->setCullMaskLeft(Isect::Visible | Isect::Left | Isect::OsgEarthSecondary); // also cull secondary geometry in osgEarth
        coVRConfig::instance()->channels[i].camera->setCullMaskRight(Isect::Visible | Isect::Right | Isect::OsgEarthSecondary); // also cull secondary geometry in osgEarth
        //coVRConfig::instance()->screens[i].camera->getGraphicsContext()->getState()->checkGLErrors(osg::State::ONCE_PER_ATTRIBUTE);
    }
    else
        cerr << "window " << coVRConfig::instance()->viewports[coVRConfig::instance()->channels[i].viewportNum].window << " of screen " << i << " not defined" << endl;

    osg::DisplaySettings *ds = NULL;
    ds = _displaySettings.valid() ? _displaySettings.get() : osg::DisplaySettings::instance().get();
    if (i > 0) //we need different displaySettings for other channels
    {
        ds = new osg::DisplaySettings(*(_displaySettings.valid() ? _displaySettings.get() : osg::DisplaySettings::instance().get()));
    }

    // set up the use of stereo by default.
    ds->setStereo(coVRConfig::instance()->stereoState());
    if (coVRConfig::instance()->doMultisample())
        ds->setNumMultiSamples(coVRConfig::instance()->getMultisampleSamples());
    ds->setStereoMode((osg::DisplaySettings::StereoMode)coVRConfig::instance()->channels[i].stereoMode);
    ds->setMinimumNumStencilBits(coVRConfig::instance()->numStencilBits());
    coVRConfig::instance()->channels[i].ds = ds;
    coVRConfig::instance()->channels[i].camera->setDisplaySettings(ds);

    coVRConfig::instance()->channels[i].camera->setComputeNearFarMode(osgUtil::CullVisitor::DO_NOT_COMPUTE_NEAR_FAR);
    //coVRConfig::instance()->channels[i].camera->setCullingMode(osg::CullSettings::VIEW_FRUSTUM_CULLING);
    coVRConfig::instance()->channels[i].camera->setCullingMode(osg::CullSettings::ENABLE_ALL_CULLING);

    osgViewer::Renderer *renderer = new coVRRenderer(coVRConfig::instance()->channels[i].camera.get(), i);
    coVRConfig::instance()->channels[i].camera->setRenderer(renderer);
    if (i != 0)
    {
        addCamera(coVRConfig::instance()->channels[i].camera.get());
    }

    //PHILIP add LOD scale from configuration to deal with LOD updating
    float lodScale = coCoviseConfig::getFloat("COVER.LODScale", 0.0f);
    if (lodScale != 0)
    {
        coVRConfig::instance()->channels[i].camera->setLODScale(lodScale);
    }

  
}

void
VRViewer::culling(bool enable, osg::CullSettings::CullingModeValues mode, bool once)
{
    if (once)
    {
        reEnableCulling = true;
    }
    for (int i = 0; i < coVRConfig::instance()->numChannels(); i++)
    {
        if (enable)
            coVRConfig::instance()->channels[i].camera->setCullingMode(mode);
        else
            coVRConfig::instance()->channels[i].camera->setCullingMode(osg::CullSettings::NO_CULLING);
    }
}

void
VRViewer::setClearColor(const osg::Vec4 &color)
{
    for (int i = 0; i < coVRConfig::instance()->numChannels(); ++i)
    {
        coVRConfig::instance()->channels[i].camera->setClearColor(color);
    }
}


//OpenCOVER
void
VRViewer::setFrustumAndView(int i)
{
    if (cover->debugLevel(5))
        fprintf(stderr, "VRViewer::setFrustum %d\n", i);

    coVRConfig *coco = coVRConfig::instance();
    channelStruct *currentChannel = &coco->channels[i];
    screenStruct *currentScreen = &coco->screens[currentChannel->screenNum];

    if (overwritePAndV)
    {
        currentChannel->camera->setViewMatrix(currentChannel->rightView);
        currentChannel->camera->setProjectionMatrix(currentChannel->rightProj);
    }
    else
    {

        osg::Vec3 xyz; // center position of the screen
        osg::Vec3 hpr; // orientation of the screen
        osg::Matrix mat, trans, euler; // xform screencenter - world origin
        osg::Matrixf offsetMat;
        osg::Vec3 leftEye, rightEye, middleEye; // transformed eye position
        float rc_dist, lc_dist, mc_dist; // dist from eye to screen for left&right chan
        float rc_left, rc_right, rc_bottom, rc_top; // parameter of right frustum
        float lc_left, lc_right, lc_bottom, lc_top; // parameter of left frustum
        float mc_left, mc_right, mc_bottom, mc_top; // parameter of middle frustum
        float n_over_d; // near over dist -> Strahlensatz
        float dx, dz; // size of screen

        //othEyesDirOffset; == hpr
        //osg::Vec3  rightEyePosOffset(0.0,0.0,0.0), leftEyePosOffset(0.0,0.0,0.0);

        osg::Matrixf offsetMatRight;
        osg::Matrixf offsetMatLeft;

        dx = currentScreen->hsize;
        dz = currentScreen->vsize;

        hpr = currentScreen->hpr;
        xyz = currentScreen->xyz;

        // first set pos and yaxis
        // next set separation and dir from covise.config
        // which I think workks only if 0 0 0 (dr)
        rightViewPos.set(separation / 2.0f, 0.0f, 0.0f);
        leftViewPos.set(-(separation / 2.0f), 0.0f, 0.0f);
        middleViewPos.set(0.0, 0.0, 0.0);

        ////// IWR : get values of moving screen; change only if moved by >1%
        if (screen_angle && screen_angle[0].screen == i)
        {
            float new_angle;

            new_angle = ((*screen_angle[0].value - screen_angle[0].cmin) / (screen_angle[0].cmax - screen_angle[0].cmin)) * (screen_angle[0].maxangle - screen_angle[0].minangle) + screen_angle[0].minangle;

            // change angle only, when change is significant (> 1%)
            float change_delta = fabs(screen_angle[0].maxangle - screen_angle[0].minangle) * 0.01;

            if (fabs(currentScreen->hpr[screen_angle[0].hpr] - new_angle) > change_delta)
            {
                currentScreen->hpr[screen_angle[0].hpr] = new_angle;
                //		   cerr << "Cereal gives " << *screen_angle[0].value << endl;
                cerr << "Setting Screen angle " << screen_angle[0].hpr << " to " << new_angle << endl;
            }
        }

        if (coco->trackedHMD) // moving HMD
        {
            // moving hmd: frustum ist fixed and only a little bit assymetric through stereo
            rightEye.set(separation / 2.0, 0, 0);
            leftEye.set(-(separation / 2.0), 0, 0);
            middleEye.set(0.0, 0, 0);

            // transform the left and right eye with the viewer matrix
            rightEye = viewMat.preMult(rightEye);
            leftEye = viewMat.preMult(leftEye);
            middleEye = viewMat.preMult(middleEye);
        }

        else if (coco->HMDMode) // weiss nicht was das fuer ein code ist
        // wenn screen center und dir 000 sind
        {

            // transform the screen to fit the xz-plane

            trans.makeTranslate(xyz[0], xyz[1], xyz[2]);
            trans.invert(trans);

            MAKE_EULER_MAT(euler, hpr[0], hpr[1], hpr[2]);
            euler.invert(euler);

            mat.mult(trans, euler);

            euler.makeRotate(-coco->worldAngle(), osg::X_AXIS);
            //euler.invertN(euler);
            mat.mult(euler, mat);

            rightEye.set(separation / 2.0f, 0.0f, 0.0f);
            leftEye.set(-(separation / 2.0f), 0.0f, 0.0f);
            middleEye.set(0.0, 0.0, 0.0);
            rightEye += initialViewPos;
            leftEye += initialViewPos;
            middleEye += initialViewPos;

            // transform the left and right eye with this matrix
            rightEye = viewMat.preMult(rightEye);
            leftEye = viewMat.preMult(leftEye);
            middleEye = viewMat.preMult(middleEye);

            rightViewPos += initialViewPos;
            leftViewPos += initialViewPos;

            // add world angle
            osg::Matrixf rotAll, newDir;
            rotAll.makeRotate(-coco->worldAngle(), osg::X_AXIS);
            newDir.mult(viewMat, rotAll);

            // first set it if both channels were at the position of a mono channel
            // currentScreen->camera->setOffset(newDir.ptr(),0,0);

            viewPos = newDir.getTrans();

            // for stereo use the pfChanViewOffset to position it correctly
            // and to set the viewing direction (normal of the screen)

            //rightEyePosOffset=rightViewPos - viewPos;
            //leftEyePosOffset=leftViewPos - viewPos;
        }

        else // fixed screens: viewing frustums change with tracking and are asymetric because of tracking and stereo
        {

            // transform the screen to fit the xz-plane
            trans.makeTranslate(-xyz[0], -xyz[1], -xyz[2]);

            //euler.makeRotate(hpr[0],osg::Y_AXIS, hpr[1],osg::X_AXIS, hpr[2],osg::Z_AXIS);

            MAKE_EULER_MAT_VEC(euler, hpr);
            euler.invert(euler);

            mat.mult(trans, euler);

            euler.makeRotate(-coco->worldAngle(), osg::X_AXIS);
            //euler.invertN(euler);
            mat.mult(euler, mat);
            //cerr << "test" << endl;

            rightEye.set(separation / 2.0, 0.0, 0.0);
            leftEye.set(-(separation / 2.0), 0.0, 0.0);
            middleEye.set(0.0, 0.0, 0.0);

            // transform the left and right eye with this matrix
            rightEye = viewMat.preMult(rightEye);
            leftEye = viewMat.preMult(leftEye);
            middleEye = viewMat.preMult(middleEye);

            rightEye = mat.preMult(rightEye);
            leftEye = mat.preMult(leftEye);
            middleEye = mat.preMult(middleEye);

            // rightEye = (0.5*separation,0,0)*viewMat*eventuell die trafos des screens in den ursprung(cave)
        }

        offsetMat = mat;

        // compute right frustum

        // dist of right channel eye to screen (absolute)
        if (coco->trackedHMD)
        {
            rc_dist = coco->HMDDistance;
            if (coco->orthographic())
            {
                currentChannel->rightProj.makeOrtho(-dx / 2.0, dx / 2.0, -dz / 2.0, dz / 2.0, coco->nearClip(), coco->farClip());
                currentChannel->leftProj.makeOrtho(-dx / 2.0, dx / 2.0, -dz / 2.0, dz / 2.0, coco->nearClip(), coco->farClip());
                currentChannel->camera->setProjectionMatrixAsOrtho(-dx / 2.0, dx / 2.0, -dz / 2.0, dz / 2.0, coco->nearClip(), coco->farClip());
            }
            else
            {
                if (currentScreen->lTan != -1)
                {
                    float n = coco->nearClip();
                    currentChannel->rightProj.makeFrustum(-n * currentScreen->lTan, n * currentScreen->rTan, -n * currentScreen->bTan, n * currentScreen->tTan, coco->nearClip(), coco->farClip());
                    currentChannel->leftProj.makeFrustum(-n * currentScreen->lTan, n * currentScreen->rTan, -n * currentScreen->bTan, n * currentScreen->tTan, coco->nearClip(), coco->farClip());
                    currentChannel->camera->setProjectionMatrixAsFrustum(-n * currentScreen->lTan, n * currentScreen->rTan, -n * currentScreen->bTan, n * currentScreen->tTan, coco->nearClip(), coco->farClip());
                }
                else
                {
                    currentChannel->rightProj.makePerspective(coco->HMDViewingAngle, dx / dz, coco->nearClip(), coco->farClip());
                    currentChannel->leftProj.makePerspective(coco->HMDViewingAngle, dx / dz, coco->nearClip(), coco->farClip());
                    currentChannel->camera->setProjectionMatrixAsPerspective(coco->HMDViewingAngle, dx / dz, coco->nearClip(), coco->farClip());
                }
            }
        }
        else
        {
            rc_dist = -rightEye[1];
            lc_dist = -leftEye[1];
            mc_dist = -middleEye[1];

            // relation near plane to screen plane
            if (coco->orthographic())
                n_over_d = 1.0;
            else
                n_over_d = coco->nearClip() / rc_dist;

            // parameter of right channel
            rc_right = n_over_d * (dx / 2.0 - rightEye[0]);
            rc_left = -n_over_d * (dx / 2.0 + rightEye[0]);
            rc_top = n_over_d * (dz / 2.0 - rightEye[2]);
            rc_bottom = -n_over_d * (dz / 2.0 + rightEye[2]);

            // compute left frustum
            if (coco->orthographic())
                n_over_d = 1.0;
            else
                n_over_d = coco->nearClip() / lc_dist;
            lc_right = n_over_d * (dx / 2.0 - leftEye[0]);
            lc_left = -n_over_d * (dx / 2.0 + leftEye[0]);
            lc_top = n_over_d * (dz / 2.0 - leftEye[2]);
            lc_bottom = -n_over_d * (dz / 2.0 + leftEye[2]);

            // compute left frustum
            if (coco->orthographic())
                n_over_d = 1.0;
            else
                n_over_d = coco->nearClip() / mc_dist;
            mc_right = n_over_d * (dx / 2.0 - middleEye[0]);
            mc_left = -n_over_d * (dx / 2.0 + middleEye[0]);
            mc_top = n_over_d * (dz / 2.0 - middleEye[2]);
            mc_bottom = -n_over_d * (dz / 2.0 + middleEye[2]);

            if (coco->orthographic())
            {
                currentChannel->rightProj.makeOrtho(rc_left, rc_right, rc_bottom, rc_top, coco->nearClip(), coco->farClip());
                currentChannel->leftProj.makeOrtho(lc_left, lc_right, lc_bottom, lc_top, coco->nearClip(), coco->farClip());
                currentChannel->camera->setProjectionMatrixAsOrtho(mc_left, mc_right, mc_bottom, mc_top, coco->nearClip(), coco->farClip());
            }
            else
            {
                currentChannel->rightProj.makeFrustum(rc_left, rc_right, rc_bottom, rc_top, coco->nearClip(), coco->farClip());
                currentChannel->leftProj.makeFrustum(lc_left, lc_right, lc_bottom, lc_top, coco->nearClip(), coco->farClip());
                currentChannel->camera->setProjectionMatrixAsFrustum(mc_left, mc_right, mc_bottom, mc_top, coco->nearClip(), coco->farClip());
            }
        }

        // set view
        if (coco->trackedHMD)
        {
            // set the view mat from tracker, translated by separation/2
            osg::Vec3 viewDir(0, 1, 0), viewUp(0, 0, 1);
            viewDir = viewMat.transform3x3(viewDir, viewMat);
            viewDir.normalize();
            //fprintf(stderr,"viewDir=[%f %f %f]\n", viewDir[0], viewDir[1], viewDir[2]);
            viewUp = viewMat.transform3x3(viewUp, viewMat);
            viewUp.normalize();
            currentChannel->rightView.makeLookAt(rightEye, rightEye + viewDir, viewUp);
            ///currentScreen->rightView=viewMat;
            currentChannel->leftView.makeLookAt(leftEye, leftEye + viewDir, viewUp);
            ///currentScreen->leftView=viewMat;

            currentChannel->camera->setViewMatrix(osg::Matrix::lookAt(middleEye, middleEye + viewDir, viewUp));
            ///currentScreen->camera->setViewMatrix(viewMat);
        }
        else
        {
            // take the normal to the plane as orientation this is (0,1,0)
            currentChannel->rightView.makeLookAt(osg::Vec3(rightEye[0], rightEye[1], rightEye[2]), osg::Vec3(rightEye[0], rightEye[1] + 1, rightEye[2]), osg::Vec3(0, 0, 1));
            currentChannel->rightView.preMult(offsetMat);

            currentChannel->leftView.makeLookAt(osg::Vec3(leftEye[0], leftEye[1], leftEye[2]), osg::Vec3(leftEye[0], leftEye[1] + 1, leftEye[2]), osg::Vec3(0, 0, 1));
            currentChannel->leftView.preMult(offsetMat);

            currentChannel->camera->setViewMatrix(offsetMat * osg::Matrix::lookAt(osg::Vec3(middleEye[0], middleEye[1], middleEye[2]), osg::Vec3(middleEye[0], middleEye[1] + 1, middleEye[2]), osg::Vec3(0, 0, 1)));
        }
    }
}

//OpenCOVER
void
VRViewer::readConfigFile()
{
    if (cover->debugLevel(4))
        fprintf(stderr, "VRViewer::readConfigFile\n");

    /// ================= Moving screen initialisation =================

    // temp initialisation until the real address is determin in
    // the tracker routines
    static float init_cereal_analog_value = 0.0;

    screen_angle = NULL;
    string line = coCoviseConfig::getEntry("analogInput", "COVER.Input.CerealConfig.ScreenAngle");
    if (!line.empty())
    {
        char hpr;
        cerr << "Reading ScreenAngle";
        screen_angle = new angleStruct;
        screen_angle->value = &init_cereal_analog_value;

        // parameters:  Analog Input, min, max, minangle, maxangle, screen, HPR
        //if(sscanf(line,   "%d %f %f %f %f %d %c",

        const char *configEntry = "COVER.Input.CerealConfig.ScreenAngle";

        screen_angle->analogInput = coCoviseConfig::getInt("analogInput", configEntry, 0);
        screen_angle->cmin = coCoviseConfig::getFloat("cmin", configEntry, 0.0f);
        screen_angle->cmax = coCoviseConfig::getFloat("cmax", configEntry, 0.0f);
        screen_angle->minangle = coCoviseConfig::getFloat("minangle", configEntry, 0.0f);
        screen_angle->maxangle = coCoviseConfig::getFloat("maxangle", configEntry, 0.0f);
        screen_angle->screen = coCoviseConfig::getInt("screen", configEntry, 0);
        hpr = coCoviseConfig::getEntry("hpr", configEntry)[0];

        *screen_angle->value = screen_angle->cmin;
        cerr << "\n  - Using analog Input #" << screen_angle->analogInput
             << "\n  cMin/cMax " << screen_angle->cmin
             << " ... " << screen_angle->cmax
             << " " << screen_angle->minangle << " " << screen_angle->maxangle << " " << screen_angle->screen << " " << hpr << endl;
        switch (hpr)
        {
        case 'H':
        case 'h':
            screen_angle->hpr = 0;
            break;
        case 'P':
        case 'p':
            screen_angle->hpr = 1;
            break;
        case 'R':
        case 'r':
            screen_angle->hpr = 2;
            break;
        default:
            cerr << "could not identify rotation axis\nIgnoring ...\n";
            screen_angle = NULL;
        }
    }

    /// ================= Moving screen initialisation End =================

    strcpy(stereoCommand, "");
    strcpy(monoCommand, "");
    line = coCoviseConfig::getEntry("command", "COVER.Stereo");
    if (!line.empty())
    {
        strcpy(stereoCommand, line.c_str());
    }
    line = coCoviseConfig::getEntry("command", "COVER.Mono");
    if (!line.empty())
    {
        strcpy(monoCommand, line.c_str());
    }
    if (coVRConfig::instance()->stereoState())
    {
        if (strlen(stereoCommand) > 0)
            if (system(stereoCommand) == -1)
            {
                cerr << "VRViewer::readConfigFile: stereo command " << stereoCommand << " failed" << endl;
            }
    }
}

/*______________________________________________________________________*/
// OpenCOVER
void
VRViewer::stereoSepCallback(void *data, buttonSpecCell *spec)
{
    VRViewer *viewer = static_cast<VRViewer *>(data);
    int status = (int)spec->state;

    viewer->stereoOn = status;
    viewer->setSeparation(Input::instance()->eyeDistance());
}

/*______________________________________________________________________*/
// OpenCOVER
void
VRViewer::freezeCallback(void *, buttonSpecCell *spec)
{
    if (spec->state == 1.0)
    {
        coVRConfig::instance()->setFrozen(true);
    }
    else
    {
        coVRConfig::instance()->setFrozen(false);
    }
}

/*______________________________________________________________________*/
// OpenCOVER
void
VRViewer::orthographicCallback(void *, buttonSpecCell *spec)
{
    coVRConfig::instance()->setOrthographic((spec->state == 1.0));
}

// OpenCOVER
void VRViewer::redrawHUD(double interval)
{
    // do a redraw of the scene no more then once per second (default)
    if (cover->currentTime() > lastFrameTime + interval)
    {
        unsyncedFrames++; // do not wait for slaves during temporary updates of the headup display
        frame(); // draw one frame
    }
}

// OpenCOVER
void VRViewer::frame()
{

    lastFrameTime = cover->currentTime();

    osgViewer::Viewer::frame(cover->frameTime());
    if (reEnableCulling)
    {
        culling(true);
        reEnableCulling = false;
    }
}

void VRViewer::startThreading()
{
    //DebugBreak();
    if (_threadsRunning)
        return;

    osg::notify(osg::INFO) << "VRViewer::VRstartThreading() - starting threading" << std::endl;

    // release any context held by the main thread.
    releaseContext();

    _threadingModel = _threadingModel == AutomaticSelection ? suggestBestThreadingModel() : _threadingModel;

    Contexts contexts;
    getContexts(contexts);

    Cameras cameras;
    getCameras(cameras);

    unsigned int numThreadsOnStartBarrier = 0;
    unsigned int numThreadsOnEndBarrier = 0;
    switch (_threadingModel)
    {
    case (SingleThreaded):
        numThreadsOnStartBarrier = 1;
        numThreadsOnEndBarrier = 1;
        return;
    case (CullDrawThreadPerContext):
        numThreadsOnStartBarrier = contexts.size() + 1;
        numThreadsOnEndBarrier = contexts.size() + 1;
        break;
    case (DrawThreadPerContext):
        numThreadsOnStartBarrier = 1;
        numThreadsOnEndBarrier = 1;
        break;
    case (CullThreadPerCameraDrawThreadPerContext):
        numThreadsOnStartBarrier = cameras.size() + 1;
        numThreadsOnEndBarrier = 1;
        break;
    default:
        osg::notify(osg::NOTICE) << "Error: Threading model not selected" << std::endl;
        return;
    }

    // using multi-threading so make sure that new objects are allocated with thread safe ref/unref
    osg::Referenced::setThreadSafeReferenceCounting(true);

    Scenes scenes;
    getScenes(scenes);
    for (Scenes::iterator scitr = scenes.begin();
         scitr != scenes.end();
         ++scitr)
    {
        if ((*scitr)->getSceneData())
        {
            osg::notify(osg::INFO) << "Making scene thread safe" << std::endl;

            // make sure that existing scene graph objects are allocated with thread safe ref/unref
            (*scitr)->getSceneData()->setThreadSafeRefUnref(true);

            // update the scene graph so that it has enough GL object buffer memory for the graphics contexts that will be using it.
            (*scitr)->getSceneData()->resizeGLObjectBuffers(osg::DisplaySettings::instance()->getMaxNumberOfGraphicsContexts());
        }
    }

    int numProcessors = OpenThreads::GetNumberOfProcessors();
    bool affinity = numProcessors > 1;

    Contexts::iterator citr;

    unsigned int numViewerDoubleBufferedRenderingOperation = 0;

    bool graphicsThreadsDoesCull = _threadingModel == CullDrawThreadPerContext || _threadingModel == SingleThreaded;

    for (Cameras::iterator camItr = cameras.begin();
         camItr != cameras.end();
         ++camItr)
    {
        osg::Camera *camera = *camItr;
        osgViewer::Renderer *renderer = dynamic_cast<osgViewer::Renderer *>(camera->getRenderer());
        if (renderer)
        {
            renderer->setGraphicsThreadDoesCull(graphicsThreadsDoesCull);
            renderer->setDone(false);
            ++numViewerDoubleBufferedRenderingOperation;
        }
    }

    if (_threadingModel == CullDrawThreadPerContext)
    {
        _startRenderingBarrier = 0;
        _endRenderingDispatchBarrier = 0;
        _endDynamicDrawBlock = 0;
    }
    else if (_threadingModel == DrawThreadPerContext || _threadingModel == CullThreadPerCameraDrawThreadPerContext)
    {
        _startRenderingBarrier = 0;
        _endRenderingDispatchBarrier = 0;
        _endDynamicDrawBlock = new osg::EndOfDynamicDrawBlock(numViewerDoubleBufferedRenderingOperation);

#ifndef OSGUTIL_RENDERBACKEND_USE_REF_PTR
        if (!osg::Referenced::getDeleteHandler())
            osg::Referenced::setDeleteHandler(new osg::DeleteHandler(2));
        else
            osg::Referenced::getDeleteHandler()->setNumFramesToRetainObjects(2);
#endif
    }

    if (numThreadsOnStartBarrier > 1)
    {
        _startRenderingBarrier = new osg::BarrierOperation(numThreadsOnStartBarrier, osg::BarrierOperation::NO_OPERATION);
        _startRenderingBarrier->setName("_startRenderingBarrier");
    }

    if (numThreadsOnEndBarrier > 1)
    {
        _endRenderingDispatchBarrier = new osg::BarrierOperation(numThreadsOnEndBarrier, osg::BarrierOperation::NO_OPERATION);
        _endRenderingDispatchBarrier->setName("_endRenderingDispatchBarrier");
    }

    osg::ref_ptr<osg::BarrierOperation> swapReadyBarrier = contexts.empty() ? 0 : new osg::BarrierOperation(contexts.size(), osg::BarrierOperation::NO_OPERATION);
    if (swapReadyBarrier != NULL)
        swapReadyBarrier->setName("swapReadyBarrier");

    osg::ref_ptr<osg::SwapBuffersOperation> swapOp = new osg::SwapBuffersOperation();
    if (swapOp != NULL)
        swapOp->setName("swapOp");

    typedef std::map<OpenThreads::Thread *, int> ThreadAffinityMap;
    ThreadAffinityMap threadAffinityMap;

    // begin OpenCOVER
    osg::BarrierOperation *finishOp = new osg::BarrierOperation(contexts.size(), osg::BarrierOperation::GL_FINISH);
    finishOp->setName("osg::BarrierOperation::GL_FINISH");
    // end OpenCOVER

    unsigned int processNum = 1;
    for (citr = contexts.begin();
         citr != contexts.end();
         ++citr, ++processNum)
    {
        osg::GraphicsContext *gc = (*citr);
        if (!gc->isRealized())
        {
            //OSG_INFO<<"ViewerBase::startThreading() : Realizng window "<<gc<<std::endl;
            gc->realize();
        }

        gc->getState()->setDynamicObjectRenderingCompletedCallback(_endDynamicDrawBlock.get());

        // create the a graphics thread for this context
        gc->createGraphicsThread();

        if (affinity)
            gc->getGraphicsThread()->setProcessorAffinity(-1);
        threadAffinityMap[gc->getGraphicsThread()] = -1;

        // add the startRenderingBarrier
        if (_threadingModel == CullDrawThreadPerContext && _startRenderingBarrier.valid())
            gc->getGraphicsThread()->add(_startRenderingBarrier.get());

        // begin OpenCOVER
        // add the rendering operation itself.
        ViewerRunOperations *vr = new ViewerRunOperations();
        vr->setName("ViewerRunOperations");
        gc->getGraphicsThread()->add(vr);
        // end OpenCOVER

        if (_threadingModel == CullDrawThreadPerContext && _endBarrierPosition == BeforeSwapBuffers && _endRenderingDispatchBarrier.valid())
        {
            // add the endRenderingDispatchBarrier
            gc->getGraphicsThread()->add(_endRenderingDispatchBarrier.get());
        }
        gc->getGraphicsThread()->add(finishOp); // make sure all threads have finished drawing

        PreSwapBuffersOp *pso = new PreSwapBuffersOp(processNum - 1);
        gc->getGraphicsThread()->add(pso);

        // wait on a Barrier so that all rendering threads are ready to swap, otherwise we are not sure that after a remote sync, we can swap rightaway
        if (coVRConfig::instance()->numChannels() > 1)
        {

            if (swapReadyBarrier.valid())
                gc->getGraphicsThread()->add(swapReadyBarrier.get());
        }

        if (processNum == 1) // first context, add sync operation
        {
            RemoteSyncOp *rso = new RemoteSyncOp();
            rso->setName("RemoteSyncOp");
            gc->getGraphicsThread()->add(rso);
        }
        // end OpenCOVER

        if (swapReadyBarrier.valid())
            gc->getGraphicsThread()->add(swapReadyBarrier.get());

        // add the swap buffers
        gc->getGraphicsThread()->add(swapOp.get());

        // begin OpenCOVER
        PostSwapBuffersOp *psbo = new PostSwapBuffersOp(processNum - 1);
        psbo->setName("PostSwapBuffersOp");
        gc->getGraphicsThread()->add(psbo);
        // end OpenCOVER

        if (_threadingModel == CullDrawThreadPerContext && _endBarrierPosition == AfterSwapBuffers && _endRenderingDispatchBarrier.valid())
        {
            // add the endRenderingDispatchBarrier
            gc->getGraphicsThread()->add(_endRenderingDispatchBarrier.get());
        }
    }

    if (_threadingModel == CullThreadPerCameraDrawThreadPerContext && numThreadsOnStartBarrier > 1)
    {
        Cameras::iterator camItr;

        for (camItr = cameras.begin();
             camItr != cameras.end();
             ++camItr, ++processNum)
        {
            osg::Camera *camera = *camItr;
            camera->createCameraThread();

            if (affinity)
                camera->getCameraThread()->setProcessorAffinity(-1);
            threadAffinityMap[camera->getCameraThread()] = -1;

            osg::GraphicsContext *gc = camera->getGraphicsContext();

            // add the startRenderingBarrier
            if (_startRenderingBarrier.valid())
                camera->getCameraThread()->add(_startRenderingBarrier.get());

            osgViewer::Renderer *renderer = dynamic_cast<osgViewer::Renderer *>(camera->getRenderer());
            renderer->setName("Renderer");
            renderer->setGraphicsThreadDoesCull(false);
            camera->getCameraThread()->add(renderer);

            if (_endRenderingDispatchBarrier.valid())
            {
                // add the endRenderingDispatchBarrier
                gc->getGraphicsThread()->add(_endRenderingDispatchBarrier.get());
            }
        }

        for (camItr = cameras.begin();
             camItr != cameras.end();
             ++camItr)
        {
            osg::Camera *camera = *camItr;
            if (camera->getCameraThread() && !camera->getCameraThread()->isRunning())
            {
                osg::notify(osg::INFO) << "  camera->getCameraThread()-> " << camera->getCameraThread() << std::endl;
                camera->getCameraThread()->startThread();
            }
        }
    }

#if 0    
    if (affinity) 
    {
        OpenThreads::SetProcessorAffinityOfCurrentThread(-1);
        if (_scene.valid() && _scene->getDatabasePager())
        {
#if 0        
            _scene->getDatabasePager()->setProcessorAffinity(1);
#else
            _scene->getDatabasePager()->setProcessorAffinity(0);
#endif
        }
    }
#endif

#if 0
    if (affinity)
    {
        for(ThreadAffinityMap::iterator titr = threadAffinityMap.begin();
            titr != threadAffinityMap.end();
            ++titr)
        {
            titr->first->setProcessorAffinity(titr->second);
        }
    }
#endif

    for (citr = contexts.begin();
         citr != contexts.end();
         ++citr)
    {
        osg::GraphicsContext *gc = (*citr);
        if (gc->getGraphicsThread() && !gc->getGraphicsThread()->isRunning())
        {
            osg::notify(osg::INFO) << "  gc->getGraphicsThread()->startThread() " << gc->getGraphicsThread() << std::endl;
            fprintf(stderr,"setProcessorAffinity to %d\n",coVRConfig::instance()->lockToCPU());
            gc->getGraphicsThread()->setProcessorAffinity(coVRConfig::instance()->lockToCPU());
            gc->getGraphicsThread()->startThread();
            // OpenThreads::Thread::YieldCurrentThread();
        }
    }

    _threadsRunning = true;

    osg::notify(osg::INFO) << "Set up threading" << std::endl;
}

void VRViewer::getCameras(Cameras &cameras, bool onlyActive)
{
    cameras.clear();
    if (myPreRenderCameras.size() > 0)
    {
        for (std::list<osg::ref_ptr<osg::Camera> >::iterator itr = myPreRenderCameras.begin();
             itr != myPreRenderCameras.end();
             ++itr)
        {
            if (!onlyActive || (itr->get()->getGraphicsContext() && itr->get()->getGraphicsContext()->valid()))
                cameras.push_back((*itr).get());
        }
    }
    else
    {

        if (_camera.valid() && (!onlyActive || (_camera->getGraphicsContext() && _camera->getGraphicsContext()->valid())))
        {
            cameras.push_back(_camera.get());
        }
        for (std::list<osg::ref_ptr<osg::Camera> >::iterator itr = myCameras.begin();
             itr != myCameras.end();
             ++itr)
        {
            if (!onlyActive || (itr->get()->getGraphicsContext() && itr->get()->getGraphicsContext()->valid()))
                cameras.push_back((*itr).get());
        }
    }
}
//from osgUtil Viewer.cpp
struct LessGraphicsContext
{
    bool operator()(const osg::GraphicsContext *lhs, const osg::GraphicsContext *rhs) const
    {
        int screenLeft = lhs->getTraits() ? lhs->getTraits()->screenNum : 0;
        int screenRight = rhs->getTraits() ? rhs->getTraits()->screenNum : 0;
        if (screenLeft < screenRight)
            return true;
        if (screenLeft > screenRight)
            return false;

        screenLeft = lhs->getTraits() ? lhs->getTraits()->x : 0;
        screenRight = rhs->getTraits() ? rhs->getTraits()->x : 0;
        if (screenLeft < screenRight)
            return true;
        if (screenLeft > screenRight)
            return false;

        screenLeft = lhs->getTraits() ? lhs->getTraits()->y : 0;
        screenRight = rhs->getTraits() ? rhs->getTraits()->y : 0;
        if (screenLeft < screenRight)
            return true;
        if (screenLeft > screenRight)
            return false;

        return lhs < rhs;
    }
};

void VRViewer::getContexts(Contexts &contexts, bool onlyValid)
{
    typedef std::set<osg::GraphicsContext *> ContextSet;
    ContextSet contextSet;
    contexts.clear();
    if (myPreRenderCameras.size() > 0)
    {
        for (std::list<osg::ref_ptr<osg::Camera> >::iterator itr = myPreRenderCameras.begin();
             itr != myPreRenderCameras.end();
             ++itr)
        {
            if ((*itr)->getGraphicsContext() && ((*itr)->getGraphicsContext()->valid() || !onlyValid))
            {
                if (contextSet.find((*itr)->getGraphicsContext()) == contextSet.end()) // only add context if not already in list
                {
                    contextSet.insert((*itr)->getGraphicsContext());
                    contexts.push_back((*itr)->getGraphicsContext());
                }
            }
        }
    }
    else
    {
        if (_camera.valid() && _camera->getGraphicsContext() && (_camera->getGraphicsContext()->valid() || !onlyValid))
        {
            contextSet.insert(_camera->getGraphicsContext());
            contexts.push_back(_camera->getGraphicsContext());
        }

        for (std::list<osg::ref_ptr<osg::Camera> >::iterator itr = myCameras.begin();
             itr != myCameras.end();
             ++itr)
        {
            if ((*itr)->getGraphicsContext() && ((*itr)->getGraphicsContext()->valid() || !onlyValid))
            {
                if (contextSet.find((*itr)->getGraphicsContext()) == contextSet.end()) // only add context if not already in list
                {
                    contextSet.insert((*itr)->getGraphicsContext());
                    contexts.push_back((*itr)->getGraphicsContext());
                }
            }
        }
    }
}

void VRViewer::getScenes(Scenes &scenes, bool /*onlyValid*/)
{
    /*if(myPreRenderCameras.size()>0)
   {
    scenes.clear();
    scenes.push_back(PreRenderScene.get());
   }
   else*/
    {
        scenes.clear();
        scenes.push_back(_scene.get());
    }
}

// OpenCOVER
void VRViewer::addCamera(osg::Camera *camera)
{
    bool threadsWereRuinning = _threadsRunning;
    if (threadsWereRuinning)
        stopThreading();

    myCameras.push_back(camera);

    if (threadsWereRuinning)
        startThreading();
}
// OpenCOVER
void VRViewer::removeCamera(osg::Camera *camera)
{

    bool threadsWereRuinning = _threadsRunning;
    if (threadsWereRuinning)
        stopThreading();

    for (std::list<osg::ref_ptr<osg::Camera> >::iterator itr = myCameras.begin();
         itr != myCameras.end();
         ++itr)
    {
        if (itr->get() == camera)
        {
            itr = myCameras.erase(itr);
            break;
        }
    }

    if (threadsWereRuinning)
        startThreading();
}
// OpenCOVER
void VRViewer::addPreRenderCamera(osg::Camera *camera)
{
    bool threadsWereRuinning = _threadsRunning;
    if (threadsWereRuinning)
        stopThreading();
    //PreRenderScene = new osgViewer::Scene();
    //PreRenderScene->setSceneData(camera->getChild(0));
    myPreRenderCameras.push_back(camera);
    if (threadsWereRuinning)
        startThreading();
}
// OpenCOVER
void VRViewer::removePreRenderCamera(osg::Camera *camera)
{
    bool threadsWereRuinning = _threadsRunning;
    if (threadsWereRuinning)
        stopThreading();
    for (std::list<osg::ref_ptr<osg::Camera> >::iterator itr = myPreRenderCameras.begin();
         itr != myPreRenderCameras.end();
         ++itr)
    {
        if (itr->get() == camera)
        {
            itr = myPreRenderCameras.erase(itr);
            break;
        }
    }
    if (threadsWereRuinning)
        startThreading();
}

void VRViewer::renderingTraversals()
{
    bool _outputMasterCameraLocation = false;
    if (_outputMasterCameraLocation)
    {
        Views views;
        getViews(views);

        for (Views::iterator itr = views.begin();
             itr != views.end();
             ++itr)
        {
            osgViewer::View *view = *itr;
            if (view)
            {
                const osg::Matrixd &m = view->getCamera()->getInverseViewMatrix();
                OSG_NOTICE << "View " << view << ", Master Camera position(" << m.getTrans().x() << "," << m.getTrans().y() << "," << m.getTrans().z() << ","
                           << "), rotation(" << m.getRotate().x() << "," << m.getRotate().y() << "," << m.getRotate().z() << "," << m.getRotate().w() << ","
                           << ")" << std::endl;
            }
        }
    }

    Contexts contexts;
    getContexts(contexts);

    // check to see if windows are still valid
    checkWindowStatus();
    if (_done)
        return;

    double beginRenderingTraversals = elapsedTime();

    osg::FrameStamp *frameStamp = getViewerFrameStamp();

    if (getViewerStats() && getViewerStats()->collectStats("scene"))
    {
        unsigned int frameNumber = frameStamp ? frameStamp->getFrameNumber() : 0;

        Views views;
        getViews(views);
        for (Views::iterator vitr = views.begin();
             vitr != views.end();
             ++vitr)
        {
            View *view = *vitr;
            osg::Stats *stats = view->getStats();
            osg::Node *sceneRoot = view->getSceneData();
            if (sceneRoot && stats)
            {
                osgUtil::StatsVisitor statsVisitor;
                sceneRoot->accept(statsVisitor);
                statsVisitor.totalUpStats();

                unsigned int unique_primitives = 0;
                osgUtil::Statistics::PrimitiveCountMap::iterator pcmitr;
                for (pcmitr = statsVisitor._uniqueStats.GetPrimitivesBegin();
                     pcmitr != statsVisitor._uniqueStats.GetPrimitivesEnd();
                     ++pcmitr)
                {
                    unique_primitives += pcmitr->second;
                }

                stats->setAttribute(frameNumber, "Number of unique StateSet", static_cast<double>(statsVisitor._statesetSet.size()));
                stats->setAttribute(frameNumber, "Number of unique Group", static_cast<double>(statsVisitor._groupSet.size()));
                stats->setAttribute(frameNumber, "Number of unique Transform", static_cast<double>(statsVisitor._transformSet.size()));
                stats->setAttribute(frameNumber, "Number of unique LOD", static_cast<double>(statsVisitor._lodSet.size()));
                stats->setAttribute(frameNumber, "Number of unique Switch", static_cast<double>(statsVisitor._switchSet.size()));
                stats->setAttribute(frameNumber, "Number of unique Geode", static_cast<double>(statsVisitor._geodeSet.size()));
                stats->setAttribute(frameNumber, "Number of unique Drawable", static_cast<double>(statsVisitor._drawableSet.size()));
                stats->setAttribute(frameNumber, "Number of unique Geometry", static_cast<double>(statsVisitor._geometrySet.size()));
                stats->setAttribute(frameNumber, "Number of unique Vertices", static_cast<double>(statsVisitor._uniqueStats._vertexCount));
                stats->setAttribute(frameNumber, "Number of unique Primitives", static_cast<double>(unique_primitives));

                unsigned int instanced_primitives = 0;
                for (pcmitr = statsVisitor._instancedStats.GetPrimitivesBegin();
                     pcmitr != statsVisitor._instancedStats.GetPrimitivesEnd();
                     ++pcmitr)
                {
                    instanced_primitives += pcmitr->second;
                }

                stats->setAttribute(frameNumber, "Number of instanced Stateset", static_cast<double>(statsVisitor._numInstancedStateSet));
                stats->setAttribute(frameNumber, "Number of instanced Group", static_cast<double>(statsVisitor._numInstancedGroup));
                stats->setAttribute(frameNumber, "Number of instanced Transform", static_cast<double>(statsVisitor._numInstancedTransform));
                stats->setAttribute(frameNumber, "Number of instanced LOD", static_cast<double>(statsVisitor._numInstancedLOD));
                stats->setAttribute(frameNumber, "Number of instanced Switch", static_cast<double>(statsVisitor._numInstancedSwitch));
                stats->setAttribute(frameNumber, "Number of instanced Geode", static_cast<double>(statsVisitor._numInstancedGeode));
                stats->setAttribute(frameNumber, "Number of instanced Drawable", static_cast<double>(statsVisitor._numInstancedDrawable));
                stats->setAttribute(frameNumber, "Number of instanced Geometry", static_cast<double>(statsVisitor._numInstancedGeometry));
                stats->setAttribute(frameNumber, "Number of instanced Vertices", static_cast<double>(statsVisitor._instancedStats._vertexCount));
                stats->setAttribute(frameNumber, "Number of instanced Primitives", static_cast<double>(instanced_primitives));
            }
        }
    }

    Scenes scenes;
    getScenes(scenes);

    for (Scenes::iterator sitr = scenes.begin();
         sitr != scenes.end();
         ++sitr)
    {
        osgViewer::Scene *scene = *sitr;
        osgDB::DatabasePager *dp = scene ? scene->getDatabasePager() : 0;
        if (dp)
        {
            dp->signalBeginFrame(frameStamp);
        }

        if (scene->getSceneData())
        {
            // fire off a build of the bounding volumes while we
            // are still running single threaded.
            scene->getSceneData()->getBound();
        }
    }

    // osg::notify(osg::NOTICE)<<std::endl<<"Start frame"<<std::endl;

    Cameras cameras;
    getCameras(cameras);

    Contexts::iterator itr;

    bool doneMakeCurrentInThisThread = false;

    if (_endDynamicDrawBlock.valid())
    {
        _endDynamicDrawBlock->reset();
    }
    // dispatch the the rendering threads
    if (_startRenderingBarrier.valid())
        _startRenderingBarrier->block();

    // reset any double buffer graphics objects
    for (Cameras::iterator camItr = cameras.begin();
         camItr != cameras.end();
         ++camItr)
    {
        osg::Camera *camera = *camItr;
        osgViewer::Renderer *renderer = dynamic_cast<osgViewer::Renderer *>(camera->getRenderer());
        if (renderer)
        {
            if (!renderer->getGraphicsThreadDoesCull() && !(camera->getCameraThread()))
            {
                renderer->cull();
            }
        }
    }

    for (itr = contexts.begin();
         itr != contexts.end();
         ++itr)
    {
        if (_done)
            return;
        if (!((*itr)->getGraphicsThread()) && (*itr)->valid())
        {
            doneMakeCurrentInThisThread = true;
            makeCurrent(*itr);
            (*itr)->runOperations();
        }
    }

    // osg::notify(osg::NOTICE)<<"Joing _endRenderingDispatchBarrier block "<<_endRenderingDispatchBarrier.get()<<std::endl;

    // wait till the rendering dispatch is done.
    if (_endRenderingDispatchBarrier.valid())
        _endRenderingDispatchBarrier->block();

    // begin OpenCOVER
    bool sync = false;
    for (itr = contexts.begin();
         itr != contexts.end();
         ++itr)
    {
        if (_done)
            return;

        if (!((*itr)->getGraphicsThread())) // no graphics thread, so we need to sync manually
        {

            doneMakeCurrentInThisThread = true;
            makeCurrent(*itr);
            static int numClears = 0;
            if (VRViewer::instance()->clearWindow || numClears > 0)
            {
                VRViewer::instance()->clearWindow = false;
                if (numClears == 0)
                {
                    numClears = coVRConfig::instance()->numWindows() * 2;
                }
                numClears--;
                glDrawBuffer(GL_BACK);
                glViewport(0, 0, 1, 1);
                glScissor(0, 0, 1, 1);
                glClearColor(0.0, 0.0, 0.0, 1.0);
                glClear(GL_COLOR_BUFFER_BIT);
                glDrawBuffer(GL_FRONT);
                glViewport(0, 0, 1, 1);
                glScissor(0, 0, 1, 1);
                glClearColor(0.0, 0.0, 0.0, 1.0);
                glClear(GL_COLOR_BUFFER_BIT);
                makeCurrent(*itr);
            }
            //cerr << "finish" << endl;
            glFinish();
            sync = true;
        }
    }

    if (OpenCOVER::instance()->initDone())
    {
        int WindowNum = 0;
        for (itr = contexts.begin();
             itr != contexts.end();
             ++itr)
        {
            if (!((*itr)->getGraphicsThread()) && (*itr)->valid())
            {
                doneMakeCurrentInThisThread = true;
                makeCurrent(*itr);
                coVRPluginList::instance()->preSwapBuffers(WindowNum);
                WindowNum++;
            }
        }
    }

    if (sync)
    {
        //do sync here before swapBuffers

        // sync multipc instances
        if (VRViewer::unsyncedFrames <= 0)
        {
            double beginSync = elapsedTime();
            coVRMSController::instance()->syncDraw();
            double endSync = elapsedTime();
            getStats()->setAttribute(frameStamp->getFrameNumber(), "sync begin time ", beginSync);
            getStats()->setAttribute(frameStamp->getFrameNumber(), "sync end time ", endSync);
            getStats()->setAttribute(frameStamp->getFrameNumber(), "sync time taken", endSync - beginSync);
        }
        else
            VRViewer::unsyncedFrames--;
    }
    double beginSwap = elapsedTime();
    // end OpenCOVER
    for (itr = contexts.begin();
         itr != contexts.end();
         ++itr)
    {
        if (_done)
            return;

        if (!((*itr)->getGraphicsThread()) && (*itr)->valid())
        {
            doneMakeCurrentInThisThread = true;
            makeCurrent(*itr);
            (*itr)->swapBuffers();
        }
    }
    double endSwap = elapsedTime();
    getStats()->setAttribute(frameStamp->getFrameNumber(), "swap begin time ", beginSwap);
    getStats()->setAttribute(frameStamp->getFrameNumber(), "swap end time ", endSwap);
    getStats()->setAttribute(frameStamp->getFrameNumber(), "swap time taken", endSwap - beginSwap);

    if (OpenCOVER::instance()->initDone())
    {
        int WindowNum = 0;
        for (itr = contexts.begin();
             itr != contexts.end();
             ++itr)
        {
            if (!((*itr)->getGraphicsThread()) && (*itr)->valid())
            {
                doneMakeCurrentInThisThread = true;
                makeCurrent(*itr);
                coVRPluginList::instance()->postSwapBuffers(WindowNum);
                WindowNum++;
            }
        }
    }

    for (Scenes::iterator sitr = scenes.begin();
         sitr != scenes.end();
         ++sitr)
    {
        osgViewer::Scene *scene = *sitr;
        osgDB::DatabasePager *dp = scene ? scene->getDatabasePager() : 0;
        if (dp)
        {
            dp->signalEndFrame();
        }
    }

    // wait till the dynamic draw is complete.
    if (_endDynamicDrawBlock.valid())
    {
        // osg::Timer_t startTick = osg::Timer::instance()->tick();
        _endDynamicDrawBlock->block();
        // osg::notify(osg::NOTICE)<<"Time waiting "<<osg::Timer::instance()->delta_m(startTick, osg::Timer::instance()->tick())<<std::endl;;
    }

    if (_releaseContextAtEndOfFrameHint && doneMakeCurrentInThisThread)
    {
        //OSG_NOTICE<<"Doing release context"<<std::endl;
        releaseContext();
    }

    if (getStats() && getStats()->collectStats("update"))
    {
        double endRenderingTraversals = elapsedTime();

        // update current frames stats
        getStats()->setAttribute(frameStamp->getFrameNumber(), "Rendering traversals begin time ", beginRenderingTraversals);
        getStats()->setAttribute(frameStamp->getFrameNumber(), "Rendering traversals end time ", endRenderingTraversals);
        getStats()->setAttribute(frameStamp->getFrameNumber(), "Rendering traversals time taken", endRenderingTraversals - beginRenderingTraversals);
    }

    _requestRedraw = false;
}

void VRViewer::statistics(bool enable)
{

    //simulate key press
    osgGA::GUIEventAdapter *ea = new osgGA::GUIEventAdapter;
    ea->setEventType(osgGA::GUIEventAdapter::KEYDOWN);
    ea->setKey(statsHandler->getKeyEventTogglesOnScreenStats());
    static bool oldEnable = false;
    if (oldEnable != enable)
    {
        if (enable)
        {
            //on
            statsHandler->handle(*ea, *this);
            //  statsHandler->handle(*ea,*this);
        }
        else
        {
            //off
            statsHandler->handle(*ea, *this);
        }
    }
    oldEnable = enable;
}

void
VRViewer::statisticsCallback(void *, buttonSpecCell *spec)
{
    coVRConfig::instance()->drawStatistics = spec->state != 0.0;
    VRViewer::instance()->statistics(coVRConfig::instance()->drawStatistics);
    //XXX VRViewer::instance()->setInstrumentationMode( coVRConfig::instance()->drawStatistics );
}

template <typename Cameras, typename F>
osg::Node::NodeMask getCullMaskImpl(Cameras const& cameras, F func)
{
    if (cameras.size() == 0)
    {
        return 0x0;
    }

    typename Cameras::const_iterator camItr = cameras.begin();
    osg::Node::NodeMask result = ((*camItr)->*func)();
    for (;
         camItr != cameras.end();
         ++camItr)
    {
        assert( (result & ((*camItr)->*func)()) == result );
    }

    return result;
}

osg::Node::NodeMask
VRViewer::getCullMask() /*const*/
{
    Cameras cameras;
    getCameras(cameras);
    return getCullMaskImpl(cameras, &osg::Camera::getCullMask);
}

osg::Node::NodeMask
VRViewer::getCullMaskLeft() /*const*/
{
    Cameras cameras;
    getCameras(cameras);
    return getCullMaskImpl(cameras, &osg::Camera::getCullMaskLeft);
}

osg::Node::NodeMask
VRViewer::getCullMaskRight() /*const*/
{
    Cameras cameras;
    getCameras(cameras);
    return getCullMaskImpl(cameras, &osg::Camera::getCullMaskRight);
}

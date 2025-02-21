/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#include <cover/coVRPluginSupport.h>
#include <cover/coVRNavigationManager.h>
#include <OpenVRUI/coTrackerButtonInteraction.h>
#include <OpenVRUI/osg/mathUtils.h>

#include <cover/mui/Tab.h>
#include <cover/mui/ToggleButton.h>

#include <osg/MatrixTransform>
#include <osg/Geode>
#include <osg/ClipNode>
#include <osg/Geometry>
#include <osg/Material>
#include <osg/LineWidth>

#include "ClipPlane.h"

#include <PluginUtil/PluginMessageTypes.h>

#include <config/CoviseConfig.h>

#include <net/message.h>
#include <net/tokenbuffer.h>

using namespace osg;
using covise::coCoviseConfig;

void ClipPlanePlugin::message(int type, int len, const void *buf)
{
    if (type == 1 && len == 1)
    {
        int planeNumber = atoi((const char *)buf);
        plane[planeNumber].enabled = !plane[planeNumber].enabled;
        plane[planeNumber].EnableButton->setState(plane[planeNumber].enabled);
        return;
    }

    if (type != PluginMessageTypes::ClipPlaneMessage)
        return;

    int planeNumber = 0;
    Vec4d eq;

    const char *number = (const char *)buf;
    if (!strncasecmp(number, "set", 3))
    {
        sscanf(number, "set %d %lf %lf %lf %lf", &planeNumber, &eq[0], &eq[1], &eq[2], &eq[3]);

        plane[planeNumber].clip->setClipPlane(eq);
        plane[planeNumber].valid = true;
    }
    else if (!strncasecmp(number, "enablePick", 10))
    {
        planeNumber = atoi(&number[11]);
        plane[planeNumber].enabled = true;
        cover->getObjectsRoot()->addClipPlane(plane[planeNumber].clip.get());
        plane[planeNumber].PickInteractorButton->setState(true);
        plane[planeNumber].EnableButton->setState(true);
    }
    else if (!strncasecmp(number, "disablePick", 11))
    {
        planeNumber = atoi(&number[12]);
        plane[planeNumber].enabled = false;
        cover->getObjectsRoot()->removeClipPlane(plane[planeNumber].clip.get());
        plane[planeNumber].PickInteractorButton->setState(false);
        plane[planeNumber].EnableButton->setState(true);
        muiEvent(plane[planeNumber].PickInteractorButton);
    }
    else if (!strncasecmp(number, "enable", 6))
    {
        planeNumber = atoi(&number[7]);
        if (!plane[planeNumber].enabled)
        {
            plane[planeNumber].enabled = true;
            cover->getObjectsRoot()->addClipPlane(plane[planeNumber].clip.get());
            plane[planeNumber].EnableButton->setState(true);
        }
    }
    else if (!strncasecmp(number, "disable", 7))
    {
        planeNumber = atoi(&number[8]);
        if (plane[planeNumber].enabled)
        {
            plane[planeNumber].enabled = false;
            cover->getObjectsRoot()->removeClipPlane(plane[planeNumber].clip.get());
            plane[planeNumber].EnableButton->setState(false);
        }
    }
    else
    {

        covise::TokenBuffer tb(number, len);
        tb >> planeNumber;

        for (int i = 0; i < 4; i++)
            tb >> eq[i];
        //fprintf(stderr,"ClipPlanePlugin::message for plane %d eq=[%f %f %f %f]\n", planeNumber, eq[0], eq[1], eq[2], eq[3]);

        plane[planeNumber].clip->setClipPlane(eq);
        plane[planeNumber].valid = true;
    }
}

ClipPlanePlugin::ClipPlanePlugin()
{
}

bool ClipPlanePlugin::init()
{
    clipTab = mui::Tab::create("plugins.general.ClipPlane.ClipTab");
    clipTab->setLabel("ClipPlane");
    clipTab->setEventListener(this);
    clipTab->setPos(0, 0);

    for (int i = 0; i < cover->getNumClipPlanes(); i++)
    {
        char name[100];

        sprintf(name, "ClipPlane %d PickInteractor", i);
        plane[i].PickInteractorButton = mui::ToggleButton::create(std::string("plugins.general.ClipPlane")+name, clipTab);
        plane[i].PickInteractorButton->setLabel(std::string(name));
        plane[i].PickInteractorButton->setEventListener(this);
        plane[i].PickInteractorButton->setPos(0, i);

        sprintf(name, "ClipPlane %d DirectInteractor", i);
        plane[i].DirectInteractorButton = mui::ToggleButton::create(std::string("plugins.general.ClipPlane")+name, clipTab);
        plane[i].DirectInteractorButton->setLabel(std::string(name));
        plane[i].DirectInteractorButton->setEventListener(this);
        plane[i].DirectInteractorButton->setPos(1, i);

        sprintf(name, "ClipPlane %d enable", i);
        plane[i].EnableButton = mui::ToggleButton::create(std::string("plugins.general.ClipPlane")+name, clipTab);
        plane[i].EnableButton->setLabel(std::string(name));
        plane[i].EnableButton->setEventListener(this);
        plane[i].EnableButton->setPos(2, i);

        plane[i].directInteractor = new coTrackerButtonInteraction(coInteraction::ButtonA, "sphere");

        osg::Matrix m;
        // default size for all interactors
        float interSize = -1.f;
        // if defined, COVER.IconSize overrides the default
        interSize = coCoviseConfig::getFloat("COVER.IconSize", interSize);
        // if defined, COVERConfigCuttingSurfacePlugin.IconSize overrides both
        interSize = coCoviseConfig::getFloat("COVER.Plugin.Cuttingsurface.IconSize", interSize);
        plane[i].pickInteractor = new coVR3DTransRotInteractor(m, interSize, coInteraction::ButtonA, "hand", "ClipPlane", coInteraction::Medium);
        plane[i].pickInteractor->hide();

        plane[i].clip = cover->getClipPlane(i);
    }

    active = false;
    cover->setActiveClippingPlane(0);

    // get the transform node of the hand device
    pointerTransform = cover->getPointer();
    interactorTransform = new MatrixTransform;
    cover->getScene()->addChild(interactorTransform.get());

    // load the visible geometry (transparent rectangle)
    visibleClipPlaneGeode = loadPlane();

    return true;
}

ClipPlanePlugin::~ClipPlanePlugin()
{
    cover->getScene()->removeChild(interactorTransform.get());

    ClipNode *clipNode = cover->getObjectsRoot();
    for (int i = 0; i < cover->getNumClipPlanes(); i++)
    {
        if (plane[i].clip.get())
        {
            clipNode->removeClipPlane(plane[i].clip.get());
        }
    }

    delete clipTab;
}

Vec4d ClipPlanePlugin::matrixToEquation(const Matrix &mat)
{
    Vec3 pos(mat(3, 0), mat(3, 1), mat(3, 2));
    Vec3 normal(mat(1, 0), mat(1, 1), mat(1, 2));
    normal.normalize();
    double distance = -pos * normal;
    return Vec4d(normal[0], normal[1], normal[2], distance);
}

void ClipPlanePlugin::preFrame()
{

    for (int i = 0; i < cover->getNumClipPlanes(); i++)
    {

        // pick interaction update
        plane[i].pickInteractor->preFrame();

        // directInteraction started
        if (plane[i].directInteractor && plane[i].directInteractor->wasStarted())
        {
            interactorTransform->addChild(visibleClipPlaneGeode.get());
            cover->setActiveClippingPlane(i);
            if (plane[i].showPickInteractor_)
            {
                plane[i].pickInteractor->hide();
                plane[i].pickInteractor->disableIntersection();
            }
        }

        // pick interaction started
        if (plane[i].pickInteractor->wasStarted())
        {
            interactorTransform->addChild(visibleClipPlaneGeode.get());
            cover->setActiveClippingPlane(i);
        }

        // direct interaction started or stopped or running
        if (plane[i].directInteractor && !plane[i].directInteractor->isIdle())
        {

            // get the transformation matrix of the transform
            pointerMatrix = pointerTransform->getMatrix();
            if (coVRNavigationManager::instance()->isSnapping())
            {
                Matrix w_to_o = cover->getInvBaseMat();
                pointerMatrix.postMult(w_to_o);
                if (!coVRNavigationManager::instance()->isDegreeSnapping())
                    snapTo45Degrees(&pointerMatrix);
                else
                    snapToDegrees(coVRNavigationManager::instance()->snappingDegrees(), &pointerMatrix);
                Matrix o_to_w = cover->getBaseMat();
                pointerMatrix.postMult(o_to_w);
                coCoord coord = pointerMatrix;
                coord.makeMat(pointerMatrix);
            }
            // rotate geometry
            interactorTransform->setMatrix(pointerMatrix);

            covise::TokenBuffer tb;

            tb << cover->getActiveClippingPlane();

            Vec4d eq;
            Matrix pointerMatrix_w(pointerMatrix);
            Matrix pointerMatrix_o;
            pointerMatrix_o = pointerMatrix_w * cover->getInvBaseMat();

            coCoord coord = pointerMatrix_o;
            coord.makeMat(pointerMatrix_o);

            eq = matrixToEquation(pointerMatrix_o);

            for (int j = 0; j < 4; j++)
                tb << eq[j];

            cover->sendMessage(this, coVRPluginSupport::TO_SAME,
                               PluginMessageTypes::ClipPlaneMessage,
                               tb.get_length(), tb.get_data());

            plane[i].pickInteractor->updateTransform(pointerMatrix_o);
        }

        // pick interaction started or stopped or running
        if (!plane[i].pickInteractor->isIdle())
        {
            Matrix m_o = plane[i].pickInteractor->getMatrix();

            //fprintf(stderr,"pickInteractor m:\n");
            //for (int i=0; i<4; i++)
            //   fprintf(stderr,"%f %f %f %f\n", m_o(i,0),m_o(i,1), m_o(i,2), m_o(i,3));

            Matrix o_to_w = cover->getBaseMat();
            Matrix m_w;
            m_w = m_o * o_to_w;
            coCoord coord = m_w;
            coord.makeMat(m_w);
            interactorTransform->setMatrix(m_w);

            covise::TokenBuffer tb;

            tb << cover->getActiveClippingPlane();

            Vec4d eq;
            eq = matrixToEquation(m_o);
            for (int j = 0; j < 4; j++)
                tb << eq[j];

            cover->sendMessage(this, coVRPluginSupport::TO_SAME,
                               PluginMessageTypes::ClipPlaneMessage,
                               tb.get_length(), tb.get_data());
        }

        // direct interaction stopped
        if (plane[i].directInteractor && plane[i].directInteractor->wasStopped())
        {
            interactorTransform->removeChild(visibleClipPlaneGeode.get());
            if (plane[i].showPickInteractor_)
            {
                plane[i].pickInteractor->show();
                plane[i].pickInteractor->enableIntersection();
            }
        }

        // pick interaction stopped
        if (plane[i].pickInteractor->wasStopped())
        {
            interactorTransform->removeChild(visibleClipPlaneGeode.get());
        }
    }
}

/*void ClipPlanePlugin::menuEvent(coMenuItem *item)
{
    ClipNode *clipNode = cover->getObjectsRoot();

    for (int i = 0; i < cover->getNumClipPlanes(); i++)
    {

        // enabled checkbox was clicked
        if (item == plane[i].vruiEnableCheckbox)
        {
            if (plane[i].vruiEnableCheckbox->getState())
            {
                plane[i].enabled = true;
                clipNode->addClipPlane(plane[i].clip.get());
            }
            else
            {
                plane[i].enabled = false;
                clipNode->removeClipPlane(plane[i].clip.get());
            }

            if (plane[i].tuiEnableButton)
            {
                plane[i].tuiEnableButton->setState(plane[i].enabled);
            }
        }

        // direct interactor checkbox was cklicked
        else if (item == plane[i].vruiDirectInteractorCheckbox)
        {
            if (plane[i].vruiDirectInteractorCheckbox->getState())
            {
                plane[i].showDirectInteractor_ = true;
                if (!plane[i].directInteractor->isRegistered())
                {
                    fprintf(stderr, "registerInteraction\n");
                    coInteractionManager::the()->registerInteraction(plane[i].directInteractor);
                    plane[i].enabled = true;
                    clipNode->addClipPlane(plane[i].clip.get());
                    plane[i].vruiEnableCheckbox->setState(true);
                }
            }
            else
            {
                plane[i].showDirectInteractor_ = false;
                if (plane[i].directInteractor->isRegistered())
                {
                    fprintf(stderr, "unregisterInteraction\n");
                    coInteractionManager::the()->unregisterInteraction(plane[i].directInteractor);
                }
            }
        }
        // pick interactor checkbox was cklicked
        else if (item == plane[i].vruiPickInteractorCheckbox)
        {
            if (plane[i].vruiPickInteractorCheckbox->getState())
            {
                plane[i].showPickInteractor_ = true;
                plane[i].pickInteractor->show();
                plane[i].pickInteractor->enableIntersection();

                plane[i].enabled = true;
                clipNode->addClipPlane(plane[i].clip.get());
                plane[i].vruiEnableCheckbox->setState(true);

                if (!plane[i].valid)
                {
                    setInitialEquation(i);
                }
            }
            else
            {
                plane[i].showPickInteractor_ = false;
                plane[i].pickInteractor->hide();
            }
        }
    }
}*/

void ClipPlanePlugin::muiEvent(mui::Element *muiItem)
{
    ClipNode *clipNode = cover->getObjectsRoot();

    for (int i = 0; i < cover->getNumClipPlanes(); i++)
    {
        // enable button was clicked
        if (muiItem == plane[i].EnableButton)
        {
            bool state = plane[i].EnableButton->getState();
            if (state)
            {
                plane[i].enabled = true;
                clipNode->addClipPlane(plane[i].clip.get());
            }
            else
            {
                plane[i].enabled = false;
                clipNode->removeClipPlane(plane[i].clip.get());
            }
        }
        else if (muiItem == plane[i].DirectInteractorButton)
        {
            bool state = plane[i].DirectInteractorButton->getState();
            if (state)
            {
                plane[i].showDirectInteractor_ = true;
                if (!plane[i].directInteractor->isRegistered())
                {
                    coInteractionManager::the()->registerInteraction(plane[i].directInteractor);
                    plane[i].enabled = true;
                    clipNode->addClipPlane(plane[i].clip.get());
                }
            }
            else
            {
                plane[i].showDirectInteractor_ = false;
                if (plane[i].directInteractor->isRegistered())
                {
                    coInteractionManager::the()->unregisterInteraction(plane[i].directInteractor);
                }
            }
            if (plane[i].DirectInteractorButton)
            {
                plane[i].DirectInteractorButton->setState(state);
            }
        }
        else if (muiItem == plane[i].PickInteractorButton)
        {
            bool state = plane[i].PickInteractorButton->getState();
            if (state)
            {
                plane[i].showPickInteractor_ = true;
                plane[i].pickInteractor->show();
                plane[i].pickInteractor->enableIntersection();

                plane[i].enabled = true;
                clipNode->addClipPlane(plane[i].clip.get());

                if (!plane[i].valid)
                {
                    setInitialEquation(i);
                }
            }
            else
            {
                plane[i].showPickInteractor_ = false;
                plane[i].pickInteractor->hide();
            }
        }
    }
}

void ClipPlanePlugin::muiPressEvent(mui::Element *)
{
}

void ClipPlanePlugin::muiReleaseEvent(mui::Element *)
{
}

Geode *ClipPlanePlugin::loadPlane()
{

    // *5---*6---*7
    // |    |    |
    // *3--------*4
    // |    |    |
    // *0---*1---*2

    float w = cover->getSceneSize() * 0.1; // width of plane

    Vec3Array *lineCoords = new Vec3Array(12);
    (*lineCoords)[0].set(-w, -0.01, -w);
    (*lineCoords)[1].set(w, -0.01, -w);
    (*lineCoords)[2].set(-w, -0.01, 0.0f);
    (*lineCoords)[3].set(w, -0.01, 0.0f);
    (*lineCoords)[4].set(-w, -0.01, w);
    (*lineCoords)[5].set(w, -0.01, w);
    (*lineCoords)[6].set(-w, -0.01, -w);
    (*lineCoords)[7].set(-w, -0.01, w);
    (*lineCoords)[8].set(0.0f, -0.01, -w);
    (*lineCoords)[9].set(0.0f, -0.01, w);
    (*lineCoords)[10].set(w, -0.01, -w);
    (*lineCoords)[11].set(w, -0.01, w);

    DrawArrayLengths *primitives = new DrawArrayLengths(PrimitiveSet::LINE_STRIP);
    for (int i = 0; i < 6; i++)
    {
        primitives->push_back(2);
    }

    Vec3Array *lineColors = new Vec3Array(12);
    for (int i = 0; i < 12; i++)
    {
        (*lineColors)[i].set(Vec3(1.0f, 1.0f, 1.0f));
    }

    Geometry *geoset = new Geometry();
    geoset->setVertexArray(lineCoords);
    geoset->addPrimitiveSet(primitives);
    geoset->setColorArray(lineColors);

    Material *mtl = new Material;
    mtl->setColorMode(Material::AMBIENT_AND_DIFFUSE);
    mtl->setAmbient(Material::FRONT_AND_BACK, Vec4(0.2f, 0.2f, 0.2f, 1.0f));
    mtl->setDiffuse(Material::FRONT_AND_BACK, Vec4(0.9f, 0.9f, 0.9f, 1.0f));
    mtl->setSpecular(Material::FRONT_AND_BACK, Vec4(0.9f, 0.9f, 0.9f, 1.0f));
    mtl->setEmission(Material::FRONT_AND_BACK, Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    mtl->setShininess(Material::FRONT_AND_BACK, 16.0f);

    Geode *geode = new Geode;
    geode->setName("ClipPlane");
    geode->addDrawable(geoset);

    StateSet *geostate = geode->getOrCreateStateSet();
    geostate->setAttributeAndModes(mtl, StateAttribute::ON);
    geostate->setMode(GL_LIGHTING, StateAttribute::OFF);
    LineWidth *lineWidth = new LineWidth(3.0);
    geostate->setAttributeAndModes(lineWidth, StateAttribute::ON);
    geode->setStateSet(geostate);

    return geode;
}

void ClipPlanePlugin::setInitialEquation(int i)
{
    // initialize clipPlanes
    Matrix m;
    Vec4d eq;

    BoundingBox bb = cover->getBBox(cover->getObjectsRoot());
    float d = 0.0;
    if (bb.valid())
    {
        d = 0.25 * (bb._max[0] - bb._min[0]);
    }

    if (i == 0)
    {
        m.makeRotate(-1.5, 0, 0, 1);
        m.setTrans(-d, 0, 0);
        eq = matrixToEquation(m);
        plane[0].clip->setClipPlane(eq);
        plane[0].pickInteractor->updateTransform(m);
    }
    else if (i == 1)
    {
        m.makeRotate(1.5, 0, 0, 1);
        m.setTrans(d, 0, 0);
        eq = matrixToEquation(m);
        plane[1].clip->setClipPlane(eq);
        plane[1].pickInteractor->updateTransform(m);
    }
    else if (i == 1)
    {
        m.makeRotate(0, 0, 0, 1);
        m.setTrans(0, -d, 0);
        eq = matrixToEquation(m);
        plane[2].clip->setClipPlane(eq);
        plane[2].pickInteractor->updateTransform(m);
    }
    else if (i == 3)
    {
        m.makeRotate(3, 0, 0, 1);
        m.setTrans(0, d, 0);
        eq = matrixToEquation(m);
        plane[3].clip->setClipPlane(eq);
        plane[3].pickInteractor->updateTransform(m);
    }

    else if (i == 4)
    {
        m.makeRotate(1.5, 1, 0, 0);
        m.setTrans(0, 0, -d);
        eq = matrixToEquation(m);
        plane[4].clip->setClipPlane(eq);
        plane[4].pickInteractor->updateTransform(m);
    }
    else if (i == 5)
    {
        m.makeRotate(-1.5, 1, 0, 0);
        m.setTrans(0, 0, d);
        eq = matrixToEquation(m);
        plane[5].clip->setClipPlane(eq);
        plane[5].pickInteractor->updateTransform(m);
    }
}
COVERPLUGIN(ClipPlanePlugin)

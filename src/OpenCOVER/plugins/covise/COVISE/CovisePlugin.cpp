/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#include <cover/coInteractor.h>
#include "VRCoviseObjectManager.h"
#include "VRCoviseConnection.h"
#include "coVRMenuList.h"
#include "CovisePlugin.h"
#include <net/message.h>
#include <util/coTimer.h>
#include <covise/covise_appproc.h>
#include <appl/RenderInterface.h>
#include <cover/VRPinboard.h>
#include <cover/VRSceneGraph.h>
#include <OpenVRUI/sginterface/vruiButtons.h>

#include "VRSlider.h"
#include "VRRotator.h"
#include "VRVectorInteractor.h"
#include "coVRTUIParam.h"
#include "coVRDistributionManager.h"

#include <cover/coVRMSController.h>
#include <cover/coVRPluginSupport.h>

#include <cover/coVRMSController.h>
#include <cover/coVRPluginSupport.h>
#include <cover/coVRPluginList.h>

using namespace covise;
using namespace opencover;
using namespace std;
using vrui::vruiButtons;

CovisePlugin::CovisePlugin()
{
    std::cerr << "Starting COVISE connection..." << std::endl;
    new VRCoviseConnection();
}

static void initPinboard(VRPinboard *pb)
{
    pb->addFunction("Execute", VRPinboard::BTYPE_FUNC, "execute", "COVISE", VRCoviseConnection::executeCallback, VRCoviseConnection::covconn);
    pb->addFunction("CuttingSurface", VRPinboard::BTYPE_NAVGROUP, "CuttingSurface", "COVISE", ObjectManager::feedbackCallback, ObjectManager::instance());
    pb->addFunction("CutGeometry", VRPinboard::BTYPE_NAVGROUP, "CutGeometry", "COVISE", ObjectManager::feedbackCallback, ObjectManager::instance());
    pb->addFunction("Isosurface", VRPinboard::BTYPE_NAVGROUP, "IsoSurface", "COVISE", ObjectManager::feedbackCallback, ObjectManager::instance());
    pb->addFunction("Tracer", VRPinboard::BTYPE_NAVGROUP, "Tracer", "COVISE", ObjectManager::feedbackCallback, ObjectManager::instance());
    pb->addFunction("TracerUSG", VRPinboard::BTYPE_NAVGROUP, "TracerUsg", "COVISE", ObjectManager::feedbackCallback, ObjectManager::instance());
    pb->addFunction("Stracer", VRPinboard::BTYPE_NAVGROUP, "STracer", "COVISE", ObjectManager::feedbackCallback, ObjectManager::instance());
    pb->addFunction("TracerSTR", VRPinboard::BTYPE_NAVGROUP, "TracerStr", "COVISE", ObjectManager::feedbackCallback, ObjectManager::instance());
    pb->addFunction("TetraTrace", VRPinboard::BTYPE_NAVGROUP, "TetraTrace", "COVISE", ObjectManager::feedbackCallback, ObjectManager::instance());
    pb->addFunction("MagTracer", VRPinboard::BTYPE_NAVGROUP, "MagTracer", "COVISE", ObjectManager::feedbackCallback, ObjectManager::instance());
    pb->addFunction("MagBlockTracer", VRPinboard::BTYPE_NAVGROUP, "MagBlockTracer", "COVISE", ObjectManager::feedbackCallback, ObjectManager::instance());
    pb->addFunction("CellTracer", VRPinboard::BTYPE_NAVGROUP, "CellTracer", "COVISE", ObjectManager::feedbackCallback, ObjectManager::instance());
    pb->addFunction("LTracer", VRPinboard::BTYPE_NAVGROUP, "LTracer", "COVISE", ObjectManager::feedbackCallback, ObjectManager::instance());

    //void configInteractionPinboard() {
    // now we make the EXECUTE button
    if (VRCoviseConnection::covconn)
    {
        int i = pb->getIndex("EXECUTE");

        if (!pb->customPinboard)
        {
            pb->functions[i].isInPinboard = true;
            pb->makeButton(pb->functions[i].functionName, pb->functions[i].defButtonName,
                           pb->functions[i].defMenuName, pb->functions[i].functionType,
                           pb->functions[i].callback, pb->functions[i].callbackClass);
        }
        else
        {
            if (pb->functions[i].isInPinboard)
                pb->makeButton(pb->functions[i].functionName,
                               pb->functions[i].customButtonName,
                               pb->functions[i].customMenuName, pb->functions[i].functionType,
                               pb->functions[i].callback, pb->functions[i].callbackClass);
        }
    }
}

static void messageCallback(int len, const void *buf)
{
    coVRPluginList::instance()->forwardMessage(len, buf);
}

bool CovisePlugin::init()
{
    coVRDistributionManager::instance().init();
    initPinboard(VRPinboard::instance());
    CoviseRender::set_render_module_callback(messageCallback);
    return VRCoviseConnection::covconn;
}

CovisePlugin::~CovisePlugin()
{
    delete VRCoviseConnection::covconn;
    VRCoviseConnection::covconn = NULL;
}

static void updateScenegraph()
{
    SliderList::instance()->update();
    RotatorList::instance()->update();
    coVRMenuList::instance()->update();

    if (VRSceneGraph::instance()->m_vectorInteractor && !cover->isPointerLocked())
    {
        float position[3];
        float direction[3];
        float direction2[3];
        // retrieve pointer coordinates
        VRSceneGraph::instance()->getHandWorldPosition(position, direction, direction2);

        coPointerButton *button = cover->getPointerButton();
        if ((button->getState() & vruiButtons::ACTION_BUTTON) && (button->wasPressed() || VRSceneGraph::instance()->m_oldHandLocked))
        {

            VectorInteractor::vector = vectorList.find(position[0], position[1], position[2]);
            if (VectorInteractor::vector)
            {
                VectorInteractor::vector->updateValue(position[0], position[1], position[2]);
            }
        }
        else if (button->getState() & vruiButtons::ACTION_BUTTON)
        {
            if (VectorInteractor::vector)
            {
                VectorInteractor::vector->updateValue(position[0], position[1], position[2]);
            }
        }
        else if (button->wasReleased() && (button->oldState() & vruiButtons::ACTION_BUTTON))
        {
            if (VectorInteractor::vector)
            {
                VectorInteractor::vector->updateValue(position[0], position[1], position[2]);
                VectorInteractor::vector->updateParameter();
                VectorInteractor::vector = NULL;
            }
        }
    }
}

void CovisePlugin::preFrame()
{
    VRCoviseConnection::covconn->update();
    updateScenegraph();
}

void CovisePlugin::requestQuit(bool killSession)
{
    if (coVRMSController::instance()->isMaster())
    {
        // send DEL to controller to do a clean exit
        if (VRCoviseConnection::covconn)
        {
            VRCoviseConnection::covconn->sendQuit();
        }

        if (killSession)
            CoviseRender::send_quit_message();
    }
}

void CovisePlugin::removeNode(osg::Node *group, bool isGroup, osg::Node *node)
{
    (void)group;

    // DON'T FORGETT TO UPDATE ROTATOR-STUFF !!!
    if (RotatorList::instance()->find(node))
        RotatorList::instance()->remove();

    // remove all sliders attached to this geometry
    SliderList::instance()->removeAll(node);
    // remove all vectors attached to this geometry
    vectorList.removeAll(node);

    // remove all vectors attached to this geometry
    tuiParamList.removeAll(node);
}

bool CovisePlugin::sendVisMessage(const Message *msg)
{
    if (CoviseRender::appmod)
    {
        if (coVRMSController::instance()->isMaster())
        {
            CoviseRender::appmod->send_ctl_msg(msg);
        }
        return true;
    }
    return false;
}

bool CovisePlugin::becomeCollaborativeMaster()
{
    if ((VRCoviseConnection::covconn))
    {
        if (coVRMSController::instance()->isMaster())
            CoviseRender::send_ui_message("FORCE_MASTER", CoviseRender::get_host());
        return true;
    }
    return false;
}

bool CovisePlugin::executeAll()
{
    if ((VRCoviseConnection::covconn))
    {
        if (coVRMSController::instance()->isMaster())
            VRCoviseConnection::covconn->executeCallback(NULL, NULL);
        return true;
    }
    return false;
}

covise::Message *CovisePlugin::waitForVisMessage(int type)
{
    return CoviseRender::appmod->wait_for_msg(type, CoviseRender::appmod->getControllerConnection());
}

void CovisePlugin::expandBoundingSphere(osg::BoundingSphere &bsphere)
{
    if (coVRMSController::instance()->isCluster() && coVRDistributionManager::instance().isActive())
    {
        struct BSphere
        {
            double x, y, z, radius;
        };
        BSphere b_sphere;
        b_sphere.x = bsphere.center()[0];
        b_sphere.y = bsphere.center()[1];
        b_sphere.z = bsphere.center()[2];
        b_sphere.radius = bsphere.radius();

        if (coVRMSController::instance()->isMaster())
        {
            coVRMSController::SlaveData result(sizeof(b_sphere));

            if (coVRMSController::instance()->readSlaves(&result) < 0)
            {
                std::cerr << "VRSceneGraph::getBoundingSphere err: sync error";
                return;
            }

            BSphere bs;
            for (std::vector<void *>::iterator i = result.data.begin();
                 i != result.data.end(); ++i)
            {
                memcpy(&bs, *i, sizeof(bs));
                osg::BoundingSphere otherBs = osg::BoundingSphere(osg::Vec3(bs.x, bs.y, bs.z), bs.radius);
                bsphere.expandBy(otherBs);
            }

            b_sphere.x = bsphere.center()[0];
            b_sphere.y = bsphere.center()[1];
            b_sphere.z = bsphere.center()[2];
            b_sphere.radius = bsphere.radius();

            coVRMSController::instance()->sendSlaves(&b_sphere, sizeof(b_sphere));
        }
        else
        {
            coVRMSController::instance()->sendMaster(&b_sphere, sizeof(b_sphere));
            coVRMSController::instance()->readMaster(&b_sphere, sizeof(b_sphere));
            bsphere = osg::BoundingSphere(osg::Vec3(b_sphere.x, b_sphere.y, b_sphere.z), b_sphere.radius);
        }
    }
}

COVERPLUGIN(CovisePlugin)

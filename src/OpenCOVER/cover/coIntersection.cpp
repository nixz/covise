/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#include "coIntersection.h"

#include "coVRPluginSupport.h"
#include "coVRConfig.h"
#include "VRSceneGraph.h"
#include "VRViewer.h"

#include <config/CoviseConfig.h>

#include <osg/LineSegment>
#include <osg/Matrix>
#include <osg/Vec3>

#include <OpenVRUI/osg/OSGVruiHit.h>
#include <OpenVRUI/osg/OSGVruiNode.h>

#include <OpenVRUI/util/vruiLog.h>

#include <input/input.h>

#include "coVRNavigationManager.h"
#include "coVRIntersectionInteractorManager.h"
#include "coIntersectionUtil.h"

#include <util/coWristWatch.h>
#include <numeric>

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace osg;
using namespace osgUtil;
using namespace vrui;
using namespace opencover;
using covise::coCoviseConfig;

coIntersection *coIntersection::intersector = 0;

int coIntersection::myFrameIndex = -1;
int coIntersection::myFrame = 0;

coIntersection::coIntersection()
#ifdef _OPENMP
    : elapsedTimes(omp_get_max_threads())
#else
    : elapsedTimes(1)
#endif
{
    // should match OpenCOVER.cpp
    std::string openmpThreads = coCoviseConfig::getEntry("value", "COVER.OMPThreads", "off");
    useOmp = openmpThreads != "off";

    //VRUILOG("coIntersection::<init> info: creating");

    if (myFrameIndex < 0)
    {
        myFrameIndex = frames().size();
        frameIndex = myFrameIndex;
        frames().push_back(&myFrame);
        intersectors().push_back(this);
    }

    intersectionDist = coCoviseConfig::getFloat("COVER.PointerAppearance.Intersection", 1000000.0f);

    intersector = this;
    numIsectAllNodes = 0;
}

bool coIntersection::isVerboseIntersection()
{
    static bool verboseIntersection = coCoviseConfig::isOn(std::string("intersect"), std::string("COVER.DebugLevel"), false);
    return verboseIntersection;
}

coIntersection::~coIntersection()
{
    //VRUILOG("coIntersection::<dest> info: destroying");
}

coIntersection *coIntersection::instance()
{
    if (intersector == 0)
    {
        //VRUILOG("coIntersection::the info: making new coIntersection");
        intersector = new coIntersection();
    }
    return intersector;
}

void coIntersection::intersect()
{
    double beginTime = VRViewer::instance()->elapsedTime();

    cover->intersectedNode = 0;

    if (Input::instance()->isTrackingOn())
    {
        if (true /*&& !coVRConfig::instance()->useWiiMote() */)
        {
            //fprintf(stderr, "coIntersection::intersect() NOT wiiMode\n");
            if (coVRNavigationManager::instance()->getMode() == coVRNavigationManager::TraverseInteractors && coVRIntersectionInteractorManager::the()->getCurrentIntersectionInteractor())
            {
                osg::Matrix handMat = cover->getPointerMat();
                osg::Matrix o_to_w = cover->getBaseMat();
                osg::Vec3 currentInterPos_w = coVRIntersectionInteractorManager::the()->getCurrentIntersectionInteractor()->getMatrix().getTrans() * o_to_w;
                //fprintf(stderr, "--- coIntersection::intersect currentInterPos_o(%f %f %f) \n", currentInterPos_w.x(),currentInterPos_w.y(),currentInterPos_w.z());
                handMat.setTrans(currentInterPos_w);

                intersect(handMat, false);
            }
            else
                intersect(cover->getPointerMat(), false);
        }
        /*
      else
      {
         //fprintf(stderr, "coIntersection::intersect() wiiMode\n");
         Matrix m = cover->getPointerMat();
         Vec3 trans = m.getTrans();
         //fprintf(stderr, "trans %f %f %f\n", trans[0], trans[1], trans[2]);
         //trans[1] = -500;
         m.setTrans(trans);
         //       VRTracker::instance()->setHandMat(m);
         intersect(m, false);
      }
      */
    }

    if (!cover->intersectedNode && coVRConfig::instance()->mouseNav())
        intersect(cover->getMouseMat(), true);

    if (VRViewer::instance()->getStats() && VRViewer::instance()->getStats()->collectStats("isect"))
    {
        int fn = VRViewer::instance()->getFrameStamp()->getFrameNumber();
        double endTime = VRViewer::instance()->elapsedTime();
        VRViewer::instance()->getStats()->setAttribute(fn, "Isect begin time", beginTime);
        VRViewer::instance()->getStats()->setAttribute(fn, "Isect end time", endTime);
        VRViewer::instance()->getStats()->setAttribute(fn, "Isect time taken", endTime - beginTime);
    }
}

void coIntersection::intersect(const osg::Matrix &handMat, bool mouseHit)
{
#ifdef _OPENMP
    if (useOmp)
    {
	intersectTemp<opencover::Private::coIntersectionVisitor>(handMat, mouseHit);
    }
    else
#endif
    {
	intersectTemp<IntersectVisitor>(handMat, mouseHit);
    }
}

template<class IsectVisitor>
void coIntersection::intersectTemp(const osg::Matrix &handMat, bool mouseHit)
{

    //VRUILOG("coIntersection::intersect info: called");

    Vec3 q0, q1;

    q0.set(0.0f, 0.0f, 0.0f);
    q1.set(0.0f, intersectionDist, 0.0f);

    // xform the intersection line segment
    q0 = handMat.preMult(q0);
    q1 = handMat.preMult(q1);

    //fprintf(stderr, "--- coIntersection::intersect q0(%f %f %f) q1(%f %f %f) \n", q0.x(),q0.y(),q0.z(),q1.x(),q1.y(),q1.z());
    //VRUILOG("coIntersection::intersect info: hand " << handMat);
    //VRUILOG("coIntersection::intersect info: ray from " << q0 << " to " << q1);

    ref_ptr<LineSegment> ray = new LineSegment();
    ray->set(q0, q1);

    if (q0 != q1)
    {
        IsectVisitor visitor;
        if (numIsectAllNodes > 0)
        {
            visitor.setTraversalMask(Isect::Pick);
        }
        else
        {
            visitor.setTraversalMask(Isect::Intersection);
        }
        visitor.addLineSegment(ray.get());

        {

            covise::coWristWatch watch;
            cover->getScene()->accept(visitor);

            if (isVerboseIntersection())
            {
#ifdef _OPENMP
                elapsedTimes[omp_get_max_threads() - 1].push_back(watch.elapsed());
#else
                elapsedTimes[0].push_back(watch.elapsed());
#endif
                std::cerr << " avg. intersection times";
                for (size_t ctr = 0; ctr < elapsedTimes.size(); ++ctr)
                {
                    std::cerr << " | " << ctr + 1 << ":"
                              << std::accumulate(elapsedTimes[ctr].begin(), elapsedTimes[ctr].end(), 0.0f) / elapsedTimes[ctr].size()
                              << "s";
                }
                std::cerr << " | " << std::endl;
            }
        }

        if (visitor.getNumHits(ray.get()))
        {
            //VRUILOG("coIntersection::intersect info: hit");
            //fprintf(stderr, " --- HIT \n");

            std::vector<osgUtil::Hit> hitList;
            hitList = visitor.getHitList(ray.get());

            // check which node in the hit list is also visible
            bool hasVisibleHit = false;
            for (size_t i = 0; i < hitList.size(); i++)
            {

                if (hitList[i]._geode->getNodeMask() & (Isect::Visible))
                {

                    hitInformation = hitList[i];
                    hasVisibleHit = true;
                    // check also parents of this visible node,
                    osg::Node *parent;
                    // there could be an invisible dcs above
                    if (hitList[i]._geode->getNumParents())
                        parent = hitList[i]._geode->getParent(0);
                    else
                        parent = NULL;
                    while (parent && (parent != cover->getObjectsRoot()))
                    {

                        if (parent->getNodeMask() & (Isect::Visible))
                        {

                            //parent was also visible, get his parent
                            if (parent->getNumParents())
                            {

                                parent = parent->getParent(0);
                            }
                            else
                                parent = NULL;
                        }
                        else // parent not visible
                        {

                            //stop this while loop for going ip in sg
                            hasVisibleHit = false;
                            break;
                        }
                    }
                    if (hasVisibleHit) // all parents are also visible
                        break; // stop this for loop for going through other hits farer away from nearest
                }

                //else
                //{
                //   if (! (hitList[i]._geode->getName().empty()) )
                //   {
                //      fprintf(stderr,"intersceting a unvisible node with name %s\n", hitList[i]._geode->getName().c_str());
                //   }
                //}
            }
            if (hasVisibleHit)
            {
                cover->intersectionHitPointWorld = hitInformation.getWorldIntersectPoint();
                cover->intersectionHitPointWorldNormal = hitInformation.getWorldIntersectNormal();
                cover->intersectionHitPointLocal = hitInformation.getLocalIntersectPoint();
                cover->intersectionHitPointLocalNormal = hitInformation.getLocalIntersectNormal();
                cover->intersectionMatrix = hitInformation._matrix;
                cover->intersectedNode = hitInformation._geode;

                //if( !cover->intersectedNode.get()->getName().empty())
                //    fprintf(stderr,"coIntersection::intersect hit node %s\n", cover->intersectedNode.get()->getName().c_str());
                //else
                //    fprintf(stderr,"coIntersection::intersect hit node without name\n");

                cover->intersectedNodePath = hitInformation.getNodePath();
                // walk up to the root and call all coActions
                OSGVruiHit hit(hitInformation, mouseHit);
                OSGVruiNode node(cover->intersectedNode.get());
                callActions(&node, &hit);
            }
        }
        else
        {
            //VRUILOG("coIntersection::intersect info: miss");
        }
    }
}

const char *coIntersection::getClassName() const
{
    return "coIntersection";
}

const char *coIntersection::getActionName() const
{
    return "coAction";
}

void coIntersection::isectAllNodes(bool isectAll)
{
    //fprintf(stderr,"coIntersection::isectAllNodes %d...", isectAll);

    if (isectAll)
        numIsectAllNodes++;
    else
    {
        if (numIsectAllNodes > 0)
            numIsectAllNodes--;
    }

    //fprintf(stderr,"numIsectAllNodes=%d\n", numIsectAllNodes);
}

void coIntersection::forceIsectAllNodes(bool isectAll)
{
    //fprintf(stderr,"coIntersection::isectAllNodes %d...", isectAll);

    if (isectAll)
        numIsectAllNodes++;
    else
    {
        numIsectAllNodes = 0;
    }

    //fprintf(stderr,"numIsectAllNodes=%d\n", numIsectAllNodes);
}

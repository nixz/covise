/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

/**************************************************************************
** ODD: OpenDRIVE Designer
**   Frank Naegele (c) 2010
**   <mail@f-naegele.de>
**   02.02.2010
**
**************************************************************************/

#include "rsystemelementroad.hpp"

#include <math.h>

// Data //
//
#include "src/data/projectdata.hpp"
#include "roadsystem.hpp"

#include "roadlink.hpp"

#include "sections/typesection.hpp"
#include "sections/surfacesection.hpp"
#include "track/trackcomponent.hpp"
#include "sections/elevationsection.hpp"
#include "sections/superelevationsection.hpp"
#include "sections/crossfallsection.hpp"
#include "sections/lanesection.hpp"
#include "sections/objectobject.hpp"
#include "sections/crosswalkobject.hpp"
#include "sections/signalobject.hpp"
#include "sections/sensorobject.hpp"
#include "sections/bridgeobject.hpp"

// Qt //
//
#include <QVector2D>

// temp
#include <QDebug>
// temp

/** CONSTRUCTOR.
*	The length will be defined by its child tracks.
*/
RSystemElementRoad::RSystemElementRoad(const QString &name, const QString &id, const QString &junction)
    : RSystemElement(name, id, RSystemElement::DRE_Road)
    , roadChanges_(0x0)
    , junction_(junction)
    , cachedLength_(0.0)
    , predecessor_(NULL)
    , successor_(NULL)
    , surfaceSection_(NULL)
    , signalIdCount_(0)
    , objectIdCount_(0)
{
    // Road Length //
    //
    updateLength();
}

/** DESTRUCTOR.
*/
RSystemElementRoad::~RSystemElementRoad()
{
    delete predecessor_;
    delete successor_;

    // Delete child nodes //
    //
    foreach (TypeSection *child, typeSections_)
        delete child;

    if (surfaceSection_)
        delete surfaceSection_;

    foreach (TrackComponent *child, trackSections_)
        delete child;

    foreach (ElevationSection *child, elevationSections_)
        delete child;

    foreach (SuperelevationSection *child, superelevationSections_)
        delete child;

    foreach (CrossfallSection *child, crossfallSections_)
        delete child;

    foreach (LaneSection *child, laneSections_)
        delete child;

    foreach (Crosswalk *child, crosswalks_)
        delete child;
}

/*! \brief Set the Id of the junction. If the Id is "-1" the road is not a path of any junction.
*
*/
void
RSystemElementRoad::setJunction(const QString &junctionId)
{
    junction_ = junctionId;
    addRoadChanges(RSystemElementRoad::CRD_JunctionChange);
}

//###################//
// road:link         //
//###################//

void
RSystemElementRoad::setPredecessor(RoadLink *roadLink)
{
    if (predecessor_)
    {
        predecessor_->setParentRoad(NULL, RoadLink::DRL_UNKNOWN);
    }
    predecessor_ = roadLink;
    if (predecessor_)
    {
        predecessor_->setParentRoad(this, RoadLink::DRL_PREDECESSOR);
    }
    addRoadChanges(RSystemElementRoad::CRD_PredecessorChange);
}

void
RSystemElementRoad::setSuccessor(RoadLink *roadLink)
{
    if (successor_)
    {
        successor_->setParentRoad(NULL, RoadLink::DRL_UNKNOWN);
    }
    successor_ = roadLink;
    if (successor_)
    {
        successor_->setParentRoad(this, RoadLink::DRL_SUCCESSOR);
    }
    addRoadChanges(RSystemElementRoad::CRD_SuccessorChange);
}

bool
RSystemElementRoad::delPredecessor()
{
    if (!predecessor_)
    {
        qDebug("WARNING 1004090904! Tried to delete a Predecessor that wasn't there.");
        return false;
    }

    predecessor_->setParentRoad(NULL, RoadLink::DRL_UNKNOWN);
    predecessor_ = NULL;

    addRoadChanges(RSystemElementRoad::CRD_PredecessorChange);

    return true;
}

bool
RSystemElementRoad::delSuccessor()
{
    if (!successor_)
    {
        qDebug("WARNING 1004090904! Tried to delete a Successor that wasn't there.");
        return false;
    }

    successor_->setParentRoad(NULL, RoadLink::DRL_UNKNOWN);
    successor_ = NULL;

    addRoadChanges(RSystemElementRoad::CRD_SuccessorChange);

    return true;
}

//###################//
// road:section      //
//###################//

/*! \brief Moves the section to the new s coordinate.
*
* The s coordinate will be clamped to [0.0, roadLength].
*/
bool
RSystemElementRoad::moveRoadSection(RoadSection *section, double newS, RSystemElementRoad::DRoadSectionType sectionType)
{
    // Clamp (just in case) //
    //
    if (newS < 0.0)
    {
        qDebug("WARNING 1007141735! Tried to move a road section but failed (s < 0.0).");
        newS = 0.0;
    }

    // Call specific function //
    //
    bool success = false;
    if (sectionType == RSystemElementRoad::DRS_TypeSection)
    {
        success = moveTypeSection(section->getSStart(), newS);
    }
    else if (sectionType == RSystemElementRoad::DRS_ElevationSection)
    {
        success = moveElevationSection(section->getSStart(), newS);
    }
    else if (sectionType == RSystemElementRoad::DRS_SuperelevationSection)
    {
        success = moveSuperelevationSection(section->getSStart(), newS);
    }
    else if (sectionType == RSystemElementRoad::DRS_CrossfallSection)
    {
        success = moveCrossfallSection(section->getSStart(), newS);
    }
    else if (sectionType == RSystemElementRoad::DRS_LaneSection)
    {
        success = moveLaneSection(section->getSStart(), newS);
    }
    else if (sectionType == RSystemElementRoad::DRS_SignalSection)
    {
        success = moveSignal(section, newS);
    }
    else if (sectionType == RSystemElementRoad::DRS_ObjectSection)
    {
        success = moveObject(section, newS);
    }
    else if (sectionType == RSystemElementRoad::DRS_BridgeSection)
    {
        success = moveBridge(section, newS);
    }

    // Done //
    //
    if (!success)
    {
        qDebug("WARNING 1006160941! Tried to move a road section but failed.");
        return false;
    }

    return true;
}

RoadSection *
RSystemElementRoad::getRoadSectionBefore(double s, RSystemElementRoad::DRoadSectionType sectionType) const
{
    // Call specific function //
    //
    if (sectionType == RSystemElementRoad::DRS_TypeSection)
    {
        return getTypeSectionBefore(s);
    }
    else if (sectionType == RSystemElementRoad::DRS_ElevationSection)
    {
        return getElevationSectionBefore(s);
    }
    else if (sectionType == RSystemElementRoad::DRS_SuperelevationSection)
    {
        return getSuperelevationSectionBefore(s);
    }
    else if (sectionType == RSystemElementRoad::DRS_CrossfallSection)
    {
        return getCrossfallSectionBefore(s);
    }
    else if (sectionType == RSystemElementRoad::DRS_LaneSection)
    {
        return getLaneSectionBefore(s);
    }
    else
    {
        return NULL;
    }
}

//###################//
// road:planView     //
//###################//

/*!
* Adds a TrackComponent to this road. The key is the start
* coordinate of the track (road coordinate, s in [m]).
*
* \param track The TrackComponent to be added.
*/
void
RSystemElementRoad::addTrackComponent(TrackComponent *track)
{
    // Notify section //
    //
    track->setParentRoad(this);

    // Insert and Notify //
    //
    trackSections_.insert(track->getSStart(), track);
    addRoadChanges(RSystemElementRoad::CRD_TrackSectionChange);

    // Road Length //
    //
    updateLength();
}

/*!
* Deletes the given track of this road.
*
* \param track The TrackComponent to be removed.
* \returns Returns true if a track has been removed. False otherwise.
*/
bool
RSystemElementRoad::delTrackComponent(TrackComponent *track)
{
    return delTrackComponent(track->getSStart());
}

/*!
* Deletes the track of this road that starts at the
* given road coordinate s [m].
*
* \note This does not delete the TrackComponent! (It is now owned by a
* command, so it can be inserted again.)
*
* \param s The start coordinate of the track to be removed.
* \returns Returns true if a track has been removed. False otherwise.
*/
bool
RSystemElementRoad::delTrackComponent(double s)
{
    TrackComponent *track = trackSections_.value(s, NULL);
    if (!track)
    {
        qDebug("WARNING 1004090904! Tried to delete a TrackComponent that wasn't there.");
        return false;
    }
    else
    {
        // Notify section //
        //
        track->setParentRoad(NULL);

        // Delete and Notify //
        //
        trackSections_.remove(s);
        addRoadChanges(RSystemElementRoad::CRD_TrackSectionChange);

        // Road Length //
        //
        updateLength();

        return true;
    }
}

void
RSystemElementRoad::setTrackComponents(QMap<double, TrackComponent *> newSections)
{
    foreach (TrackComponent *section, trackSections_)
    {
        section->setParentRoad(NULL);
    }

    foreach (TrackComponent *section, newSections)
    {
        section->setParentRoad(this);
    }

    trackSections_ = newSections;
    rebuildTrackComponentList();
    addRoadChanges(RSystemElementRoad::CRD_TrackSectionChange);
}

/** \brief Returns the TrackComponent containing s.
*
* If s is out of bounds, a NULL pointer will be returned.
* Road coordinates [m].
*/
TrackComponent *
RSystemElementRoad::getTrackComponent(double s) const
{
    QMap<double, TrackComponent *>::const_iterator i = trackSections_.upperBound(s);
    if (i == trackSections_.constBegin())
    {
        //		qDebug("WARNING 1005311531! Trying to get TrackComponent but coordinate is out of bounds!");
        return NULL;
    }
    else
    {
        --i;
        return i.value();
    }
}

TrackComponent *
RSystemElementRoad::getTrackComponentBefore(double s) const
{
    QMap<double, TrackComponent *>::const_iterator i = trackSections_.upperBound(s); // the second one after the one we want
    if (i == trackSections_.constBegin())
    {
        return NULL;
    }
    --i;

    if (i == trackSections_.constBegin())
    {
        return NULL;
    }
    --i;

    return i.value();
}

TrackComponent *
RSystemElementRoad::getTrackComponentNext(double s) const
{
    QMap<double, TrackComponent *>::const_iterator i = trackSections_.upperBound(s);
    if (i == trackSections_.constEnd())
    {
        return NULL;
    }

    return i.value();
}

/*! \brief .
*
*
*/
void
RSystemElementRoad::rebuildTrackComponentList()
{
    // Fill new list //
    //
    QMap<double, TrackComponent *> newTrackSections;
    double length = 0.0;
    QMap<double, TrackComponent *>::const_iterator i = trackSections_.begin();
    while (i != trackSections_.end())
    {
        TrackComponent *track = i.value();
        track->setSStart(length);
        newTrackSections.insert(track->getSStart(), track);
        length += track->getLength();
        ++i;
    }
    trackSections_ = newTrackSections;
    //addRoadChanges(RSystemElementRoad::CRD_TrackSectionChange); // Only the length changes here! No new tracks.

    // Road Length //
    //
    addRoadChanges(RSystemElementRoad::CRD_LengthChange);
    cachedLength_ = length;
}

/** \brief .
*
*
*/
QPointF
RSystemElementRoad::getGlobalPoint(double s, double d) const
{
    QMap<double, TrackComponent *>::const_iterator i = trackSections_.upperBound(s);
    if (!(i == trackSections_.begin()))
        --i;
    return i.value()->getGlobalPoint(s, d);
}

/** \brief .
*
*
*/
double
RSystemElementRoad::getGlobalHeading(double s) const
{
    QMap<double, TrackComponent *>::const_iterator i = trackSections_.upperBound(s);
    if (!(i == trackSections_.begin()))
        --i;
    return i.value()->getGlobalHeading(s);
}

/** \brief
*
*
*/
QVector2D
RSystemElementRoad::getGlobalTangent(double s) const
{
    QMap<double, TrackComponent *>::const_iterator i = trackSections_.upperBound(s);
    if (!(i == trackSections_.begin()))
        --i;
    return i.value()->getGlobalTangent(s);
}

/** \brief
*
*
*/
QVector2D
RSystemElementRoad::getGlobalNormal(double s) const
{
    QMap<double, TrackComponent *>::const_iterator i = trackSections_.upperBound(s);
    if (!(i == trackSections_.begin()))
        --i;
    return i.value()->getGlobalNormal(s);
}

/**
*/
QTransform
RSystemElementRoad::getGlobalTransform(double s, double d) const
{
    QPointF pos = getGlobalPoint(s, d);
    double heading = getGlobalHeading(s);

    QTransform trafo;
    trafo.translate(pos.x(), pos.y());
    trafo.rotate(heading);
    return trafo;
}

/** \brief Calculates the s coordinate for a given global point.
*
* Calculates a rough approximation, then ...
*
* Searches in the interval [sMin, sMax].
*
* \param sMin Lower endpoint of the interval [sMin, sMax].
* \param sMax Upper endpoint of the interval [sMin, sMax].
* \param sInit Pass an initial value as a tip (e.g. the last result).
* This speeds up the iteration.
*/
double
RSystemElementRoad::getSFromGlobalPoint(const QPointF &globalPos, double sStart, double sEnd)
{
    // Checks //
    //
    if (sStart < 0.0)
    {
        sStart = 0.0;
    }
    if (sEnd < 0.0)
    {
        sEnd = getLength();
    }
    if (sEnd < sStart)
        sEnd = sStart;

    // Road Approximation //
    //
    // The road will be approximated by 5 lines. The start and end points are cached,
    // once in road coordinates s in [m] and once in global coordinates [m].

    int segmentCount = 5;
    double segmentLength = (sEnd - sStart) / segmentCount;
    QVector<double> supportPoints(segmentCount + 1);
    QVector<QVector2D> supportVectors(segmentCount + 1);

    QVector2D pos = QVector2D(globalPos);

    for (int i = 0; i < segmentCount + 1; ++i)
    {
        double s = sStart + i * segmentLength;
        supportPoints[i] = s;
        supportVectors[i] = QVector2D(getGlobalPoint(s, 0.0)); // chord line points
    }

    // Find the segment that is closest to globalPos //
    //
    double minDistance = 1.0e10;
    double minLambda = supportPoints[0];
    int minSegment = 0;

    for (int i = 0; i < segmentCount; ++i)
    {
        QVector2D t = supportVectors[i + 1] - supportVectors[i];
        double lambda = QVector2D::dotProduct(t, pos - supportVectors[i]) / QVector2D::dotProduct(t, t);

        // Check boundaries 0<=lambda<=1 //
        //
        // Do not use exact boundaries because the lines are just approximations
        if (lambda < -0.5)
        {
            continue;
        }
        else if (lambda > 1.5)
        {
            continue;
        }

        // Normal Vector //
        //
        QVector2D n = pos - supportVectors[i] - t * lambda;

        // Check if this is the best so far //
        //
        double distance = n.length();
        if (distance < minDistance)
        {
            minDistance = distance;
            minLambda = lambda;
            minSegment = i;
        }
    }

    double sApprox = supportPoints[minSegment] + minLambda * (supportPoints[minSegment + 1] - supportPoints[minSegment]);
    if (sApprox < 0.0)
    {
        sApprox = 0.0;
    }

// Newton Raphson //
//
#if 1
    double sOld = sApprox;
    double crit = 0.01; // stop if improvement is less than 1cm
    int iIteration = 0;

    QVector2D rc;
    QVector2D t;
    QVector2D n;

    do
    {
        sOld = sApprox;
        ++iIteration;

        rc = pos - QVector2D(getGlobalPoint(sApprox, 0.0));
        t = getGlobalTangent(sApprox);
        n = getGlobalNormal(sApprox);
        //		sApprox = sApprox - (QVector2D::dotProduct(rc, t) / (-fabs(QVector2D::dotProduct(rc, n)) - t.length())); // more iterations, wrong
        sApprox = sApprox - (QVector2D::dotProduct(rc, t) / (-getTrackComponent(sApprox)->getCurvature(sApprox) * QVector2D::dotProduct(rc, n) - 1.0));

        // Check if it's ok to continue //
        //
        if (sApprox < sStart)
        {
            return sStart;
        }
        else if (sApprox > sEnd)
        {
            return sEnd;
        }
        else if (iIteration >= 50)
        {
            return sApprox;
        }
        //		return Vector2D(std::numeric_limits<float>::signaling_NaN(), std::numeric_limits<float>::signaling_NaN());
    } while (fabs(sOld - sApprox) > crit);

    // Check result for validity //
    //
    if (sApprox != sApprox)
    {
        qDebug("sApprox != sApprox");
        return 0.0;
        // TODO: PROBLEM
        //		return Vector2D(std::numeric_limits<float>::signaling_NaN(), std::numeric_limits<float>::signaling_NaN());
    }

#endif

    return sApprox;
}

//###################//
// road:type         //
//###################//

/*!
* Adds a road type section to this road. The key is the start
* coordinate of the section (road coordinate, s in [m]).
*
* \param typeSection The TypeSection to be added.
*/
void
RSystemElementRoad::addTypeSection(TypeSection *section)
{
    // Notify section //
    //
    section->setParentRoad(this);

    // Notify shrinking section //
    //
    if (!typeSections_.isEmpty())
    {
        TypeSection *lastSection = getTypeSection(section->getSStart()); // the section that is here before insertion
        if (lastSection)
        {
            lastSection->addRoadSectionChanges(RoadSection::CRS_LengthChange);
        }
    }

    // Insert and Notify //
    //
    typeSections_.insert(section->getSStart(), section);
    addRoadChanges(RSystemElementRoad::CRD_TypeSectionChange);
}

void
RSystemElementRoad::setTypeSections(QMap<double, TypeSection *> newSections)
{
    foreach (TypeSection *section, typeSections_)
    {
        section->setParentRoad(NULL);
    }

    foreach (TypeSection *section, newSections)
    {
        section->setParentRoad(this);
    }

    typeSections_ = newSections;
    addRoadChanges(RSystemElementRoad::CRD_TypeSectionChange);
}

/*!
* Deletes the given road type section of this road.
*
* \param typeSection The TypeSection to be removed.
* \returns Returns true if a section has been removed. False otherwise.
*/
bool
RSystemElementRoad::delTypeSection(TypeSection *section)
{
    double s = section->getSStart();

    // Delete section //
    //
    bool success = delTypeSection(s);
    if (success)
    {
        // Notify expanding section //
        //
        TypeSection *lastSection = getTypeSection(s); // the section that is now here
        if (lastSection)
        {
            lastSection->addRoadSectionChanges(RoadSection::CRS_LengthChange);
        }
    }
    else
    {
        qDebug("WARNING 1003300932! Could not delete TypeSection.");
    }

    return success;
}

/*!
* Deletes the road type section of this road that starts at the
* given road coordinate s [m].
*
* \param s The start coordinate of the TypeSection to be removed.
* \returns Returns true if a section has been removed. False otherwise.
*/
bool
RSystemElementRoad::delTypeSection(double s)
{
    TypeSection *section = typeSections_.value(s, NULL);
    if (!section)
    {
        qDebug("WARNING 1003171532! Tried to delete a road type section that wasn't there.");
        return false;
    }
    else
    {
        // Notify section //
        //
        section->setParentRoad(NULL);

        // Delete and Notify //
        //
        typeSections_.remove(s);
        addRoadChanges(RSystemElementRoad::CRD_TypeSectionChange);

        return true;
    }
}

/** \brief Returns the typeSection containing s.
*
* If s is out of bounds, a NULL pointer will be returned.
* Road coordinates [m].
*/
TypeSection *
RSystemElementRoad::getTypeSection(double s) const
{
    QMap<double, TypeSection *>::const_iterator i = typeSections_.upperBound(s); // the one after the one we want
    if (i == typeSections_.constBegin())
    {
        //		qDebug("WARNING 1003151037! Trying to get typeSection but coordinate is out of bounds!");
        return NULL;
    }
    else
    {
        --i;
        return i.value();
    }
}

TypeSection *
RSystemElementRoad::getTypeSectionBefore(double s) const
{
    QMap<double, TypeSection *>::const_iterator i = typeSections_.upperBound(s); // the second one after the one we want
    if (i == typeSections_.constBegin())
    {
        return NULL;
    }
    --i;

    if (i == typeSections_.constBegin())
    {
        return NULL;
    }
    --i;

    return i.value();
}

/** \brief Returns the s coordinate of the end of the typeSection
* containing s.
*
* If the typeSection is the last in the list, the end of
* the road will be returned. Otherwise the end of a typeSection
* is the start of the successing one.
* Road coordinates [m].
*/
double
RSystemElementRoad::getTypeSectionEnd(double s) const
{
    QMap<double, TypeSection *>::const_iterator nextIt = typeSections_.upperBound(s);
    if (nextIt == typeSections_.constEnd())
    {
        return getLength(); // road: [0.0, length]
    }
    else
    {
        return (*nextIt)->getSStart();
    }
}

bool
RSystemElementRoad::moveTypeSection(double oldS, double newS)
{
    // Section //
    //
    TypeSection *section = typeSections_.value(oldS, NULL);
    if (!section)
    {
        return false;
    }

    // Previous section //
    //
    double previousS = 0.0;
    if (newS > section->getSStart())
    {
        // Expand previous section //
        //
        previousS = section->getSStart() - 0.001;
    }
    else
    {
        // Shrink previous section //
        //
        previousS = newS;
    }
    TypeSection *previousSection = getTypeSection(previousS);
    if (previousSection)
    {
        previousSection->addRoadSectionChanges(RoadSection::CRS_LengthChange);
    }

    // Set and insert //
    //
    section->setSStart(newS);
    typeSections_.remove(oldS);
    typeSections_.insert(newS, section);

    return true;
}

//###################//
// road:surface      //
//###################//

/*!
* Adds a road surface section to this road.
*/
void
RSystemElementRoad::addSurfaceSection(SurfaceSection *section)
{
    // Insert //
    //
    surfaceSection_ = section; // one section per road
}

//#################################//
// road:elevationProfile:elevation //
//#################################//

void
RSystemElementRoad::addElevationSection(ElevationSection *section)
{
    // Notify section //
    //
    section->setParentRoad(this);

    // Notify shrinking section //
    //
    if (!elevationSections_.isEmpty())
    {
        ElevationSection *lastSection = getElevationSection(section->getSStart()); // the section that is here before insertion
        if (lastSection)
        {
            lastSection->addRoadSectionChanges(RoadSection::CRS_LengthChange);
        }
    }

    // Insert and Notify //
    //
    elevationSections_.insert(section->getSStart(), section);
    addRoadChanges(RSystemElementRoad::CRD_ElevationSectionChange);
}

bool
RSystemElementRoad::delElevationSection(ElevationSection *section)
{
    double s = section->getSStart();

    // Delete section //
    //
    bool success = delElevationSection(s);
    if (success)
    {
        // Notify expanding section //
        //
        ElevationSection *lastSection = getElevationSection(s); // the section that now here
        if (lastSection)
        {
            lastSection->addRoadSectionChanges(RoadSection::CRS_LengthChange);
        }
    }
    else
    {
        qDebug("WARNING 1006231543! Could not delete ElevationSection.");
    }

    return success;
}

bool
RSystemElementRoad::delElevationSection(double s)
{
    ElevationSection *section = elevationSections_.value(s, NULL);
    if (!section)
    {
        qDebug("WARNING 1003221752! Tried to delete a road elevation section that wasn't there.");
        return false;
    }
    else
    {
        // Notify section //
        //
        section->setParentRoad(NULL);

        // Delete and Notify //
        //
        elevationSections_.remove(s);
        addRoadChanges(RSystemElementRoad::CRD_ElevationSectionChange);

        return true;
    }
}

ElevationSection *
RSystemElementRoad::getElevationSection(double s) const
{
    QMap<double, ElevationSection *>::const_iterator i = elevationSections_.upperBound(s);
    if (i == elevationSections_.constBegin())
    {
        //		qDebug("WARNING 1003221753! Trying to get elevationSection but coordinate is out of bounds!");
        return NULL;
    }
    else
    {
        --i;
        return i.value();
    }
}

ElevationSection *
RSystemElementRoad::getElevationSectionBefore(double s) const
{
    QMap<double, ElevationSection *>::const_iterator i = elevationSections_.upperBound(s); // the second one after the one we want
    if (i == elevationSections_.constBegin())
    {
        return NULL;
    }
    --i;

    if (i == elevationSections_.constBegin())
    {
        return NULL;
    }
    --i;

    return i.value();
}

ElevationSection *
RSystemElementRoad::getElevationSectionNext(double s) const
{
    QMap<double, ElevationSection *>::const_iterator i = elevationSections_.upperBound(s); // the second one after the one we want
    if (i == elevationSections_.constEnd())
    {
        return NULL;
    }

    return i.value();
}

double
RSystemElementRoad::getElevationSectionEnd(double s) const
{
    QMap<double, ElevationSection *>::const_iterator nextIt = elevationSections_.upperBound(s);
    if (nextIt == elevationSections_.constEnd())
    {
        return getLength(); // road: [0.0, length]
    }
    else
    {
        return (*nextIt)->getSStart();
    }
}

bool
RSystemElementRoad::moveElevationSection(double oldS, double newS)
{
    // Section //
    //
    ElevationSection *section = elevationSections_.value(oldS, NULL);
    if (!section)
    {
        return false;
    }

    // Previous section //
    //
    double previousS = 0.0;
    if (newS > section->getSStart())
    {
        // Expand previous section //
        //
        previousS = section->getSStart() - 0.001;
    }
    else
    {
        // Shrink previous section //
        //
        previousS = newS;
    }
    ElevationSection *previousSection = getElevationSection(previousS);
    if (previousSection)
    {
        previousSection->addRoadSectionChanges(RoadSection::CRS_LengthChange);
    }

    // Set and insert //
    //
    section->setSStart(newS);
    elevationSections_.remove(oldS);
    elevationSections_.insert(newS, section);

    return true;
}

void
RSystemElementRoad::setElevationSections(QMap<double, ElevationSection *> newSections)
{
    foreach (ElevationSection *section, elevationSections_)
    {
        section->setParentRoad(NULL);
    }

    foreach (ElevationSection *section, newSections)
    {
        section->setParentRoad(this);
    }

    elevationSections_ = newSections;
    addRoadChanges(RSystemElementRoad::CRD_ElevationSectionChange);
}

//####################################//
// road:lateralProfile:superelevation //
//####################################//

void
RSystemElementRoad::addSuperelevationSection(SuperelevationSection *section)
{
    // Notify section //
    //
    section->setParentRoad(this);

    // Notify shrinking section //
    //
    if (!superelevationSections_.isEmpty())
    {
        SuperelevationSection *lastSection = getSuperelevationSection(section->getSStart()); // the section that is here before insertion
        if (lastSection)
        {
            lastSection->addRoadSectionChanges(RoadSection::CRS_LengthChange);
        }
    }

    // Insert and Notify //
    //
    superelevationSections_.insert(section->getSStart(), section);
    addRoadChanges(RSystemElementRoad::CRD_SuperelevationSectionChange);
}

bool
RSystemElementRoad::delSuperelevationSection(SuperelevationSection *section)
{
    double s = section->getSStart();

    // Delete section //
    //
    bool success = delSuperelevationSection(s);
    if (success)
    {
        // Notify expanding section //
        //
        SuperelevationSection *lastSection = getSuperelevationSection(s); // the section that now here
        if (lastSection)
        {
            lastSection->addRoadSectionChanges(RoadSection::CRS_LengthChange);
        }
    }
    else
    {
        qDebug("WARNING 1007151040! Could not delete SuperelevationSection.");
    }

    return success;
}

bool
RSystemElementRoad::delSuperelevationSection(double s)
{
    SuperelevationSection *section = superelevationSections_.value(s, NULL);
    if (!section)
    {
        qDebug("WARNING 1003221754! Tried to delete a road superelevation section that wasn't there.");
        return false;
    }
    else
    {
        // Notify section //
        //
        section->setParentRoad(NULL);

        // Delete and Notify //
        //
        superelevationSections_.remove(s);
        addRoadChanges(RSystemElementRoad::CRD_SuperelevationSectionChange);

        return true;
    }
}

SuperelevationSection *
RSystemElementRoad::getSuperelevationSection(double s) const
{
    QMap<double, SuperelevationSection *>::const_iterator i = superelevationSections_.upperBound(s);
    if (i == superelevationSections_.constBegin())
    {
        //		qDebug("WARNING 1003221755! Trying to get superelevationSection but coordinate is out of bounds!");
        return NULL;
    }
    else
    {
        --i;
        return i.value();
    }
}

SuperelevationSection *
RSystemElementRoad::getSuperelevationSectionBefore(double s) const
{
    QMap<double, SuperelevationSection *>::const_iterator i = superelevationSections_.upperBound(s); // the second one after the one we want
    if (i == superelevationSections_.constBegin())
    {
        return NULL;
    }
    --i;

    if (i == superelevationSections_.constBegin())
    {
        return NULL;
    }
    --i;

    return i.value();
}

double
RSystemElementRoad::getSuperelevationSectionEnd(double s) const
{
    QMap<double, SuperelevationSection *>::const_iterator nextIt = superelevationSections_.upperBound(s);
    if (nextIt == superelevationSections_.constEnd())
    {
        return getLength(); // road: [0.0, length]
    }
    else
    {
        return (*nextIt)->getSStart();
    }
}

bool
RSystemElementRoad::moveSuperelevationSection(double oldS, double newS)
{
    // Section //
    //
    SuperelevationSection *section = superelevationSections_.value(oldS, NULL);
    if (!section)
    {
        return false;
    }

    // Previous section //
    //
    double previousS = 0.0;
    if (newS > section->getSStart())
    {
        // Expand previous section //
        //
        previousS = section->getSStart() - 0.001;
    }
    else
    {
        // Shrink previous section //
        //
        previousS = newS;
    }
    SuperelevationSection *previousSection = getSuperelevationSection(previousS);
    if (previousSection)
    {
        previousSection->addRoadSectionChanges(RoadSection::CRS_LengthChange);
    }

    // Set and insert //
    //
    section->setSStart(newS);
    superelevationSections_.remove(oldS);
    superelevationSections_.insert(newS, section);

    return true;
}

void
RSystemElementRoad::setSuperelevationSections(QMap<double, SuperelevationSection *> newSections)
{
    foreach (SuperelevationSection *section, superelevationSections_)
    {
        section->setParentRoad(NULL);
    }

    foreach (SuperelevationSection *section, newSections)
    {
        section->setParentRoad(this);
    }

    superelevationSections_ = newSections;
    addRoadChanges(RSystemElementRoad::CRD_SuperelevationSectionChange);
}

//###############################//
// road:lateralProfile:crossfall //
//###############################//

void
RSystemElementRoad::addCrossfallSection(CrossfallSection *section)
{
    // Notify section //
    //
    section->setParentRoad(this);

    // Notify shrinking section //
    //
    if (!crossfallSections_.isEmpty())
    {
        CrossfallSection *lastSection = getCrossfallSection(section->getSStart()); // the section that is here before insertion
        if (lastSection)
        {
            lastSection->addRoadSectionChanges(RoadSection::CRS_LengthChange);
        }
    }

    // Insert and Notify //
    //
    crossfallSections_.insert(section->getSStart(), section);
    addRoadChanges(RSystemElementRoad::CRD_CrossfallSectionChange);
}

bool
RSystemElementRoad::delCrossfallSection(CrossfallSection *section)
{
    double s = section->getSStart();

    // Delete section //
    //
    bool success = delCrossfallSection(s);
    if (success)
    {
        // Notify expanding section //
        //
        CrossfallSection *lastSection = getCrossfallSection(s); // the section that now here
        if (lastSection)
        {
            lastSection->addRoadSectionChanges(RoadSection::CRS_LengthChange);
        }
    }
    else
    {
        qDebug("WARNING 1006231543! Could not delete CrossfallSection.");
    }

    return success;
}

bool
RSystemElementRoad::delCrossfallSection(double s)
{
    CrossfallSection *section = crossfallSections_.value(s, NULL);
    if (!section)
    {
        qDebug("WARNING 1003221756! Tried to delete a road crossfall section that wasn't there.");
        return false;
    }
    else
    {
        // Notify section //
        //
        section->setParentRoad(NULL);

        // Delete and Notify //
        //
        crossfallSections_.remove(s);
        addRoadChanges(RSystemElementRoad::CRD_CrossfallSectionChange);

        return true;
    }
}

CrossfallSection *
RSystemElementRoad::getCrossfallSection(double s) const
{
    QMap<double, CrossfallSection *>::const_iterator i = crossfallSections_.upperBound(s);
    if (i == crossfallSections_.constBegin())
    {
        //		qDebug("WARNING 1003221757! Trying to get crossfallSection but coordinate is out of bounds!");
        return NULL;
    }
    else
    {
        --i;
        return i.value();
    }
}

CrossfallSection *
RSystemElementRoad::getCrossfallSectionBefore(double s) const
{
    QMap<double, CrossfallSection *>::const_iterator i = crossfallSections_.upperBound(s); // the second one after the one we want
    if (i == crossfallSections_.constBegin())
    {
        return NULL;
    }
    --i;

    if (i == crossfallSections_.constBegin())
    {
        return NULL;
    }
    --i;

    return i.value();
}

double
RSystemElementRoad::getCrossfallSectionEnd(double s) const
{
    QMap<double, CrossfallSection *>::const_iterator nextIt = crossfallSections_.upperBound(s);
    if (nextIt == crossfallSections_.constEnd())
    {
        return getLength(); // road: [0.0, length]
    }
    else
    {
        return (*nextIt)->getSStart();
    }
}

bool
RSystemElementRoad::moveCrossfallSection(double oldS, double newS)
{
    // Section //
    //
    CrossfallSection *section = crossfallSections_.value(oldS, NULL);
    if (!section)
    {
        return false;
    }

    // Previous section //
    //
    double previousS = 0.0;
    if (newS > section->getSStart())
    {
        // Expand previous section //
        //
        previousS = section->getSStart() - 0.001;
    }
    else
    {
        // Shrink previous section //
        //
        previousS = newS;
    }
    CrossfallSection *previousSection = getCrossfallSection(previousS);
    if (previousSection)
    {
        previousSection->addRoadSectionChanges(RoadSection::CRS_LengthChange);
    }

    // Set and insert //
    //
    section->setSStart(newS);
    crossfallSections_.remove(oldS);
    crossfallSections_.insert(newS, section);

    return true;
}

void
RSystemElementRoad::setCrossfallSections(QMap<double, CrossfallSection *> newSections)
{
    foreach (CrossfallSection *section, crossfallSections_)
    {
        section->setParentRoad(NULL);
    }

    foreach (CrossfallSection *section, newSections)
    {
        section->setParentRoad(this);
    }

    crossfallSections_ = newSections;
    addRoadChanges(RSystemElementRoad::CRD_CrossfallSectionChange);
}

//###################//
// road:laneSection  //
//###################//

/** \brief Adds a lane section to this road.
*
*/
void
RSystemElementRoad::addLaneSection(LaneSection *section)
{
    // Notify section //
    //
    section->setParentRoad(this);

    // Notify shrinking section //
    //
    if (!laneSections_.isEmpty())
    {
        LaneSection *lastSection = getLaneSection(section->getSStart()); // the section that is here before insertion
        if (lastSection)
        {
            lastSection->addRoadSectionChanges(RoadSection::CRS_LengthChange);
        }
    }

    // Insert and Notify //
    //
    laneSections_.insert(section->getSStart(), section);
    addRoadChanges(RSystemElementRoad::CRD_LaneSectionChange);
}

bool
RSystemElementRoad::delLaneSection(LaneSection *section)
{
    double s = section->getSStart();

    // Delete section //
    //
    bool success = delLaneSection(s);
    if (success)
    {
        // Notify expanding section //
        //
        LaneSection *lastSection = getLaneSection(s); // the section that now here
        if (lastSection)
        {
            lastSection->addRoadSectionChanges(RoadSection::CRS_LengthChange);
        }
    }
    else
    {
        qDebug("WARNING 1007151040! Could not delete LaneSection.");
    }

    return success;
}

bool
RSystemElementRoad::delLaneSection(double s)
{
    LaneSection *section = laneSections_.value(s, NULL);
    if (!section)
    {
        qDebug("WARNING 1006101713! Tried to delete a road LaneSection that wasn't there.");
        return false;
    }
    else
    {
        // Notify section //
        //
        section->setParentRoad(NULL);

        // Delete and Notify //
        //
        laneSections_.remove(s);
        addRoadChanges(RSystemElementRoad::CRD_LaneSectionChange);

        return true;
    }
}

/** \brief Returns the laneSection containing s.
*
* If s is out of bounds, a NULL pointer will be returned.
* Road coordinates [m].
*/
LaneSection *
RSystemElementRoad::getLaneSection(double s) const
{
    QMap<double, LaneSection *>::const_iterator i = laneSections_.upperBound(s);
    if (i == laneSections_.constBegin())
    {
        //		qDebug("WARNING 1003121649! Trying to get laneSection but coordinate is out of bounds!");
        return NULL;
    }
    else
    {
        --i;
        return i.value();
    }
}

LaneSection *
RSystemElementRoad::getLaneSectionBefore(double s) const
{
    QMap<double, LaneSection *>::const_iterator i = laneSections_.upperBound(s); // the second one after the one we want
    if (i == laneSections_.constBegin())
    {
        return NULL;
    }
    --i;

    if (i == laneSections_.constBegin())
    {
        return NULL;
    }
    --i;

    return i.value();
}

LaneSection *
RSystemElementRoad::getLaneSectionNext(double s) const
{
    QMap<double, LaneSection *>::const_iterator i = laneSections_.upperBound(s);
    if (i == laneSections_.constEnd())
    {
        return NULL;
    }

    return i.value();
}

/** \brief Returns the s coordinate of the end of the laneSection
* containing s.
*
* If the laneSection is the last in the list, the end of
* the road will be returned. Otherwise the end of a laneSection
* is the start of the successing one.
* Road coordinates [m].
*/
double
RSystemElementRoad::getLaneSectionEnd(double s) const
{
    QMap<double, LaneSection *>::const_iterator nextIt = laneSections_.upperBound(s);
    if (nextIt == laneSections_.constEnd())
    {
        return getLength(); // road: [0.0, length]
    }
    else
    {
        return (*nextIt)->getSStart();
    }
}

bool
RSystemElementRoad::moveLaneSection(double oldS, double newS)
{
    // Section //
    //
    LaneSection *section = laneSections_.value(oldS, NULL);
    if (!section)
    {
        return false;
    }

    // Previous section //
    //
    double previousS = 0.0;
    if (newS > section->getSStart())
    {
        // Expand previous section //
        //
        previousS = section->getSStart() - 0.001;
    }
    else
    {
        // Shrink previous section //
        //
        previousS = newS;
    }
    LaneSection *previousSection = getLaneSection(previousS);
    if (previousSection)
    {
        previousSection->addRoadSectionChanges(RoadSection::CRS_LengthChange);
    }

    // Set and insert //
    //
    section->setSStart(newS);
    laneSections_.remove(oldS);
    laneSections_.insert(newS, section);

    return true;
}

void
RSystemElementRoad::setLaneSections(QMap<double, LaneSection *> newSections)
{
    foreach (LaneSection *section, laneSections_)
    {
        section->setParentRoad(NULL);
    }

    foreach (LaneSection *section, newSections)
    {
        section->setParentRoad(this);
    }

    laneSections_ = newSections;
    addRoadChanges(RSystemElementRoad::CRD_LaneSectionChange);
}

//#############################//
// road:laneSection:lane:width //
//#############################//

/** \brief Returns the width of the positive side of the road.
*
* This is the distance from the track chord to the outmost point
* of the road in positive lane direction.
*/
double
RSystemElementRoad::getMaxWidth(double s) const
{
    if (s < 0.0)
    {
        s = 0.0;
    }
    LaneSection *laneSection = getLaneSection(s);
    if (!laneSection)
    {
        qDebug("WARNING 1003121653! No LaneSection found.");
        return 0.0;
    }
    else
    {
        return laneSection->getLaneSpanWidth(0, laneSection->getLeftmostLaneId(), s);
    }
}

/** \brief Returns the width of the negative side of the road.
*
* This is the distance from the track chord to the outmost point
* of the road in negative lane direction.
*/
double
RSystemElementRoad::getMinWidth(double s) const
{
    if (s < 0.0)
    {
        s = 0.0;
    }
    LaneSection *laneSection = getLaneSection(s);
    if (!laneSection)
    {
        qDebug("WARNING 1003121653! No LaneSection found.");
        return 0.0;
    }
    else
    {
        return -laneSection->getLaneSpanWidth(0, laneSection->getRightmostLaneId(), s);
    }
}

//#############################//
// road:objects:object      //
//#############################//

/** \brief Adds a object object to the road
*
* This is a pedestrian object object
*/
void
RSystemElementRoad::addObject(Object *object)
{
    // Notify object //
    //
    object->setParentRoad(this);

    // Id //
    //
    QString name = object->getName();

    QString id = getRoadSystem()->getUniqueId(object->getId(), name);
    if (id != object->getId())
    {
        object->setId(id);
        if (name != object->getName())
        {
            object->setName(name);
        }
    }

    // Insert and Notify //
    //
    objects_.insert(object->getSStart(), object);
    addRoadChanges(RSystemElementRoad::CRD_ObjectChange);
}

bool
RSystemElementRoad::delObject(Object *object)
{
    QList<Object *> objectList = objects_.values(object->getSStart());
    if (!objectList.contains(object))
    {
        qDebug("WARNING 1003221758! Tried to delete a object that wasn't there.");
        return false;
    }
    else
    {
        // Notify section //
        //
        object->setParentRoad(NULL);

        // Delete and Notify //
        //
        objects_.remove(object->getSStart(), object);
        addRoadChanges(RSystemElementRoad::CRD_ObjectChange);

        return true;
    }
}

bool
RSystemElementRoad::moveObject(RoadSection *section, double newS)
{
    // Section //
    //
    Object *object = static_cast<Object *>(section);
    if (!object)
    {
        return false;
    }

    // Set and insert //
    //
    double oldS = object->getSStart();
    object->setSStart(newS);
    objects_.remove(oldS, object);
    objects_.insert(newS, object);

    object->addObjectChanges(Object::CEL_ParameterChange);

    return true;
}

//#############################//
// road:objects:bridge      //
//#############################//

/** \brief Adds a bridge object to the road
*
* This is a bridge object
*/
void
RSystemElementRoad::addBridge(Bridge *bridge)
{
    // Notify bridge //
    //
    bridge->setParentRoad(this);

    // Id //
    //
    QString name = bridge->getName();

    QString id = getRoadSystem()->getUniqueId(bridge->getId(), name);
    if (id != bridge->getId())
    {
        bridge->setId(id);
        if (name != bridge->getName())
        {
            bridge->setName(name);
        }
    }

    // Insert and Notify //
    //
    bridges_.insert(bridge->getSStart(), bridge);
    addRoadChanges(RSystemElementRoad::CRD_BridgeChange);
}

bool
RSystemElementRoad::delBridge(Bridge *bridge)
{
    QList<Bridge *> bridgeList = bridges_.values(bridge->getSStart());
    if (!bridgeList.contains(bridge))
    {
        qDebug("WARNING 1003221758! Tried to delete a bridge that wasn't there.");
        return false;
    }
    else
    {
        // Notify section //
        //
        bridge->setParentRoad(NULL);

        // Delete and Notify //
        //
        bridges_.remove(bridge->getSStart(), bridge);
        addRoadChanges(RSystemElementRoad::CRD_BridgeChange);

        return true;
    }
}

bool
RSystemElementRoad::moveBridge(RoadSection *section, double newS)
{
    // Section //
    //
    Bridge *bridge = static_cast<Bridge *>(section);
    if (!bridge)
    {
        return false;
    }

    // Set and insert //
    //
    double oldS = bridge->getSStart();
    bridge->setSStart(newS);
    bridges_.remove(oldS, bridge);
    bridges_.insert(newS, bridge);

    bridge->addBridgeChanges(Bridge::CEL_ParameterChange);

    return true;
}

//#############################//
// road:objects:crosswalk      //
//#############################//

/** \brief Adds a crosswalk object to the road
*
* This is a pedestrian crosswalk object
*/
void
RSystemElementRoad::addCrosswalk(Crosswalk *crosswalk)
{
    // Notify crosswalk //
    //
    crosswalk->setParentRoad(this);

    // Insert and Notify //
    //
    crosswalks_.insert(crosswalk->getS(), crosswalk);
    addRoadChanges(RSystemElementRoad::CRD_CrosswalkChange);
}

bool
RSystemElementRoad::delCrosswalk(Crosswalk *crosswalk)
{
    double s = crosswalk->getS();

    // Delete section //
    //
    bool success = delCrosswalk(s);
    if (!success)
    {
        qDebug("WARNING 1006231544! Could not delete crosswalk.");
    }

    return success;
}

bool
RSystemElementRoad::delCrosswalk(double s)
{
    Crosswalk *crosswalk = crosswalks_.value(s, NULL);
    if (!crosswalk)
    {
        qDebug("WARNING 1003221758! Tried to delete a crosswalk that wasn't there.");
        return false;
    }
    else
    {
        // Notify section //
        //
        crosswalk->setParentRoad(NULL);

        // Delete and Notify //
        //
        crosswalks_.remove(s);
        addRoadChanges(RSystemElementRoad::CRD_CrosswalkChange);

        return true;
    }
}

//#############################//
// road:objects:signal      //
//#############################//

/** \brief Adds a signal object to the road
*
* This is a pedestrian signal object
*/
void
RSystemElementRoad::addSignal(Signal *signal)
{
    // Notify signal //
    //
    signal->setParentRoad(this);

    // Id //
    //
    QString name = signal->getName();

    QString id = getRoadSystem()->getUniqueId(signal->getId(), name);
    if (id != signal->getId())
    {
        signal->setId(id);
        if (name != signal->getName())
        {
            signal->setName(name);
        }
    }

    // Insert and Notify //
    //
    signals_.insert(signal->getSStart(), signal);
    addRoadChanges(RSystemElementRoad::CRD_SignalChange);
}

bool
RSystemElementRoad::delSignal(Signal *signal)
{
    QList<Signal *> signalList = signals_.values(signal->getSStart());
    if (!signalList.contains(signal))
    {
        qDebug("WARNING 1003221758! Tried to delete a signal that wasn't there.");
        return false;
    }
    else
    {
        // Notify section //
        //
        signal->setParentRoad(NULL);

        // Delete and Notify //
        //
        signals_.remove(signal->getSStart(), signal);
        addRoadChanges(RSystemElementRoad::CRD_SignalChange);

        return true;
    }
}

bool
RSystemElementRoad::moveSignal(RoadSection *section, double newS)
{
    // Section //
    //
    Signal *signal = static_cast<Signal *>(section);

    if (!signal)
    {
        return false;
    }

    // Set and insert //
    //
    double oldS = signal->getSStart();
    signal->setSStart(newS);
    signals_.remove(oldS, signal);
    signals_.insert(newS, signal);

    signal->addSignalChanges(Signal::CEL_ParameterChange);

    return true;
}

int
RSystemElementRoad::getValidLane(double s, double t)
{
    int lane;

    if (t < 0)
    {
        lane = getLaneSection(s)->getRightmostLaneId();
    }
    else
    {
        lane = getLaneSection(s)->getLeftmostLaneId();
    }

    return lane;
}

Signal *
RSystemElementRoad::getSignal(const QString &id)
{
    QMap<double, Signal *>::ConstIterator iter = signals_.constBegin();

    while (iter != signals_.constEnd())
    {
        Signal * signal = iter.value();
        if (signal->getId() == id)
        {
            return signal;
        }

        iter++;
    }

    return NULL;
}

//#############################//
// road:objects:sensor      //
//#############################//

/** \brief Adds a sensor object to the road
*
* This is a pedestrian sensor object
*/
void
RSystemElementRoad::addSensor(Sensor *sensor)
{
    // Notify sensor //
    //
    sensor->setParentRoad(this);

    // Insert and Notify //
    //
    sensors_.insert(sensor->getS(), sensor);
    addRoadChanges(RSystemElementRoad::CRD_SensorChange);
}

bool
RSystemElementRoad::delSensor(Sensor *sensor)
{
    double s = sensor->getS();

    // Delete section //
    //
    bool success = delSensor(s);
    if (!success)
    {
        qDebug("WARNING 1006231544! Could not delete sensor.");
    }

    return success;
}

bool
RSystemElementRoad::delSensor(double s)
{
    Sensor *sensor = sensors_.value(s, NULL);
    if (!sensor)
    {
        qDebug("WARNING 1003221758! Tried to delete a sensor that wasn't there.");
        return false;
    }
    else
    {
        // Notify section //
        //
        sensor->setParentRoad(NULL);

        // Delete and Notify //
        //
        sensors_.remove(s);
        addRoadChanges(RSystemElementRoad::CRD_SensorChange);

        return true;
    }
}

//##################//
// IDs              //
//##################//

const QString
RSystemElementRoad::getUniqueId(const QString &suggestion, RSystemElement::DRoadSystemElementType elementType)
{
    if (elementType == RSystemElement::DRE_Signal)
    {
        // Try suggestion //
        //
        if (!suggestion.isNull())
        {
            if (!signalIds_.contains(suggestion))
            {
                signalIds_.append(suggestion);
                return suggestion;
            }
        }

        // Create new one //
        //

        QString id = QString("signal%1").arg(signalIdCount_);
        while (signalIds_.contains(id))
        {
            id = QString("signal%1").arg(signalIdCount_);
            ++signalIdCount_;
        }
        ++signalIdCount_;
        signalIds_.append(id);
        return id;
    }
    else if (elementType == RSystemElement::DRE_Object)
    {
        // Try suggestion //
        //
        if (!suggestion.isNull())
        {
            if (!objectIds_.contains(suggestion))
            {
                objectIds_.append(suggestion);
                return suggestion;
            }
        }

        // Create new one //
        //

        QString id = QString("object%1").arg(objectIdCount_);
        while (objectIds_.contains(id))
        {
            id = QString("object%1").arg(objectIdCount_);
            ++objectIdCount_;
        }
        ++objectIdCount_;
        objectIds_.append(id);
        return id;
    }
    else
    {
        return suggestion;
    }
}

//###################//
// Prototype Pattern //
//###################//

/*! \brief Adds some road sections from a prototype.
*
* Road sections will only be added if there aren't any so far.
*/
void
RSystemElementRoad::superposePrototype(const RSystemElementRoad *prototypeRoad)
{
    if (!prototypeRoad->trackSections_.empty() && trackSections_.empty())
    {
        foreach (TrackComponent *track, prototypeRoad->trackSections_)
        {
            addTrackComponent(track->getClone());
        }
    }

    if (!prototypeRoad->typeSections_.empty() && typeSections_.empty())
    {
        foreach (TypeSection *section, prototypeRoad->typeSections_)
        {
            addTypeSection(section->getClone());
        }
    }

    if (!prototypeRoad->elevationSections_.empty() && elevationSections_.empty())
    {
        foreach (ElevationSection *section, prototypeRoad->elevationSections_)
        {
            addElevationSection(section->getClone());
        }
    }

    if (!prototypeRoad->superelevationSections_.empty() && superelevationSections_.empty())
    {
        foreach (SuperelevationSection *section, prototypeRoad->superelevationSections_)
        {
            addSuperelevationSection(section->getClone());
        }
    }

    if (!prototypeRoad->crossfallSections_.empty() && crossfallSections_.empty())
    {
        foreach (CrossfallSection *section, prototypeRoad->crossfallSections_)
        {
            addCrossfallSection(section->getClone());
        }
    }

    if (!prototypeRoad->laneSections_.empty() && laneSections_.empty())
    {
        foreach (LaneSection *section, prototypeRoad->laneSections_)
        {
            addLaneSection(section->getClone());
        }
    }
}

///*! \brief Adds some road sections from a prototype at the back or front.
//*
//* The prototype sections will be cloned first.
//*/
//void
//	RSystemElementRoad
//	::appendPrototype(const RSystemElementRoad * prototypeRoad, bool atFront)
//{
//	if(atFront)
//	{
//		qDebug("TODO: RSystemElementRoad::appendPrototype at front not yet implemented!");
//	}
//	else
//	{
//		double deltaS = getLength();
//
//		// Track Components //
//		//
////		QPointF deltaPos = getGlobalPoint(getLength()) - prototypeRoad->getGlobalPoint(0.0);
//		QTransform roadTrafo = getGlobalTransform(getLength());
//		QTransform protoTrafo = prototypeRoad->getGlobalTransform(0.0);
//		double roadHeading = getGlobalHeading(getLength());
//		double protoHeading = prototypeRoad->getGlobalHeading(0.0);
////		QPointF roadPoint = getGlobalTransform(getLength());
////		QPointF protoPoint = prototypeRoad->getGlobalPoint(0.0);
////		QTransform deltaTrafo = protoTrafo * roadTrafo.inverted();
//		foreach(TrackComponent * track, prototypeRoad->trackSections_)
//		{
//			track = track->getClone(this);
////			QPointF localPos = track->getGlobalPoint(track->getSStart()) - protoPoint;
//			track->setSStart(track->getSStart()+deltaS);
////			track->setGlobalTranslation(deltaTrafo.map(track->getGlobalPoint(track->getSStart())));
////			track->setGlobalTranslation(track->getGlobalPoint(track->getSStart()) + deltaPos);
//			track->setGlobalTranslation(roadTrafo.map(protoTrafo.inverted().map(track->getGlobalPoint(track->getSStart()))));
//			track->setGlobalRotation(track->getGlobalHeading(track->getSStart()) - protoHeading + roadHeading);
//			addTrackComponent(track);
//		}
//	}
//
//}

/** Recalculates the total length of the road.
*/
double
RSystemElementRoad::updateLength()
{
    cachedLength_ = 0.0;
    foreach (TrackComponent *child, trackSections_)
    {
        cachedLength_ += child->getLength();
    }
    addRoadChanges(RSystemElementRoad::CRD_LengthChange);
    return cachedLength_;
}

/*! \brief Creates and returns a deep copy clone of this object.
*
*/
RSystemElementRoad *
RSystemElementRoad::getClone() const
{
    // New Road //
    //
    RSystemElementRoad *clonedRoad = new RSystemElementRoad(getName(), getID(), junction_);

    // RoadSections //
    //
    foreach (TypeSection *child, typeSections_)
        clonedRoad->addTypeSection(child->getClone());

    if (surfaceSection_)
        clonedRoad->addSurfaceSection(surfaceSection_->getClone());

    foreach (TrackComponent *child, trackSections_)
        clonedRoad->addTrackComponent(child->getClone());

    foreach (ElevationSection *child, elevationSections_)
        clonedRoad->addElevationSection(child->getClone());

    foreach (SuperelevationSection *child, superelevationSections_)
        clonedRoad->addSuperelevationSection(child->getClone());

    foreach (CrossfallSection *child, crossfallSections_)
        clonedRoad->addCrossfallSection(child->getClone());

    foreach (LaneSection *child, laneSections_)
        clonedRoad->addLaneSection(child->getClone());

    foreach (Crosswalk *child, crosswalks_)
        clonedRoad->addCrosswalk(child->getClone());

    // Predecessor/Successor //
    //
    if (predecessor_)
    {
        clonedRoad->setPredecessor(predecessor_->getClone());
    }

    if (successor_)
    {
        clonedRoad->setSuccessor(successor_->getClone());
    }

    // Done //
    //
    return clonedRoad;
}

//##################//
// Observer Pattern //
//##################//

/*! \brief Called after all Observers have been notified.
*
* Resets the change flags to 0x0.
*/
void
RSystemElementRoad::notificationDone()
{
    roadChanges_ = 0x0;
    DataElement::notificationDone();
}

/*! \brief Add one or more change flags.
*
*/
void
RSystemElementRoad::addRoadChanges(int changes)
{
    if (changes)
    {
        roadChanges_ |= changes;
        notifyObservers();
    }
}

//###################//
// Visitor Pattern   //
//###################//

/*!
* Accepts a visitor and passes it to all child
* nodes if autoTraverse is true.
*/
void
RSystemElementRoad::accept(Visitor *visitor)
{
    visitor->visit(this);
}

/*!
* Accepts a visitor and passes it to all child nodes.
*/
void
RSystemElementRoad::acceptForChildNodes(Visitor *visitor)
{
    acceptForRoadLinks(visitor);
    acceptForTypeSections(visitor);
    acceptForSurfaceSections(visitor);
    acceptForTracks(visitor);
    acceptForElevationSections(visitor);
    acceptForSuperelevationSections(visitor);
    acceptForCrossfallSections(visitor);
    acceptForLaneSections(visitor);
    acceptForCrosswalks(visitor);
    acceptForSignals(visitor);
    acceptForSensors(visitor);
    acceptForObjects(visitor);
    acceptForBridges(visitor);
}

/*!
* Accepts a visitor and passes it to the RoadLinks.
*/
void
RSystemElementRoad::acceptForRoadLinks(Visitor *visitor)
{
    if (predecessor_)
    {
        predecessor_->accept(visitor);
    }
    if (successor_)
    {
        successor_->accept(visitor);
    }
}

/*!
* Accepts a visitor and passes it to the type sections.
*/
void
RSystemElementRoad::acceptForTypeSections(Visitor *visitor)
{
    foreach (TypeSection *child, typeSections_)
        child->accept(visitor);
}

/*!
* Accepts a visitor and passes it to the surface sections.
*/
void
RSystemElementRoad::acceptForSurfaceSections(Visitor *visitor)
{
    if (surfaceSection_)
        surfaceSection_->accept(visitor);
}

/*!
* Accepts a visitor and passes it to the tracks.
*/
void
RSystemElementRoad::acceptForTracks(Visitor *visitor)
{
    foreach (TrackComponent *child, trackSections_)
        child->accept(visitor);
}

/*!
* Accepts a visitor and passes it to the lane sections.
*/
void
RSystemElementRoad::acceptForElevationSections(Visitor *visitor)
{
    foreach (ElevationSection *child, elevationSections_)
        child->accept(visitor);
}

/*!
* Accepts a visitor and passes it to the lane sections.
*/
void
RSystemElementRoad::acceptForSuperelevationSections(Visitor *visitor)
{
    foreach (SuperelevationSection *child, superelevationSections_)
        child->accept(visitor);
}

/*!
* Accepts a visitor and passes it to the lane sections.
*/
void
RSystemElementRoad::acceptForCrossfallSections(Visitor *visitor)
{
    foreach (CrossfallSection *child, crossfallSections_)
        child->accept(visitor);
}

/*!
* Accepts a visitor and passes it to the lane sections.
*/
void
RSystemElementRoad::acceptForLaneSections(Visitor *visitor)
{
    foreach (LaneSection *child, laneSections_)
        child->accept(visitor);
}

/*!
* Accepts a visitor and passes it to the lane sections.
*/
void
RSystemElementRoad::acceptForCrosswalks(Visitor *visitor)
{
    foreach (Crosswalk *child, crosswalks_)
        child->accept(visitor);
}

/*!
* Accepts a visitor and passes it to the lane sections.
*/
void
RSystemElementRoad::acceptForSignals(Visitor *visitor)
{
    foreach (Signal *child, signals_)
        child->accept(visitor);
}

/*!
* Accepts a visitor and passes it to the lane sections.
*/
void
RSystemElementRoad::acceptForSensors(Visitor *visitor)
{
    foreach (Sensor *child, sensors_)
        child->accept(visitor);
}

/*!
* Accepts a visitor and passes it to the lane sections.
*/
void
RSystemElementRoad::acceptForObjects(Visitor *visitor)
{
    foreach (Object *child, objects_)
        child->accept(visitor);
}

/*!
* Accepts a visitor and passes it to the lane sections.
*/
void
RSystemElementRoad::acceptForBridges(Visitor *visitor)
{
    foreach (Bridge *child, bridges_)
        child->accept(visitor);
}

#if 0
void
	RSystemElementRoad
	::resizeTrackComponent(TrackComponent * track, double oldLength)
{
	qDebug() << "change track: " << track << " " << track->getSStart();

	// Find Component //
	//
	double s = track->getSStart();
	QMap<double, TrackComponent*>::iterator i = trackSections_.find(s);
	if(i == trackSections_.end())
	{
		qDebug("WARNING 1005051026! Road resizeTrackComponent: data structure inconsistent.");
		return;
	}


	// Move  //
	//
	double deltaS = track->getLength() - oldLength;
	if(fabs(deltaS) <= NUMERICAL_ZERO8)
	{
		return; // nothing to be done
	}
	else if(deltaS < 0) // shrink
	{
		// Ascending order //
		//
		++i;
		if(i == trackSections_.end())
		{
			return; // nothing do be done
		}
		while(i != trackSections_.end())
		{
			TrackComponent * tmpTrack = i.value();
			i = trackSections_.erase(i); // automatically incremented and returned by erase()
			double newS = tmpTrack->getSStart()+deltaS;
			qDebug() << "  " << tmpTrack << ": " << tmpTrack->getSStart() << " to " << newS;
			tmpTrack->setSStart(newS);
			trackSections_.insert(newS, tmpTrack);
		}
	}
	else // expand
	{
		// Descending order //
		//
		QMap<double, TrackComponent*>::iterator j = trackSections_.end();
		QMap<double, TrackComponent*>::iterator next = j;
		--next;
		while(true)
		{
			j = next;
			--next;
			TrackComponent * tmpTrack = j.value();
			trackSections_.erase(j);

			double newS = tmpTrack->getSStart()+deltaS;
			qDebug() << "  " << tmpTrack << ": " << tmpTrack->getSStart() << " to " << newS;
			tmpTrack->setSStart(newS);
			trackSections_.insert(newS, tmpTrack);

			if(next == i)
			{
				break;
			}
		}
	}
}
#endif

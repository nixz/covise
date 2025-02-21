/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#ifndef _RecordPath_PLUGIN_H
#define _RecordPath_PLUGIN_H
/****************************************************************************\
**                                                            (C)2005 HLRS  **
**                                                                          **
** Description: RecordPath Plugin (records viewpoints and viewing directions and targets)                              **
**                                                                          **
**                                                                          **
** Author: U.Woessner		                                                 **
**                                                                          **
** History:  								                                 **
** April-05  v1	    				       		                         **
**                                                                          **
**                                                                          **
\****************************************************************************/
#include <cover/coVRPluginSupport.h>
#include <cover/coVRFileManager.h>
using namespace covise;
using namespace opencover;

#include "cover/coTabletUI.h"
#include <osg/Geode>
#include <osg/ref_ptr>
#include <osg/Geometry>
#include <osg/Material>
#include <osg/LineWidth>
#include <PluginUtil/coSphere.h>
#include <QStringList>
#include <QMap>

class PCLPlugin : public coVRPlugin, public coTUIListener
{
public:
    PCLPlugin();
    virtual ~PCLPlugin();
    static PCLPlugin *instance();
    bool init();

    int loadPCD(const char *filename, osg::Group *loadParent);
    static int sloadPCD(const char *filename, osg::Group *loadParent);
    static int unloadPCD(const char *filename);
    int loadOCT(const char *filename, osg::Group *loadParent);
    static int sloadOCT(const char *filename, osg::Group *loadParent);
    static int unloadOCT(const char *filename);

    // this will be called in PreFrame
    void preFrame();

private:
    static PCLPlugin *thePlugin;
};
#endif

/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

// **************************************************************************
//
//			Source File
//
// * Description    : Read Particle data by RECOM
//
// * Class(es)      :
//
// * inherited from :
//
// * Author  : Uwe Woessner
//
// * History : started 25.3.2010
//
// **************************************************************************

//lenght of a line
#define LINE_SIZE 512
#define MAX_FRAMES_PER_SEC
#define USE_MATH_DEFINES
#include <math.h>
#include <QDir>
#include <config/coConfig.h>
#include <config/CoviseConfig.h>

#include <cover/coVRPluginSupport.h>
#include <cover/coVRAnimationManager.h>
#include <cover/coVRFileManager.h>
#include <cover/coVRTui.h>

#ifndef _WIN32
#include <sys/dir.h>
#else
#include <stdio.h>
#include <process.h>
#include <io.h>
#include <direct.h>
#endif
#include "ParticleViewer.h"
#include "Particles.h"
#include <cover/RenderObject.h>
#include <osg/MatrixTransform>
#include <osg/Geode>
#include <cover/VRSceneGraph.h>

ParticleViewer *plugin = NULL;

FileHandler fileHandler[] = {
    { NULL,
      ParticleViewer::loadFile,
      ParticleViewer::loadFile,
      ParticleViewer::unloadFile,
      "particle" },
    { NULL,
      ParticleViewer::loadFile,
      ParticleViewer::loadFile,
      ParticleViewer::unloadFile,
      "coord" },
    { NULL,
      ParticleViewer::loadFile,
      ParticleViewer::loadFile,
      ParticleViewer::unloadFile,
      "crist" },
    { NULL,
      ParticleViewer::loadFile,
      ParticleViewer::loadFile,
      ParticleViewer::unloadFile,
      "chkpt" }
};

int ParticleViewer::loadFile(const char *fn, osg::Group *parent, const char *)
{
    if (plugin)
        return plugin->loadData(fn, parent);

    return -1;
}

int ParticleViewer::unloadFile(const char *fn, const char *)
{
    if (plugin)
    {
        plugin->unloadData(fn);
        return 0;
    }

    return -1;
}

int ParticleViewer::loadData(std::string particlepath, osg::Group *parent)
{
    int maxParticles = coCoviseConfig::getInt("COVER.Plugin.ParticleViewer.maxParticles", 10000);
    unloadData(particlepath);

    Particles *Part = new Particles(particlepath, parent, maxParticles);
    if (Part->getTimesteps() > 0)
    {
        particles.push_back(Part);
        if (Part->getMode() == Particles::M_PARTICLES)
        {
            if (valChoice->getNumEntries() < 1)
            {
                for (unsigned int i = 0; i < Part->getNumValues(); i++)
                {
                    std::vector<std::string> &varNames = Part->getValueNames();
                    if (i < varNames.size())
                    {
                        valChoice->addEntry(varNames[i]);
                        radChoice->addEntry(varNames[i]);
                    }
                }
            }
        }
        else
        {
            if (aValChoice->getNumEntries() < 2)
            {
                for (unsigned int i = 0; i < Part->getNumValues(); i++)
                {
                    aValChoice->addEntry(Part->getValueNames()[i]);
                    aRadChoice->addEntry(Part->getValueNames()[i]);
                }
            }
        }
    }

    valChoice->setSelectedEntry(0);
    radChoice->setSelectedEntry(1);

    return 0;
}

void ParticleViewer::unloadData(std::string particlepath)
{
    for (std::list<Particles *>::iterator it = particles.begin(); it != particles.end(); it++)
    {
        if ((*it)->fileName == particlepath)
        {
            delete *it;
            particles.erase(it);
            return;
        }
    }
}

//=======================================================================
// CONSTRUCTOR
//=======================================================================

ParticleViewer::ParticleViewer()
{
}

bool ParticleViewer::init()
{
    if (plugin)
        return false;
    readConfig();
    coVRFileManager::instance()->registerFileHandler(&fileHandler[0]);
    coVRFileManager::instance()->registerFileHandler(&fileHandler[1]);
    coVRFileManager::instance()->registerFileHandler(&fileHandler[2]);
    coVRFileManager::instance()->registerFileHandler(&fileHandler[3]);

    particleTab = new coTUITab("Particles", coVRTui::instance()->mainFolder->getID());
    particleTab->setPos(0, 0);
    particleLabel = new coTUILabel("particles", particleTab->getID());
    particleLabel->setPos(0, 0);
    particleFrame = new coTUIFrame("particles", particleTab->getID());
    particleFrame->setPos(0, 1);
    arrowLabel = new coTUILabel("arrows", particleTab->getID());
    arrowLabel->setPos(1, 0);
    arrowFrame = new coTUIFrame("arrows", particleTab->getID());
    arrowFrame->setPos(1, 1);

    mapChoice = new coTUIComboBox("mapChoice", particleFrame->getID());
    mapChoice->setEventListener(this);
    int i;
    for (i = 0; i < mapNames.count(); i++)
    {
        mapChoice->addEntry(mapNames[i].toStdString());
    }
    currentMap = 0;
    mapChoice->setSelectedEntry(currentMap);
    mapChoice->setPos(1, 6);

    valChoice = new coTUIComboBox("valChoice", particleFrame->getID());
    valChoice->setEventListener(this);
    valChoice->setPos(2, 6);

    radChoice = new coTUIComboBox("radChoice", particleFrame->getID());
    radChoice->setEventListener(this);
    radChoice->setPos(2, 9);

    mapLabel = new coTUILabel("map:", particleFrame->getID());
    mapLabel->setPos(0, 6);
    mapMinLabel = new coTUILabel("min:", particleFrame->getID());
    mapMinLabel->setPos(0, 7);
    mapMaxLabel = new coTUILabel("max:", particleFrame->getID());
    mapMaxLabel->setPos(0, 8);
    radiusLabel = new coTUILabel("radius:", particleFrame->getID());
    radiusLabel->setPos(0, 9);

    mapMin = new coTUIEditFloatField("min:", particleFrame->getID());
    mapMin->setValue(200);
    mapMin->setPos(1, 7);
    mapMin->setEventListener(this);
    mapMax = new coTUIEditFloatField("max:", particleFrame->getID());
    mapMax->setValue(1700);
    mapMax->setPos(1, 8);
    mapMax->setEventListener(this);
    radiusEdit = new coTUIEditFloatField("radius:", particleFrame->getID());
    radiusEdit->setValue(10);
    radiusEdit->setPos(1, 9);
    radiusEdit->setEventListener(this);

    aMapChoice = new coTUIComboBox("mapChoice", arrowFrame->getID());
    aMapChoice->setEventListener(this);

    for (int i = 0; i < mapNames.count(); i++)
    {
        aMapChoice->addEntry(mapNames[i].toStdString());
    }
    aCurrentMap = 0;
    aMapChoice->setSelectedEntry(aCurrentMap);
    aMapChoice->setPos(1, 6);

    aValChoice = new coTUIComboBox("valChoice", arrowFrame->getID());
    aValChoice->setEventListener(this);
    aValChoice->setPos(2, 6);
    aValChoice->addEntry("Constant");

    aRadChoice = new coTUIComboBox("radChoice", arrowFrame->getID());
    aRadChoice->setEventListener(this);
    aRadChoice->setPos(2, 9);

    aMapLabel = new coTUILabel("map:", arrowFrame->getID());
    aMapLabel->setPos(0, 6);
    aMapMinLabel = new coTUILabel("min:", arrowFrame->getID());
    aMapMinLabel->setPos(0, 7);
    aMapMaxLabel = new coTUILabel("max:", arrowFrame->getID());
    aMapMaxLabel->setPos(0, 8);
    aRadiusLabel = new coTUILabel("radius:", arrowFrame->getID());
    aRadiusLabel->setPos(0, 9);

    aMapMin = new coTUIEditFloatField("min:", arrowFrame->getID());
    aMapMin->setValue(200);
    aMapMin->setPos(1, 7);
    aMapMin->setEventListener(this);
    aMapMax = new coTUIEditFloatField("max:", arrowFrame->getID());
    aMapMax->setValue(1700);
    aMapMax->setPos(1, 8);
    aMapMax->setEventListener(this);
    aRadiusEdit = new coTUIEditFloatField("radius:", arrowFrame->getID());
    aRadiusEdit->setValue(10);
    aRadiusEdit->setPos(1, 9);
    aRadiusEdit->setEventListener(this);

    plugin = this;
    return true;
}

void ParticleViewer::tabletEvent(coTUIElement *tUIItem)
{
    int currentValue = valChoice->getSelectedEntry();
    int currentRValue = radChoice->getSelectedEntry();
    int aCurrentValue = aValChoice->getSelectedEntry();
    int aCurrentRValue = aRadChoice->getSelectedEntry();
    if (aCurrentValue == -1)
        aCurrentValue = 0;
    if (aCurrentRValue == -1)
        aCurrentRValue = 0;
    if (currentValue == -1)
        currentValue = 0;
    if (currentRValue == -1)
        currentRValue = 0;

    if (tUIItem == valChoice)
    {
        if (particles.begin() != particles.end())
        {
            mapMin->setValue((*particles.begin())->getMin(currentValue));
            mapMax->setValue((*particles.begin())->getMax(currentValue));
        }
        for (std::list<Particles *>::iterator it = particles.begin(); it != particles.end(); it++)
        {
            (*it)->updateColors(currentValue, aCurrentValue);
        }
    }
    if (tUIItem == aValChoice)
    {
        if (particles.begin() != particles.end())
        {
            if ((aCurrentValue > 0) && ((*particles.begin()) != NULL))
            {
                aMapMin->setValue((*particles.begin())->getMin(aCurrentValue - 1));
                aMapMax->setValue((*particles.begin())->getMax(aCurrentValue - 1));
            }
        }
        for (std::list<Particles *>::iterator it = particles.begin(); it != particles.end(); it++)
        {
            (*it)->updateColors(currentValue, aCurrentValue);
        }
    }
    if (tUIItem == mapChoice)
    {
        currentMap = mapChoice->getSelectedEntry();
        for (std::list<Particles *>::iterator it = particles.begin(); it != particles.end(); it++)
        {
            (*it)->updateColors(currentValue, aCurrentValue);
        }
    }
    if (tUIItem == mapMin || tUIItem == mapMax)
    {
        for (std::list<Particles *>::iterator it = particles.begin(); it != particles.end(); it++)
        {
            (*it)->updateColors(currentValue, aCurrentValue);
        }
    }
    if (tUIItem == aMapChoice)
    {
        aCurrentMap = aMapChoice->getSelectedEntry();
        for (std::list<Particles *>::iterator it = particles.begin(); it != particles.end(); it++)
        {
            (*it)->updateColors(currentValue, aCurrentValue);
        }
    }
    if (tUIItem == aMapMin || tUIItem == aMapMax)
    {
        for (std::list<Particles *>::iterator it = particles.begin(); it != particles.end(); it++)
        {
            (*it)->updateColors(currentValue, aCurrentValue);
        }
    }
    if ((tUIItem == radiusEdit) || (tUIItem == radChoice))
    {
        if (tUIItem == radChoice)
        {
            if (particles.begin() != particles.end())
            {
                radiusEdit->setValue((*particles.begin())->getScale(currentRValue));
            }
        }
        for (std::list<Particles *>::iterator it = particles.begin(); it != particles.end(); it++)
        {
            (*it)->updateRadii(currentRValue);
        }
    }
}

void ParticleViewer::tabletPressEvent(coTUIElement * /*tUIItem*/)
{
}
float ParticleViewer::getMinVal()
{
    return mapMin->getValue();
}

float ParticleViewer::getMaxVal()
{
    return mapMax->getValue();
}

float ParticleViewer::getRadius()
{
    return radiusEdit->getValue();
}

float ParticleViewer::getAMinVal()
{
    return aMapMin->getValue();
}

float ParticleViewer::getAMaxVal()
{
    return aMapMax->getValue();
}

float ParticleViewer::getARadius()
{
    return aRadiusEdit->getValue();
}

void ParticleViewer::deleteColorMap(const QString &name)
{
    float *mval = mapValues.value(name);
    mapSize.remove(name);
    mapValues.remove(name);
    delete[] mval;
}
//------------------------------------------------------------------------------
//
// read colormaps from xml config file
// read local colormaps
//------------------------------------------------------------------------------
void ParticleViewer::readConfig()
{
    coConfig *config = coConfig::getInstance();

    // read the name of all colormaps in file
    QStringList list;
    list = config->getVariableList("Colormaps");

    for (int i = 0; i < list.size(); i++)
        mapNames.append(list[i]);

    // read the values for each colormap
    for (int k = 1; k < mapNames.size(); k++)
    {
        // get all definition points for the colormap
        QString cmapname = "Colormaps." + mapNames[k];
        QStringList variable = config->getVariableList(cmapname);

        mapSize.insert(mapNames[k], variable.size());
        float *cval = new float[variable.size() * 5];
        mapValues.insert(mapNames[k], cval);

        // read the rgbax values
        int it = 0;
        for (int l = 0; l < variable.size() * 5; l = l + 5)
        {
            QString tmp = cmapname + ".Point:" + QString::number(it);
            cval[l] = config->getFloat("x", tmp, -1.0);
            if (cval[l] == -1)
            {
                cval[l] = (1.0 / (variable.size() - 1)) * (l / 5);
            }
            cval[l + 1] = config->getFloat("r", tmp, 1.0);
            cval[l + 2] = config->getFloat("g", tmp, 1.0);
            cval[l + 3] = config->getFloat("b", tmp, 1.0);
            cval[l + 4] = config->getFloat("a", tmp, 1.0);
            it++;
        }
    }

    // read values of local colormap files in .covise
    QString place = coConfigDefaultPaths::getDefaultLocalConfigFilePath() + "colormaps";

    QDir directory(place);
    if (directory.exists())
    {
        QStringList filters;
        filters << "colormap_*.xml";
        directory.setNameFilters(filters);
        directory.setFilter(QDir::Files);
        QStringList files = directory.entryList();

        // loop over all found colormap xml files
        for (int j = 0; j < files.size(); j++)
        {
            coConfigGroup *colorConfig = new coConfigGroup("ColorMap");
            colorConfig->addConfig(place + "/" + files[j], "local", true);

            // read the name of the colormaps
            QStringList list;
            list = colorConfig->getVariableList("Colormaps");

            // loop over all colormaps in one file
            for (int i = 0; i < list.size(); i++)
            {

                // remove global colormap with same name
                int index = mapNames.indexOf(list[i]);
                if (index != -1)
                {
                    mapNames.removeAt(index);
                    deleteColorMap(list[i]);
                }
                mapNames.append(list[i]);

                // get all definition points for the colormap
                QString cmapname = "Colormaps." + mapNames.last();
                QStringList variable = colorConfig->getVariableList(cmapname);

                mapSize.insert(list[i], variable.size());
                float *cval = new float[variable.size() * 5];
                mapValues.insert(list[i], cval);

                // read the rgbax values
                int it = 0;
                for (int l = 0; l < variable.size() * 5; l = l + 5)
                {
                    QString tmp = cmapname + ".Point:" + QString::number(it);
                    cval[l] = (colorConfig->getValue("x", tmp, " -1.0")).toFloat();
                    if (cval[l] == -1)
                    {
                        cval[l] = (1.0 / (variable.size() - 1)) * (l / 5);
                    }
                    cval[l + 1] = (colorConfig->getValue("r", tmp, "1.0")).toFloat();
                    cval[l + 2] = (colorConfig->getValue("g", tmp, "1.0")).toFloat();
                    cval[l + 3] = (colorConfig->getValue("b", tmp, "1.0")).toFloat();
                    cval[l + 4] = (colorConfig->getValue("a", tmp, "1.0")).toFloat();
                    it++;
                }
            }
            config->removeConfig(place + "/" + files[j]);
        }
    }
    mapNames.sort();
}

osg::Vec4 ParticleViewer::getColor(float pos, int particleMode)
{

    osg::Vec4 actCol;
    int idx = 0;
    //cerr << "name: " << (const char *)mapNames[currentMap].toAscii() << endl;

    float *map;
    int mapS;
    if (particleMode == Particles::M_PARTICLES)
    {
        map = mapValues.value(mapNames[currentMap]);
        mapS = mapSize.value(mapNames[currentMap]);
    }
    else
    {
        map = mapValues.value(mapNames[aCurrentMap]);
        mapS = mapSize.value(mapNames[aCurrentMap]);
    }
    if (map == NULL)
    {
        return actCol;
    }
    while (map[(idx + 1) * 5] <= pos)
    {
        idx++;
        if (idx > mapS - 2)
        {
            idx = mapS - 2;
            break;
        }
    }
    double d = (pos - map[idx * 5]) / (map[(idx + 1) * 5] - map[idx * 5]);
    actCol[0] = (float)((1 - d) * map[idx * 5 + 1] + d * map[(idx + 1) * 5 + 1]);
    actCol[1] = (float)((1 - d) * map[idx * 5 + 2] + d * map[(idx + 1) * 5 + 2]);
    actCol[2] = (float)((1 - d) * map[idx * 5 + 3] + d * map[(idx + 1) * 5 + 3]);
    actCol[3] = (float)((1 - d) * map[idx * 5 + 4] + d * map[(idx + 1) * 5 + 4]);

    return actCol;
}

//==========================================================================
// DESTRUCTOR
//==========================================================================

ParticleViewer::~ParticleViewer()
{

    coVRFileManager::instance()->unregisterFileHandler(&fileHandler[0]);
    coVRFileManager::instance()->unregisterFileHandler(&fileHandler[1]);
    coVRFileManager::instance()->unregisterFileHandler(&fileHandler[2]);
    coVRFileManager::instance()->unregisterFileHandler(&fileHandler[3]);
} // DESTRUCTOR

// Called before each frame
void ParticleViewer::preFrame()
{
}

void ParticleViewer::setTimestep(int t)
{
    for (std::list<Particles *>::iterator it = particles.begin(); it != particles.end(); it++)
    {
        (*it)->setTimestep(t);
    }
}

COVERPLUGIN(ParticleViewer)

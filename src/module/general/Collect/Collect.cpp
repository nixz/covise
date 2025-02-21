/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ++                                                        (C)2000 RUS  ++
// ++ Description: Filter program in COVISE API                           ++
// ++                                                                     ++
// ++ Author:                                                             ++
// ++                                                                     ++
// ++                            Andreas Werner                           ++
// ++               Computer Center University of Stuttgart               ++
// ++                            Allmandring 30                           ++
// ++                           70550 Stuttgart                           ++
// ++                                                                     ++
// ++ Date:  10.01.2000  V2.0                                             ++
// ++**********************************************************************/

// this includes our own class's headers
#include "Collect.h"
#include <do/coDoGeometry.h>
#include <do/coDoSet.h>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <string>
#include <vector>

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ++++
// ++++  Constructor : This will set up module port structure
// ++++
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Collect::Collect(int argc, char *argv[])
    : coModule(argc, argv, "Combine grid, normals, colors, textures and vertex "
                           "attributes in one data object for rendering")
{
    // Parameters

    // Ports
    p_grid = addInputPort("GridIn0", "StructuredGrid|UnstructuredGrid|"
                                     "RectilinearGrid|UniformGrid|Points|Spheres"
                                     "|Lines|Polygons|Quads|Triangles|TriangleStrips",
                          "Grid");

    p_color = addInputPort("DataIn0", "Byte|Float|Vec3|RGBA", "Colors or Scalar Data for Volume Visualization");
    p_color->setRequired(0);

    p_norm = addInputPort("DataIn1", "Vec3", "Normals");
    p_norm->setRequired(0);

    p_text = addInputPort("TextureIn0", "Texture", "Textures");
    p_text->setRequired(0);

    p_vertex = addInputPort("VertexAttribIn0", "Vec3|Float", "Vertex Attribute 0");
    p_vertex->setRequired(0);

    p_outPort = addOutputPort("GeometryOut0", "Geometry", "combined object");

#ifdef MATERIAL
    p_material = addMaterialParam("Material", "Material definition for Renderer");
#endif

    p_varName = addStringParam("varName", "name of variant");
    p_varName->setValue("");

    p_attribute = addStringParam("attribute", "attributes in the form name=value;name2=value2;...");
    p_attribute->setValue("");

    p_boundsMinParam = addFloatVectorParam("minBound", "minimum bound");
    p_boundsMinParam->setValue(0., 0., 0.);
    p_boundsMaxParam = addFloatVectorParam("maxBound", "maximum bound");
    p_boundsMaxParam->setValue(0., 0., 0.);
}

#ifdef MATERIAL
coDistributedObject *recCopyObject(const coDistributedObject *obj, const char *obj_name)
{
    char buf[128];
    const char **name, **content;
    int no_attrib;
    coDistributedObject *output_object = NULL;

    if (obj->isType("SETELE"))
    {
        int numObj, i;
        const coDistributedObject *const *parts = ((coDoSet *)obj)->getAllElements(&numObj);
        coDistributedObject **output_parts = new coDistributedObject *[numObj + 1];
        output_parts[numObj] = NULL;

        for (i = 0; i < numObj; i++)
        {
            sprintf(buf, "%s_%d", obj_name, i + 1);
            output_parts[i] = recCopyObject(parts[i], buf);
        }

        output_object = new coDoSet(obj_name, output_parts);
        no_attrib = ((coDoSet *)obj)->getAllAttributes(&name, &content);
        output_object->addAttributes(no_attrib, name, content);
    }
    else
    {
        output_object = obj->clone(obj_name);
    }

    return output_object;
}

void recAddMaterialAttrib(coDistributedObject *obj, const char *material)
{
    if (obj->isType("SETELE"))
    {
        int numObj, i;
        const coDistributedObject *const *parts = static_cast<coDoSet *>(obj)->getAllElements(&numObj);
        for (i = 0; i < numObj; i++)
            recAddMaterialAttrib(const_cast<coDistributedObject *>(parts[i]), material);
    }
    obj->addAttribute("MATERIAL", material);
}
#endif

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ++++
// ++++  compute() is called once for every EXECUTE message
/// ++++
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

int Collect::compute(const char *)
{
    const coDistributedObject *grid = p_grid->getCurrentObject();
    const coDistributedObject *color = p_color->getCurrentObject();
    const coDistributedObject *text = p_text->getCurrentObject();
    const coDistributedObject *norm = p_norm->getCurrentObject();
    const coDistributedObject *vertex = p_vertex->getCurrentObject();

#ifdef MATERIAL
    if (!grid)
        return FAIL; // should never get here...

    char buf[128];
    sprintf(buf, "%s_1", p_outPort->getObjName());
    coDistributedObject *copied_grid = recCopyObject(grid, buf);
    grid = copied_grid;

    // look up which value was chosen

    string datastring = "MAT: ";
    datastring.append(p_material->getValue());
#else
    grid->incRefCount(); // we re-use the grid
#endif

    if (!grid)
        return FAIL; // should never get here...

    coDoGeometry *geom = new coDoGeometry(p_outPort->getObjName(), grid);
    if (!geom || !geom->objectOk())
    {
        sendError("Could not create output object");
        return FAIL;
    }

    if (color)
    {
        geom->setColors(NONE, color);
        color->incRefCount();
    }

    if (text)
    {
        geom->setTexture(NONE, text);
        text->incRefCount();
    }

    if (norm)
    {
        geom->setNormals(NONE, norm);
        norm->incRefCount();
    }

    if (vertex)
    {
        geom->setVertexAttribute(NONE, vertex);
        vertex->incRefCount();
    }

    float minBound[3], maxBound[3];
    p_boundsMinParam->getValue(minBound[0], minBound[1], minBound[2]);
    p_boundsMaxParam->getValue(maxBound[0], maxBound[1], maxBound[2]);
    bool equal = true;
    for (int i = 0; i < 3; ++i)
    {
        if (minBound[i] != maxBound[i])
        {
            equal = false;
            break;
        }
    }
    if (!equal)
    {
        char bounds[100];
        snprintf(bounds, sizeof(bounds), "%f %f %f %f %f %f",
                 minBound[0], minBound[1], minBound[2],
                 maxBound[0], maxBound[1], maxBound[2]);
        geom->addAttribute("BOUNDING_BOX", bounds);
    }
    if (strlen(p_attribute->getValue()) > 0)
    {
        std::string param = p_attribute->getValue();
vector <string> fields;
boost::split( fields, param, boost::is_any_of( ";" ) );
for (size_t n = 0; n < fields.size(); n++)
{
vector <string> varVal;
boost::split( varVal, fields[n], boost::is_any_of( "=" ) );
if(varVal.size()==2)
{
geom->addAttribute(varVal[0].c_str(), varVal[1].c_str());
}
}
    }

    geom->addAttribute("VARIANT", p_varName->getValue());
    if (strlen(p_varName->getValue()) > 0)
    {
        geom->addAttribute("MODULE", "Variant");
    }

    if (!strstr(getTitle(), "Collect_") && !strstr(getTitle(), "Material"))
    {
        geom->addAttribute("OBJECTNAME", getTitle());
    }
    else if (const char *n = grid->getAttribute("OBJECTNAME"))
    {
        geom->addAttribute("OBJECTNAME", n);
    }
    else if (strlen(p_varName->getValue()) > 0)
    {
        geom->addAttribute("OBJECTNAME", p_varName->getValue());
    }

#ifdef MATERIAL
    // parameter there, non-empty and not 'none' ?
    if (!datastring.empty())
    {
        recAddMaterialAttrib(geom, datastring.c_str());
        recAddMaterialAttrib(copied_grid, datastring.c_str());
    }
#endif

    // finally, assign object to port
    p_outPort->setCurrentObject(geom);

    //   }
    return SUCCESS;
}

MODULE_MAIN(Tools, Collect)

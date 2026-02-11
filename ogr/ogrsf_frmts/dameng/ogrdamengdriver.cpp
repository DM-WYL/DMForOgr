/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDriver class.
 * Author:   YiLun Wu, wuyilun@dameng.com
 *
 ******************************************************************************
 * Copyright (c) 2024, YiLun Wu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

<<<<<<< HEAD:ogr/ogrsf_frmts/dameng/ogrdamengdriver.cpp
#include "ogr_dameng.h"
=======
#include "ogr_dm.h"
>>>>>>> 6015c004732898cb338d85f612307863e8cb27b0:ogr/ogrsf_frmts/dm/ogrdmdriver.cpp

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRDAMENGDriverIdentify()                        */
/************************************************************************/

static int OGRDAMENGDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "DAMENG:");
}

/************************************************************************/
/*                           OGRDAMENGDriverOpen()                          */
/************************************************************************/

static GDALDataset *OGRDAMENGDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGRDAMENGDriverIdentify(poOpenInfo))
        return nullptr;

    OGRDAMENGDataSource *poDS;

    poDS = new OGRDAMENGDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update,
                    TRUE, poOpenInfo->papszOpenOptions))
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                         OGRDAMENGDriverCreate()                          */
/************************************************************************/

static GDALDataset *OGRDAMENGDriverCreate(const char *pszName,
                                      CPL_UNUSED int nBands,
                                      CPL_UNUSED int nXSize,
                                      CPL_UNUSED int nYSize,
                                      CPL_UNUSED GDALDataType eDT,
                                      char **papszOptions)

{
    OGRDAMENGDataSource *poDS;

    poDS = new OGRDAMENGDataSource();

    if (!poDS->Open(pszName, TRUE, TRUE, papszOptions))
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "DaMeng driver doesn't currently support database creation.\n"
                 "Please create database with the DM tools before loading "
                 "tables.");
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRDAMENG()                            */
/************************************************************************/

void RegisterOGRDAMENG()

{
<<<<<<< HEAD:ogr/ogrsf_frmts/dameng/ogrdamengdriver.cpp
    if (!GDAL_CHECK_VERSION("OGR/DAMENG driver"))
=======
    if (!GDAL_CHECK_VERSION("OGR/DM driver"))
>>>>>>> 6015c004732898cb338d85f612307863e8cb27b0:ogr/ogrsf_frmts/dm/ogrdmdriver.cpp
        return;

    if (GDALGetDriverByName("DAMENG") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

<<<<<<< HEAD:ogr/ogrsf_frmts/dameng/ogrdamengdriver.cpp
    poDriver->SetDescription("DAMENG");
=======
    poDriver->SetDescription("DM");
>>>>>>> 6015c004732898cb338d85f612307863e8cb27b0:ogr/ogrsf_frmts/dm/ogrdmdriver.cpp
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "DMGEO2");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "");
    poDriver->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, "DAMENG:");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='DBNAME' type='string' description='Database name'/>"
        "  <Option name='USER' type='string' description='User name'/>"
        "  <Option name='PASSWORD' type='string' description='Password'/>"
        "  <Option name='TABLES' type='string' description='Restricted set of "
        "tables to list (comma separated)'/>"
        "  <Option name='INSERTNUM' type='boolean' description='Whether all "
        "tables, including non-spatial ones, should be listed' default='NO'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
                              "<CreationOptionList/>");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='GEOM_TYPE' type='string-select' description='Format "
        "of geometry columns' default='geometry'>"
        "    <Value>geometry</Value>"
        "    <Value>geography</Value>"
        "  </Option>"
        "  <Option name='OVERWRITE' type='boolean' description='Whether to "
        "overwrite an existing table with the layer name to be created' "
        "default='NO'/>"
        "  <Option name='LAUNDER' type='boolean' description='Whether layer "
        "and field names will be laundered' default='YES'/>"
        "  <Option name='PRECISION' type='boolean' description='Whether fields "
        "created should keep the width and precision' default='YES'/>"
        "  <Option name='DIM' type='string' description='Set to 2 to force the "
        "geometries to be 2D, 3 to be 2.5D, XYM or XYZM'/>"
        "  <Option name='GEOMETRY_NAME' type='string' description='Name of "
        "geometry column. Defaults to wkb_geometry for GEOM_TYPE=geometry or "
        "the_geog for GEOM_TYPE=geography'/>"
        "  <Option name='FID' type='string' description='Name of the FID "
        "column to create' default='ogc_fid'/>"
        "  <Option name='FID64' type='boolean' description='Whether to create "
        "the FID column with BIGSERIAL type to handle 64bit wide ids' "
        "default='NO'/>"
        "  <Option name='DESCRIPTION' type='string' description='Description "
        "string to put in the all_tab_comments system table'/>"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String Date DateTime "
                              "Time Binary");
    poDriver->SetMetadataItem(GDAL_DMD_ALTER_FIELD_DEFN_FLAGS,
        "Name Type WidthPrecision Nullable Default Unique Comment");
    poDriver->SetMetadataItem(GDAL_DCAP_NOTNULL_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DEFAULT_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_NOTNULL_GEOMFIELDS, "YES");

<<<<<<< HEAD:ogr/ogrsf_frmts/dameng/ogrdamengdriver.cpp
    poDriver->pfnIdentify = OGRDAMENGDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->pfnOpen = OGRDAMENGDriverOpen;
    poDriver->pfnCreate = OGRDAMENGDriverCreate;
=======
    poDriver->pfnIdentify = OGRDMDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->pfnOpen = OGRDMDriverOpen;
    poDriver->pfnCreate = OGRDMDriverCreate;
>>>>>>> 6015c004732898cb338d85f612307863e8cb27b0:ogr/ogrsf_frmts/dm/ogrdmdriver.cpp

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

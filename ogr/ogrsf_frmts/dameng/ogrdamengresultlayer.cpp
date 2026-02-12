/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRDAMENGResultLayer class.
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

#include "cpl_conv.h"
#include "ogr_dameng.h"

/************************************************************************/
/*                          OGRDAMENGResultLayer()                          */
/************************************************************************/

OGRDAMENGResultLayer::OGRDAMENGResultLayer(OGRDAMENGDataSource *poDSIn,
                                   const char *pszRawQueryIn,
                                   OGRDAMENGStatement *hInitialResultIn) : pszRawStatement(CPLStrdup(pszRawQueryIn))
{
    poDS = poDSIn;

    iNextShapeId = 0;

    BuildFullQueryStatement();

    ReadResultDefinition(hInitialResultIn);

    /* Find at which index the geometry column is */
    /* and prepare a request to identify not-nullable fields */
    int iGeomCol = -1;
    for (int iRawField = 0; iRawField < hInitialResultIn->GetColCount();
         iRawField++)
    {
        char char_attr[200];
        sdint2 char_len;
        slength numeric_attr = 0;
        dpi_col_attr(*hInitialResultIn->GetStatement(), ((udint2)iRawField + 1),
                     DSQL_DESC_BASE_COLUMN_NAME, char_attr, sizeof(char_attr),
                     &char_len, &numeric_attr);
        if (poFeatureDefn->GetGeomFieldCount() == 1 &&
            strcmp(char_attr,
                   poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef()) == 0)
        {
            iGeomCol = iRawField;
        }
    }

    /* Determine the table from which the geometry column is extracted */
    if (iGeomCol != -1)
    {
        char char_attr[200];
        sdint2 char_len;
        slength numeric_attr = 0;
        dpi_col_attr(*hInitialResultIn->GetStatement(), ((udint2)iGeomCol + 1),
                     DSQL_DESC_BASE_TABLE_NAME, char_attr, sizeof(char_attr),
                     &char_len, &numeric_attr);
        pszGeomTableName = CPLStrdup(char_attr);
        dpi_col_attr(*hInitialResultIn->GetStatement(), ((udint2)iGeomCol + 1),
                     DSQL_DESC_SCHEMA_NAME, char_attr, sizeof(char_attr),
                     &char_len, &numeric_attr);
        pszGeomTableSchemaName = CPLStrdup(char_attr);
    }
}

/************************************************************************/
/*                          ~OGRDAMENGResultLayer()                          */
/************************************************************************/

OGRDAMENGResultLayer::~OGRDAMENGResultLayer()

{
    CPLFree(pszRawStatement);
    CPLFree(pszGeomTableName);
    CPLFree(pszGeomTableSchemaName);
}

/************************************************************************/
/*                      BuildFullQueryStatement()                       */
/************************************************************************/

void OGRDAMENGResultLayer::BuildFullQueryStatement()

{
    if (pszQueryStatement != nullptr)
    {
        CPLFree(pszQueryStatement);
        pszQueryStatement = nullptr;
    }

    const size_t nLen = strlen(pszRawStatement) + osWHERE.size() + 40;
    pszQueryStatement = static_cast<char *>(CPLMalloc(nLen));

    if (osWHERE.empty())
        strcpy(pszQueryStatement, pszRawStatement);
    else
        snprintf(pszQueryStatement, nLen,
                 "SELECT * FROM (%s) AS ogrdamengsubquery %s", pszRawStatement,
                 osWHERE.c_str());
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRDAMENGResultLayer::ResetReading()

{
    OGRDAMENGLayer::ResetReading();
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRDAMENGResultLayer::GetFeatureCount(int bForce)

{
    if (TestCapability(OLCFastFeatureCount) == FALSE)
        return OGRDAMENGLayer::GetFeatureCount(bForce);

    OGRDAMENGConn *hDAMENGConn = poDS->GetDAMENGConn();
    char **hResult = nullptr;
    OGRDAMENGStatement oCommand(hDAMENGConn);
    CPLString osCommand;
    int nCount = 0;

    osCommand.Printf("SELECT count(*) FROM (%s) AS ogrdamengcount",
                     pszQueryStatement);

    CPLErr rt = oCommand.Execute(osCommand.c_str());
    hResult = oCommand.SimpleFetchRow();
    if (rt == CE_None && hResult != nullptr)
        nCount = atoi(hResult[0]);
    else
        CPLDebug("DAMENG", "%s; failed.", osCommand.c_str());

    return nCount;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDAMENGResultLayer::TestCapability(const char *pszCap) const

{
    GetLayerDefn();

    if (EQUAL(pszCap, OLCFastFeatureCount) ||
        EQUAL(pszCap, OLCFastSetNextByIndex))
    {
        OGRDAMENGGeomFieldDefn *poGeomFieldDefn = nullptr;
        if (poFeatureDefn->GetGeomFieldCount() > 0)
            poGeomFieldDefn =
                poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter);
        return (m_poFilterGeom == nullptr || poGeomFieldDefn == nullptr ||
                poGeomFieldDefn->eDAMENGGeoType == GEOM_TYPE_GEOMETRY ||
                poGeomFieldDefn->eDAMENGGeoType == GEOM_TYPE_GEOGRAPHY) &&
               m_poAttrQuery == nullptr;
    }
    else if (EQUAL(pszCap, OLCFastSpatialFilter))
    {
        OGRDAMENGGeomFieldDefn *poGeomFieldDefn = nullptr;
        if (poFeatureDefn->GetGeomFieldCount() > 0) 
            poGeomFieldDefn =
                poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter);
        return (poGeomFieldDefn == nullptr ||
                poGeomFieldDefn->eDAMENGGeoType == GEOM_TYPE_GEOMETRY ||
                poGeomFieldDefn->eDAMENGGeoType == GEOM_TYPE_GEOGRAPHY) &&
               m_poAttrQuery == nullptr;
    }

    else if (EQUAL(pszCap, OLCFastGetExtent))
    {
        OGRDAMENGGeomFieldDefn *poGeomFieldDefn = nullptr;
        if (poFeatureDefn->GetGeomFieldCount() > 0)
            poGeomFieldDefn = poFeatureDefn->GetGeomFieldDefn(0);
        return (poGeomFieldDefn == nullptr ||
                poGeomFieldDefn->eDAMENGGeoType == GEOM_TYPE_GEOMETRY) &&
               m_poAttrQuery == nullptr;
    }
    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRDAMENGResultLayer::GetNextFeature()

{
    OGRDAMENGGeomFieldDefn *poGeomFieldDefn = nullptr;
    if (poFeatureDefn->GetGeomFieldCount() != 0)
        poGeomFieldDefn = poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter);

    while (true)
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if (poFeature == nullptr)
            return nullptr;

        if ((m_poFilterGeom == nullptr || poGeomFieldDefn == nullptr ||
             poGeomFieldDefn->eDAMENGGeoType == GEOM_TYPE_GEOMETRY ||
             poGeomFieldDefn->eDAMENGGeoType == GEOM_TYPE_GEOGRAPHY ||
             FilterGeometry(poFeature->GetGeomFieldRef(m_iGeomFieldFilter))) &&
            (m_poAttrQuery == nullptr || m_poAttrQuery->Evaluate(poFeature)))
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

OGRErr OGRDAMENGResultLayer::ISetSpatialFilter(int iGeomField,
                                               const OGRGeometry *poGeomIn)

{
    if (iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        CPLAssertNotNull(GetLayerDefn()->GetGeomFieldDefn(iGeomField))
                ->GetType() == wkbNone)
    {
        if (iGeomField != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_NONE;
    }
    m_iGeomFieldFilter = iGeomField;

    OGRDAMENGGeomFieldDefn *poGeomFieldDefn =
        poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter);
    if (InstallFilter(poGeomIn))
    {
        if (poGeomFieldDefn->eDAMENGGeoType == GEOM_TYPE_GEOMETRY)
        {
            if (m_poFilterGeom != nullptr)
            {
                char szBox3D_1[128];
                char szBox3D_2[128];
                char szBox3D_3[128];
                char szBox3D_4[128];
                OGREnvelope sEnvelope;

                m_poFilterGeom->getEnvelope(&sEnvelope);
                CPLsnprintf(szBox3D_1, sizeof(szBox3D_1), "%.18g %.18g",
                            sEnvelope.MinX, sEnvelope.MinY);
                CPLsnprintf(szBox3D_2, sizeof(szBox3D_2), "%.18g %.18g",
                            sEnvelope.MinX, sEnvelope.MaxY);
                CPLsnprintf(szBox3D_3, sizeof(szBox3D_2), "%.18g %.18g",
                            sEnvelope.MaxX, sEnvelope.MaxY);
                CPLsnprintf(szBox3D_4, sizeof(szBox3D_2), "%.18g %.18g",
                            sEnvelope.MaxX, sEnvelope.MinY);
                osWHERE.Printf("WHERE "
                               "DMGEO2.ST_BOXCONTAINS(dmgeo2.st_geomfromtext('"
                               "POLYGON(( %s, %s, %s, %s, %s))'), %s);",
                               szBox3D_1, szBox3D_2, szBox3D_3, szBox3D_4,
                               szBox3D_1, OGRDAMENGEscapeColumnName(poGeomFieldDefn->GetNameRef())
                                    .c_str());
            }
            else
            {
                osWHERE = "";
            }

            BuildFullQueryStatement();
        }

        ResetReading();
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                            ResolveSRID()                             */
/************************************************************************/

void OGRDAMENGResultLayer::ResolveSRID(const OGRDAMENGGeomFieldDefn *poGFldDefn)

{
    /* We have to get the SRID of the geometry column, so to be able */
    /* to do spatial filtering */
    int nSRSId = UNDETERMINED_SRID;
    OGRDAMENGConn *hDAMENGConn = poDS->GetDAMENGConn();
    OGRDAMENGStatement oCommand(hDAMENGConn);

    if (nSRSId == UNDETERMINED_SRID &&
        (poGFldDefn->eDAMENGGeoType == GEOM_TYPE_GEOMETRY ||
         poGFldDefn->eDAMENGGeoType == GEOM_TYPE_GEOGRAPHY))
    {
        if (pszGeomTableName != nullptr)
        {
            CPLString osName(pszGeomTableSchemaName);
            osName += ".";
            osName += pszGeomTableName;
            OGRDAMENGLayer *poBaseLayer =
                cpl::down_cast<OGRDAMENGLayer *>(poDS->GetLayerByName(osName));
            if (poBaseLayer)
            {
                int iBaseIdx = poBaseLayer->GetLayerDefn()->GetGeomFieldIndex(
                    poGFldDefn->GetNameRef());
                if (iBaseIdx >= 0)
                {
                    const OGRDAMENGGeomFieldDefn *poBaseGFldDefn =
                        poBaseLayer->GetLayerDefn()->GetGeomFieldDefn(iBaseIdx);
                    poBaseGFldDefn
                        ->GetSpatialRef(); /* To make sure nSRSId is resolved */
                    nSRSId = poBaseGFldDefn->nSRSId;
                }
            }
        }

        if (nSRSId == UNDETERMINED_SRID || nSRSId == 0)
        {
            CPLString osGetSRID;

            if (STARTS_WITH_CI(poGFldDefn->GetNameRef(), "DMGEO2.") == TRUE)
            {
                nSRSId = 0;
            }
            else
            {
                const char *psGetSRIDFct = "DMGEO2.ST_SRID";
                osGetSRID += "SELECT ";
                osGetSRID += psGetSRIDFct;
                osGetSRID += "(";
                osGetSRID += OGRDAMENGEscapeColumnName(poGFldDefn->GetNameRef());
                osGetSRID += ") FROM (";
                osGetSRID += pszRawStatement;
                osGetSRID += ") AS ogrdamenggetsrid WHERE (";
                osGetSRID += OGRDAMENGEscapeColumnName(poGFldDefn->GetNameRef());
                osGetSRID += " IS NOT NULL) LIMIT 1";

                CPLErr err = oCommand.Execute(osGetSRID.c_str());
                char **results = oCommand.SimpleFetchRow();
                char *srid;
                if (results && err == CE_None)
                    srid = results[0];
                else
                    srid = nullptr;

                nSRSId = poDS->GetUndefinedSRID();

                if (err == CE_None && srid != nullptr)
                {
                    nSRSId = atoi(srid);
                }
            }
        }
    }
    poGFldDefn->nSRSId = nSRSId;
}

/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
<<<<<<< HEAD:ogr/ogrsf_frmts/dameng/ogrdamengconnection.cpp
 * Purpose:  Implements OGRDAMENGConnection class.
=======
 * Purpose:  Implements OGRDMConnection class.
>>>>>>> 6015c004732898cb338d85f612307863e8cb27b0:ogr/ogrsf_frmts/dm/ogrdmconnection.cpp
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

#include "ogr_dameng.h"
#include "cpl_conv.h"

/************************************************************************/
/*                          OGRGetDAMENGConnection()                        */
/************************************************************************/

OGRDAMENGConn* OGRGetDAMENGConnection(const char* pszUserid,
                              const char* pszPassword,
                              const char* pszDatabase,
                              const char *pszSchemaName)
{
    OGRDAMENGConn *poConnection;

    poConnection = new OGRDAMENGConn();
    if (poConnection->EstablishConn(pszUserid, pszPassword, pszDatabase, pszSchemaName))
        return poConnection;
    else
    {
        delete poConnection;
        return nullptr;
    }
}

/************************************************************************/
/*                          OGRDAMENGSession()                              */
/************************************************************************/

OGRDAMENGConn::OGRDAMENGConn()
{
    hEnv = nullptr;
    hRtn = 0;
    hCon = nullptr;
    pszUserid = nullptr;
    pszPassword = nullptr;
    pszDatabase = nullptr;
}

/************************************************************************/
/*                          ~OGRDAMENGSession()                             */
/************************************************************************/

OGRDAMENGConn::~OGRDAMENGConn()
{
    DPIRETURN rt;
    rt = dpi_commit(hCon);
    if (!DSQL_SUCCEEDED(rt))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to commit!");
    }
    dpi_logout(hCon);
    dpi_free_con(hCon);
    dpi_free_env(hEnv);
    CPLFree(pszUserid);
    CPLFree(pszPassword);
    CPLFree(pszDatabase);
}

/************************************************************************/
/*                          EstablishSession()                          */
/************************************************************************/
int OGRDAMENGConn::EstablishConn(const char* pszUseridIn,
                             const char* pszPasswordIn,
                             const char *pszDatabaseIn,
                             const char *pszSchemaName)
{
    DPIRETURN rt;

    rt = dpi_alloc_env(&hEnv);
    if (!DSQL_SUCCEEDED(rt))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "failed to alloc environment handle!");
        return FALSE;
    }

    rt = dpi_alloc_con(hEnv, &hCon);
    if (!DSQL_SUCCEEDED(rt))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "failed to alloc connection handle");
        return FALSE;
    }

    if (strlen(pszSchemaName))
    {
        rt = dpi_set_con_attr(hCon, DSQL_ATTR_CURRENT_SCHEMA,
                              (sdbyte *)pszSchemaName, strlen(pszSchemaName));
        if (!DSQL_SUCCEEDED(rt))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "failed to set con_attr");
            return FALSE;
        }
    }
    
    rt = dpi_login(hCon, (sdbyte *)pszDatabaseIn, (sdbyte *)pszUseridIn,
                   (sdbyte *)pszPasswordIn);
    if (!DSQL_SUCCEEDED(rt))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to login");
        return FALSE;
    }
    rt = dpi_set_con_attr(hCon, DSQL_ATTR_AUTOCOMMIT, DSQL_AUTOCOMMIT_OFF, 0);
    if (!DSQL_SUCCEEDED(rt))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to set con_attr");
        return FALSE;
    }

    pszUserid = CPLStrdup(pszUseridIn);
    pszPassword = CPLStrdup(pszPasswordIn);
    pszDatabase = CPLStrdup(pszDatabaseIn);

    return TRUE;
}

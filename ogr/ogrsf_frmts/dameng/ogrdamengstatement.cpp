/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRDAMENGStatement class.
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
#include <ogr_p.h>
#include <cmath>

OGRDAMENGStatement::OGRDAMENGStatement(OGRDAMENGConn *poConnIn)
    : hStatement(nullptr), insert_objdesc(nullptr)
{
    poConn = poConnIn;
    result = nullptr;
    papszCurImage = nullptr;
    pszCommandText = nullptr;
    object_index = nullptr;
    objdesc = nullptr;
    obj = nullptr;
    lob = nullptr;
    lobs = nullptr;
    lob_index = nullptr;
    blob_len = nullptr;
    nRawColumnCount = 0;
    is_fectmany = 0;
    insert_objs = nullptr;
    insert_geovalues = nullptr;
    insert_values = nullptr;
    geonum = 0;
    valuesnum = 0;
    results = nullptr;
    objs = nullptr;
    objdescs = nullptr;
    blob_lens = nullptr;
    papszCurImages = nullptr;
}

OGRDAMENGStatement::~OGRDAMENGStatement()

{
    Clean();
}

void OGRDAMENGStatement::Clean()

{
    if (insert_num > 0)
    {
        if (!DSQL_SUCCEEDED(dpi_set_stmt_attr(
                hStatement, DSQL_ATTR_PARAMSET_SIZE,
                reinterpret_cast<dpointer>(static_cast<uintptr_t>(insert_num)),
                0)))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "failed to set stmt paramset size");
            return;
        }
        if (!DSQL_SUCCEEDED(dpi_exec(hStatement)))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "failed to exectue");
            return;
        }
        for (int iparam = 0; iparam < geonum; iparam++)
        {
            for (int num = 0; num < insert_num; num++)
            {
                CPLFree(insert_geovalues[iparam][num]);
            }
        }
        insert_num = 0;
    }
    if (!DSQL_SUCCEEDED(dpi_commit((poConn->hCon))))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to commit");
        return;
    }
    if (pszCommandText)
        CPLFree(pszCommandText);
    pszCommandText = nullptr;
    if (is_fectmany == 0)
    {
        if (result)
        {
            for (int i = 0; result[i] != nullptr; i++)
            {
                CPLFree(result[i]);
                CPLFree(col_len[i]);
                if (object_index[i])
                {
                    if (!DSQL_SUCCEEDED(dpi_free_obj(obj[i])))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "failed to free obj");
                    }
                    if (!DSQL_SUCCEEDED(dpi_free_obj_desc(objdesc[i])))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "failed to free objdesc");
                    }
                }
                else if (lob_index[i])
                {
                    if (!DSQL_SUCCEEDED(dpi_free_lob_locator(lob[i])))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "failed to free lob");
                    }
                }
            }
            CPLFree(result);
            CPLFree(obj);
            CPLFree(objdesc);
            CPLFree(lob);
            CPLFree(blob_len);
            CPLFree(col_len);
        }
        if (papszCurImage)
            CPLFree(papszCurImage);
    }
    else
    {
        if (results)
        {
            for (int col = 0; results[col] != nullptr; col++)
            {

                if (object_index[col])
                {
                    for (int row = 0; row < fetchnum; row++)
                    {
                        if (results[col][row])
                            CPLFree(results[col][row]);
                        if (!DSQL_SUCCEEDED(dpi_free_obj(objs[col][row])))
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "failed to free obj");
                        }
                    }
                    if (!DSQL_SUCCEEDED(dpi_free_obj_desc(objdescs[col][0])))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "failed to free objdesc");
                    }
                }
                else if (lob_index[col])
                {
                    for (int row = 0; row < fetchnum; row++)
                    {
                        if (results[col][row])
                            CPLFree(results[col][row]);
                        if (!DSQL_SUCCEEDED(
                                dpi_free_lob_locator(lobs[col][row])))
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "failed to free lob");
                        }
                    }
                }
                else
                {
                    if (results[col][0])
                        CPLFree(results[col][0]);
                }
                CPLFree(results[col]);
                CPLFree(col_len[col]);
                CPLFree(objs[col]);
                CPLFree(objdescs[col]);
                CPLFree(lobs[col]);
                CPLFree(blob_lens[col]);
                if (papszCurImages && papszCurImages[col])
                    CPLFree(papszCurImages[col]);
            }
            CPLFree(results);
            CPLFree(col_len);
            CPLFree(objs);
            CPLFree(objdescs);
            CPLFree(lobs);
            CPLFree(blob_lens);
            if (papszCurImages)
                CPLFree(papszCurImages);
        }
    }
    if (object_index)
        CPLFree(object_index);
    if (lob_index)
        CPLFree(lob_index);
    object_index = nullptr;
    objdesc = nullptr;
    obj = nullptr;
    lob = nullptr;
    lob_index = nullptr;
    blob_len = nullptr;
    nRawColumnCount = 0;
    is_fectmany = 0;
    results = nullptr;
    objs = nullptr;
    objdescs = nullptr;
    blob_lens = nullptr;
    col_len = nullptr;
    papszCurImages = nullptr;
    papszCurImage = nullptr;

    if (hStatement)
    {
        if (!DSQL_SUCCEEDED(dpi_free_stmt(hStatement)))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "failed to free stmt");
        }
        hStatement = nullptr;
    }
}

CPLErr OGRDAMENGStatement::Prepare(const char *pszSQLstatement)

{
    Clean();

    CPLDebug("DAMENG", "Prepare(%s)", pszSQLstatement);

    pszCommandText = CPLStrdup(pszSQLstatement);

    if (hStatement != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Statement already executed once on this OGRDAMENGStatement.");
        return CE_Failure;
    }

    if (!DSQL_SUCCEEDED(dpi_alloc_stmt((poConn->hCon), &hStatement)))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to alloc statement");
        return CE_Failure;
    }

    if (strstr(pszCommandText, "\"\"") != nullptr)
    {
        int i = 0;
        while (pszCommandText[i + 1] != '\0')
        {
            if (pszCommandText[i] == '\"' && pszCommandText[i + 1] == '\"')
            {
                if (pszCommandText[i - 1] == ' ')
                    pszCommandText[i] = ' ';
                else if (pszCommandText[i + 2] == ' ' ||
                         pszCommandText[i + 2] == '\0' ||
                         pszCommandText[i + 2] == ')')
                    pszCommandText[i + 1] = ' ';
            }
            i++;
        }
    }

    if (!DSQL_SUCCEEDED(dpi_prepare(
            hStatement, reinterpret_cast<sdbyte *>(pszCommandText))))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to prepare, %s",
                 pszSQLstatement);
        return CE_Failure;
    }
    return CE_None;
}

CPLErr OGRDAMENGStatement::Execute_for_insert(OGRDAMENGFeatureDefn *params,
                                              OGRFeature *poFeature,
                                              std::map<std::string, int> mymap)
{
    int i = 0;
    int bind_flag = 0;
    if (paramdescs == nullptr)
    {
        bind_flag = 1;
        if (!DSQL_SUCCEEDED(dpi_set_stmt_attr(hStatement,
                                              DSQL_ATTR_PARAMSET_SIZE,
                                              (void *)FORCED_INSERT_NUM, 0)))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "failed to set stmt paramset size");
            return CE_Failure;
        }

        if (!DSQL_SUCCEEDED(dpi_number_params(
                hStatement, reinterpret_cast<udint2 *>(&param_nums))))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "failed to get params numbers");
            return CE_Failure;
        }
        paramdescs = reinterpret_cast<DmColDesc *>(
            CPLCalloc(sizeof(DmColDesc), param_nums));
        for (udint2 iparam = 0; iparam < param_nums; iparam++)
        {
            if (!DSQL_SUCCEEDED(dpi_desc_param(
                    hStatement, iparam + 1, &paramdescs[iparam].sql_type,
                    &paramdescs[iparam].prec, &paramdescs[iparam].scale,
                    &paramdescs[iparam].nullable)))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "failed to get param desc");
                return CE_Failure;
            }
            if (paramdescs[iparam].sql_type == DSQL_CLASS)
                i++;
        }

        geonum = i;
        insert_objs = reinterpret_cast<dhobj **>(CPLCalloc(sizeof(dhobj *), i));
        for (int num = 0; num < i; num++)
        {
            insert_objs[num] = reinterpret_cast<dhobj *>(
                CPLCalloc(sizeof(dhobj), FORCED_INSERT_NUM));
        }
        if (i > 0)
        {
            dhdesc hdesc_param;
            sdint4 val_len;
            if (!DSQL_SUCCEEDED(
                    dpi_get_stmt_attr(hStatement, DSQL_ATTR_IMP_PARAM_DESC,
                                      (dpointer)&hdesc_param, 0, &val_len)))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "failed to get row_desc");
                return CE_Failure;
            }
            if (!DSQL_SUCCEEDED(dpi_get_desc_field(
                    hdesc_param, (sdint2)1, DSQL_DESC_OBJ_DESCRIPTOR,
                    &insert_objdesc, sizeof(dhobjdesc), NULL)))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "failed to get geometry desc");
                return CE_Failure;
            }
        }
        for (int iparam = 0; iparam < i; iparam++)
        {
            for (int num = 0; num < FORCED_INSERT_NUM; num++)
            {
                if (!DSQL_SUCCEEDED(dpi_alloc_obj((poConn->hCon),
                                                  &insert_objs[iparam][num])))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "failed to alloc obj");
                    return CE_Failure;
                }
                if (!DSQL_SUCCEEDED(dpi_bind_obj_desc(insert_objs[iparam][num],
                                                      insert_objdesc)))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "failed to bind obj desc");
                    return CE_Failure;
                }
            }
        }
        insert_geovalues = reinterpret_cast<GSERIALIZED ***>(
            CPLCalloc(sizeof(GSERIALIZED **), i));
        for (int iparam = 0; iparam < i; iparam++)
        {
            insert_geovalues[iparam] = reinterpret_cast<GSERIALIZED **>(
                CPLCalloc(sizeof(GSERIALIZED *), FORCED_INSERT_NUM));
        }

        valuesnum = param_nums - i;
        insert_values =
            reinterpret_cast<char ***>(CPLCalloc(sizeof(char **), valuesnum));
        for (int iparam = 0; iparam < valuesnum; iparam++)
        {
            insert_values[iparam] = reinterpret_cast<char **>(
                CPLCalloc(sizeof(char *), FORCED_INSERT_NUM));
            char *date =
                reinterpret_cast<char *>(CPLMalloc(8192 * FORCED_INSERT_NUM));
            for (int num = 0; num < FORCED_INSERT_NUM; num++)
            {
                insert_values[iparam][num] = date + 8192 * num;
            }
        }
    }
    i = 0;
    for (udint2 num = 0; num < geonum; num++)
    {

        OGRDAMENGGeomFieldDefn *poGeomFieldDefn = params->GetGeomFieldDefn(i);
        OGRGeometry *poGeom = poFeature->GetGeomFieldRef(i);
        char s[100];
        strncpy(s, poGeomFieldDefn->GetNameRef(), 99);
        s[99] = '\0';
        if (mymap[s] == num + 1 && poGeom != nullptr)
        {
            if (i < params->GetGeomFieldCount())
                i++;
            if (poGeomFieldDefn->eDAMENGGeoType == GEOM_TYPE_GEOGRAPHY ||
                poGeomFieldDefn->eDAMENGGeoType == GEOM_TYPE_GEOMETRY)
            {
                poGeom->closeRings();
                poGeom->set3D(poGeomFieldDefn->GeometryTypeFlags &
                              OGRGeometry::OGR_G_3D);
                poGeom->setMeasured(poGeomFieldDefn->GeometryTypeFlags &
                                    OGRGeometry::OGR_G_MEASURED);

                int nSRSId = poGeomFieldDefn->nSRSId;

                if (!CPLTestBool(CPLGetConfigOption("DM_USE_TEXT", "NO")))
                {
                    OGREnvelope3D Envelope;
                    poGeom->getEnvelope(&Envelope);
                    char *pszHexEWKB =
                        OGRGeometryToHexEWKB(poGeom, nSRSId, 3, 3);
                    insert_geovalues[num][insert_num] =
                        OGRDAMENGGeo_From_Hexwkb(pszHexEWKB, &gser_length,
                                                 Envelope);
                    CPLFree(pszHexEWKB);
                    if (!DSQL_SUCCEEDED(dpi_set_obj_val(
                            insert_objs[num][insert_num], 1, DSQL_C_BINARY,
                            insert_geovalues[num][insert_num], gser_length)))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "failed to set obj val");
                        return CE_Failure;
                    }
                    if (bind_flag == 1)
                    {
                        if (!DSQL_SUCCEEDED(dpi_bind_param(
                                hStatement, num + 1, DSQL_PARAM_INPUT,
                                DSQL_C_CLASS, DSQL_CLASS, paramdescs[num].prec,
                                paramdescs[num].scale, &insert_objs[num][0],
                                sizeof(dhobj), NULL)))
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "failed to bind param");
                            return CE_Failure;
                        }
                    }
                }
            }
        }
        else
        {
            if (!DSQL_SUCCEEDED(dpi_set_obj_val(insert_objs[num][insert_num], 1,
                                                DSQL_C_BINARY, nullptr, 0)))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "failed to set obj val");
                return CE_Failure;
            }
            if (bind_flag == 1)
            {
                if (!DSQL_SUCCEEDED(dpi_bind_param(
                        hStatement, num + 1, DSQL_PARAM_INPUT, DSQL_C_CLASS,
                        DSQL_CLASS, paramdescs[num].prec, paramdescs[num].scale,
                        &insert_objs[num][0], sizeof(dhobj), NULL)))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "failed to bind param");
                    return CE_Failure;
                }
            }
        }
    }
    i = 0;
    for (int num = 0; num < valuesnum; num++)
    {

        const OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();
        OGRFieldType nOGRFieldType = poFeatureDefn->GetFieldDefn(i)->GetType();
        char s[100];
        strncpy(s, poFeatureDefn->GetFieldDefn(i)->GetNameRef(), 99);
        s[99] = '\0';
        if (mymap[strToupper(s)] == num + geonum + 1)
        {

            strncpy(insert_values[num][insert_num],
                    poFeature->GetFieldAsString(i), 8192);
            insert_values[num][insert_num][8192] = '\0';
            if (i < params->GetFieldCount())
                i++;
            // Check if date is NULL: 0000-00-00
            if (nOGRFieldType == OFTDate)
            {
                if (STARTS_WITH_CI(insert_values[num][insert_num], "0000"))
                {
                    strncpy(insert_values[num][insert_num], "NULL\0", 8192);
                }
            }
            else if (nOGRFieldType == OFTReal)
            {
                //Check for special values. They need to be quoted.
                double dfVal = poFeature->GetFieldAsDouble(i);
                if (std::isnan(dfVal))
                    strncpy(insert_values[num][insert_num], "'NaN'", 8192);
            }
            if (bind_flag == 1)
            {
                if (!DSQL_SUCCEEDED(
                        dpi_bind_param(hStatement, (udint2)(num + geonum + 1),
                                       DSQL_PARAM_INPUT, DSQL_C_NCHAR,
                                       paramdescs[num + geonum].sql_type,
                                       paramdescs[num + geonum].prec,
                                       paramdescs[num + geonum].scale,
                                       insert_values[num][0], 8192, NULL)))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "failed to bind param");
                    return CE_Failure;
                }
            }
        }
        else
        {
            strncpy(insert_values[num][insert_num], "", 8192);
            if (bind_flag == 1)
            {
                if (!DSQL_SUCCEEDED(
                        dpi_bind_param(hStatement, (udint2)(num + geonum + 1),
                                       DSQL_PARAM_INPUT, DSQL_C_NCHAR,
                                       paramdescs[num + geonum].sql_type,
                                       paramdescs[num + geonum].prec,
                                       paramdescs[num + geonum].scale,
                                       insert_values[num][0], 8192, NULL)))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "failed to bind param");
                    return CE_Failure;
                }
            }
        }
    }
    insert_num++;
    if (insert_num < FORCED_INSERT_NUM)
        return CE_None;
    if (!DSQL_SUCCEEDED(dpi_exec(hStatement)))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to exectue");
        return CE_Failure;
    }
    if (!DSQL_SUCCEEDED(dpi_commit((poConn->hCon))))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to commit");
        return CE_Failure;
    }
    insert_num = 0;
    for (int iparam = 0; iparam < geonum; iparam++)
    {
        for (int num = 0; num < FORCED_INSERT_NUM; num++)
        {
            CPLFree(insert_geovalues[iparam][num]);
        }
    }

    return CE_None;
}

CPLErr OGRDAMENGStatement::ExecuteInsert(const char *pszSQLStatement, int nMode)
{
    if (!DSQL_SUCCEEDED(dpi_alloc_stmt((poConn->hCon), &hStatement)))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to alloc statement");
        return CE_Failure;
    }

    if (!DSQL_SUCCEEDED(dpi_exec_direct(
            hStatement,
            reinterpret_cast<sdbyte *>(const_cast<char *>(pszSQLStatement)))))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to exectue");
        return CE_Failure;
    }

    sdint8 row_count;
    if (!DSQL_SUCCEEDED(dpi_row_count(hStatement, &row_count)))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to get row_count");
        return CE_Failure;
    }
    return CE_None;
}

CPLErr OGRDAMENGStatement::Execute(const char *pszSQLStatement, int nMode)
{
    if (pszSQLStatement != nullptr)
    {
        CPLErr eErr = Prepare(pszSQLStatement);
        if (eErr != CE_None)
            return eErr;
    }

    if (hStatement == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "prepare null");
        return CE_Failure;
    }

    sdint2 column_count;
    if (!DSQL_SUCCEEDED(dpi_number_columns(hStatement, &column_count)))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to get columns_count");
        return CE_Failure;
    }
    sdint4 nStmtType;
    slength len;

    if (!DSQL_SUCCEEDED(dpi_get_diag_field(
            DSQL_HANDLE_STMT, hStatement, 0, DSQL_DIAG_DYNAMIC_FUNCTION_CODE,
            reinterpret_cast<dpointer>(&nStmtType), 0, &len)))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to get stmt_type");
        return CE_Failure;
    }

    int bSelect = (nStmtType == DSQL_DIAG_FUNC_CODE_SELECT);

    if (!DSQL_SUCCEEDED(dpi_exec(hStatement)))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to exectue");
        return CE_Failure;
    }

    if (!bSelect)
    {
        sdint8 row_count;
        if (!DSQL_SUCCEEDED(dpi_row_count(hStatement, &row_count)))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "failed to get row_count");
            return CE_Failure;
        }
        return CE_None;
    }

    nRawColumnCount = column_count;
    object_index =
        reinterpret_cast<int *>(CPLCalloc(sizeof(int), column_count));
    lob_index = reinterpret_cast<int *>(CPLCalloc(sizeof(int), column_count));
    lob = reinterpret_cast<dhloblctr *>(
        CPLCalloc(sizeof(dhloblctr), column_count));
    obj = reinterpret_cast<dhobj *>(CPLCalloc(sizeof(dhobj), column_count));
    objdesc = reinterpret_cast<dhobjdesc *>(
        CPLCalloc(sizeof(dhobjdesc), column_count));
    blob_len = reinterpret_cast<int *>(CPLCalloc(sizeof(int), column_count));
    col_len = reinterpret_cast<slength **>(
        CPLCalloc(sizeof(slength *), column_count + 1));
    result = reinterpret_cast<char **>(
        CPLCalloc(sizeof(char *), nRawColumnCount + 1));

    dhdesc hdesc_col;
    sdint4 val_len;
    if (!DSQL_SUCCEEDED(dpi_get_stmt_attr(
            hStatement, DSQL_ATTR_IMP_ROW_DESC,
            reinterpret_cast<dpointer>(&hdesc_col), 0, &val_len)))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to get row_desc");
        return CE_Failure;
    }

    for (int iParam = 0; iParam < nRawColumnCount; iParam++)
    {
        DmColDesc coldesc;
        if (!DSQL_SUCCEEDED(dpi_desc_column(
                hStatement, (sdint2)iParam + 1, coldesc.name,
                sizeof(coldesc.name), &coldesc.nameLen, &coldesc.sql_type,
                &coldesc.prec, &coldesc.scale, &coldesc.nullable)))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "failed to get columns_desc");
            return CE_Failure;
        }

        if (coldesc.sql_type == DSQL_CLASS)
        {
            if (!DSQL_SUCCEEDED(dpi_get_desc_field(
                    hdesc_col, (sdint2)iParam + 1, DSQL_DESC_OBJ_DESCRIPTOR,
                    reinterpret_cast<dpointer>(&objdesc[iParam]),
                    sizeof(dhobjdesc), NULL)))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "failed to get object descriptor");
                return CE_Failure;
            }
            if (!DSQL_SUCCEEDED(dpi_alloc_obj((poConn->hCon), &obj[iParam])))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "failed to alloc obj");
                return CE_Failure;
            }
            if (!DSQL_SUCCEEDED(
                    dpi_bind_obj_desc(obj[iParam], objdesc[iParam])))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "failed to bind obj");
                return CE_Failure;
            }
            if (!DSQL_SUCCEEDED(dpi_bind_col(
                    hStatement, (udint2)iParam + 1, DSQL_C_CLASS, &obj[iParam],
                    sizeof(obj[iParam]), &col_len[iParam][0])))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "failed to bind col");
                return CE_Failure;
            }
            object_index[iParam] = 1;
            lob_index[iParam] = 0;
        }
        else if (coldesc.sql_type == DSQL_BLOB || coldesc.sql_type == DSQL_CLOB)
        {
            if (!DSQL_SUCCEEDED(
                    dpi_alloc_lob_locator(hStatement, &lob[iParam])))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "failed to alloc lob");
                return CE_Failure;
            }
            if (!DSQL_SUCCEEDED(dpi_bind_col(hStatement, (udint2)iParam + 1,
                                             DSQL_C_LOB_HANDLE, &lob[iParam], 0,
                                             &col_len[iParam][0])))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "failed to bind col");
                return CE_Failure;
            }
            if (coldesc.sql_type == DSQL_BLOB)
                lob_index[iParam] = 2;
            else
                lob_index[iParam] = 1;
            object_index[iParam] = 0;
        }
        else
        {
            if (!DSQL_SUCCEEDED(dpi_get_desc_field(
                    hdesc_col, (sdint2)iParam + 1, DSQL_DESC_DISPLAY_SIZE,
                    reinterpret_cast<dpointer>(&coldesc.display_size), 0,
                    &val_len)))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "failed to get col display_size");
                return CE_Failure;
            }

            int nbufwidth = 256;
            if (coldesc.prec > 0)
                nbufwidth = static_cast<int>(coldesc.display_size) + 3;
            result[iParam] = (char *)CPLMalloc(nbufwidth + 2);

            if (!DSQL_SUCCEEDED(dpi_bind_col(
                    hStatement, (udint2)iParam + 1, DSQL_C_NCHAR,
                    reinterpret_cast<dpointer>(result[iParam]),
                    coldesc.display_size + 1, &col_len[iParam][0])))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "failed to bind col");
                return CE_Failure;
            }
            object_index[iParam] = 0;
            lob_index[iParam] = 0;
        }
    }
    return CE_None;
}

CPLErr OGRDAMENGStatement::Excute_for_fetchmany(const char *pszSQLStatement)
{
    if (pszSQLStatement != nullptr)
    {
        CPLErr eErr = Prepare(pszSQLStatement);
        if (eErr != CE_None)
            return eErr;
    }

    if (hStatement == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "prepare null");
        return CE_Failure;
    }

    sdint2 column_count;
    if (!DSQL_SUCCEEDED(dpi_number_columns(hStatement, &column_count)))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to get columns_count");
        return CE_Failure;
    }

    if (!DSQL_SUCCEEDED(dpi_set_stmt_attr(hStatement, DSQL_ATTR_ROW_ARRAY_SIZE,
                                          reinterpret_cast<void *>(fetchnum),
                                          0)))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to set stmt attr");
        return CE_Failure;
    }

    sdint4 nStmtType;
    slength len;
    if (!DSQL_SUCCEEDED(dpi_set_stmt_attr(
            hStatement, DSQL_ATTR_CURSOR_TYPE,
            reinterpret_cast<dpointer>(DSQL_CURSOR_DYNAMIC), 0)))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to set stmt attr");
        return CE_Failure;
    }
    if (!DSQL_SUCCEEDED(dpi_get_diag_field(
            DSQL_HANDLE_STMT, hStatement, 0, DSQL_DIAG_DYNAMIC_FUNCTION_CODE,
            reinterpret_cast<dpointer>(&nStmtType), 0, &len)))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to get diag field");
        return CE_Failure;
    }
    int bSelect = (nStmtType == DSQL_DIAG_FUNC_CODE_SELECT);

    if (!DSQL_SUCCEEDED(dpi_exec(hStatement)))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to exectue");
        return CE_Failure;
    }

    if (!bSelect)
    {
        sdint8 row_count;
        if (!DSQL_SUCCEEDED(dpi_row_count(hStatement, &row_count)))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "failed to get row_count");
            return CE_Failure;
        }
        return CE_None;
    }
    is_fectmany = 1;
    nRawColumnCount = column_count;
    object_index =
        reinterpret_cast<int *>(CPLCalloc(sizeof(int), column_count));
    lob_index = reinterpret_cast<int *>(CPLCalloc(sizeof(int), column_count));
    results = reinterpret_cast<char ***>(
        CPLCalloc(sizeof(char **), column_count + 1));
    lobs = reinterpret_cast<dhloblctr **>(
        CPLCalloc(sizeof(dhloblctr *), column_count));
    objs = reinterpret_cast<dhobj **>(CPLCalloc(sizeof(dhobj *), column_count));
    blob_lens =
        reinterpret_cast<int **>(CPLCalloc(sizeof(int *), column_count));
    col_len = reinterpret_cast<slength **>(
        CPLCalloc(sizeof(slength *), column_count + 1));
    objdescs = reinterpret_cast<dhobjdesc **>(
        CPLCalloc(sizeof(dhobjdesc *), column_count));
    for (int i = 0; i < column_count; i++)
    {
        results[i] =
            reinterpret_cast<char **>(CPLCalloc(sizeof(char *), fetchnum));
        col_len[i] =
            reinterpret_cast<slength *>(CPLCalloc(sizeof(slength), fetchnum));
        blob_lens[i] =
            reinterpret_cast<int *>(CPLCalloc(sizeof(int), fetchnum));
        lobs[i] = reinterpret_cast<dhloblctr *>(
            CPLCalloc(sizeof(dhloblctr), fetchnum));
        objs[i] = reinterpret_cast<dhobj *>(CPLCalloc(sizeof(dhobj), fetchnum));
        objdescs[i] = reinterpret_cast<dhobjdesc *>(
            CPLCalloc(sizeof(dhobjdesc), fetchnum));
    }

    dhdesc hdesc_col;
    sdint4 val_len;
    if (!DSQL_SUCCEEDED(dpi_get_stmt_attr(
            hStatement, DSQL_ATTR_IMP_ROW_DESC,
            reinterpret_cast<dpointer>(&hdesc_col), 0, &val_len)))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to get row_desc");
        return CE_Failure;
    }

    for (int iParam = 0; iParam < nRawColumnCount; iParam++)
    {
        DmColDesc coldesc;
        if (!DSQL_SUCCEEDED(dpi_desc_column(
                hStatement, (sdint2)iParam + 1, coldesc.name,
                sizeof(coldesc.name), &coldesc.nameLen, &coldesc.sql_type,
                &coldesc.prec, &coldesc.scale, &coldesc.nullable)))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "failed to get columns_desc");
            return CE_Failure;
        }

        if (coldesc.sql_type == DSQL_CLASS)
        {
            if (!DSQL_SUCCEEDED(dpi_get_desc_field(
                    hdesc_col, (sdint2)iParam + 1, DSQL_DESC_OBJ_DESCRIPTOR,
                    &(objdescs[iParam][0]), sizeof(dhobjdesc), NULL)))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "failed to get object descriptor");
                return CE_Failure;
            }
            for (int i = 0; i < fetchnum; i++)
            {
                if (!DSQL_SUCCEEDED(
                        dpi_alloc_obj((poConn->hCon), &(objs[iParam][i]))))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "failed to alloc obj");
                    return CE_Failure;
                }
                if (!DSQL_SUCCEEDED(dpi_bind_obj_desc(objs[iParam][i],
                                                      objdescs[iParam][0])))
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "failed to bind obj");
                    return CE_Failure;
                }
            }

            if (!DSQL_SUCCEEDED(dpi_bind_col(hStatement, (udint2)iParam + 1,
                                             DSQL_C_CLASS, &objs[iParam][0],
                                             sizeof(objs[iParam][0]),
                                             &col_len[iParam][0])))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "failed to bind col");
                return CE_Failure;
            }
            object_index[iParam] = 1;
            lob_index[iParam] = 0;
        }
        else if (coldesc.sql_type == DSQL_BLOB || coldesc.sql_type == DSQL_CLOB)
        {
            for (int i = 0; i < fetchnum; i++)
            {
                if (!DSQL_SUCCEEDED(
                        dpi_alloc_lob_locator(hStatement, &(lobs[iParam][i]))))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "failed to alloc lob");
                    return CE_Failure;
                }
            }
            if (!DSQL_SUCCEEDED(
                    dpi_bind_col(hStatement, (udint2)iParam + 1,
                                 DSQL_C_LOB_HANDLE, &lobs[iParam][0],
                                 sizeof(lobs[iParam][0]), &col_len[iParam][0])))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "failed to bind col");
                return CE_Failure;
            }
            if (coldesc.sql_type == DSQL_BLOB)
                lob_index[iParam] = 2;
            else
                lob_index[iParam] = 1;
            object_index[iParam] = 0;
        }
        else
        {
            if (!DSQL_SUCCEEDED(dpi_get_desc_field(
                    hdesc_col, (sdint2)iParam + 1, DSQL_DESC_DISPLAY_SIZE,
                    reinterpret_cast<dpointer>(&coldesc.display_size), 0,
                    &val_len)))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "failed to get col display_size");
                return CE_Failure;
            }

            int nbufwidth = 256;

            if (coldesc.prec > 0)
                nbufwidth = static_cast<int>(coldesc.display_size) + 3;
            char *date =
                reinterpret_cast<char *>(CPLMalloc((nbufwidth + 2) * fetchnum));
            memset(date, 0, (nbufwidth + 2) * fetchnum);
            for (int i = 0; i < fetchnum; i++)
            {
                //results[i][iParam] = (char*)CPLMalloc(nbufwidth + 2);
                results[iParam][i] = date + i * (nbufwidth + 2);
            }
            if (!DSQL_SUCCEEDED(
                    dpi_bind_col(hStatement, (udint2)iParam + 1, DSQL_C_NCHAR,
                                 reinterpret_cast<dpointer>(results[iParam][0]),
                                 nbufwidth + 2, &col_len[iParam][0])))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "failed to bind col");
                return CE_Failure;
            }
            object_index[iParam] = 0;
            lob_index[iParam] = 0;
        }
    }
    return CE_None;
}

char **OGRDAMENGStatement::SimpleFetchRow()
{
    int i;
    if (papszCurImage == nullptr)
    {
        papszCurImage = reinterpret_cast<char **>(
            CPLCalloc(sizeof(char *), nRawColumnCount + 1));
    }
    ulength rows;
    if (dpi_fetch(hStatement, &rows) == DSQL_NO_DATA)
        return nullptr;

    for (i = 0; i < nRawColumnCount; i++)
    {
        if (object_index[i] == 0 && lob_index[i] == 0)
            papszCurImage[i] = result[i];
    }

    return papszCurImage;
}

char ***OGRDAMENGStatement::Fetchmany(ulength *rows)
{
    ulength row = 0;

    if (!DSQL_SUCCEEDED(dpi_fetch(hStatement, &row)))
        return nullptr;
    *rows = row;

    if (papszCurImages == nullptr)
    {
        papszCurImages = reinterpret_cast<char ***>(
            CPLCalloc(sizeof(char **), nRawColumnCount + 1));
        for (int i = 0; i < nRawColumnCount; i++)
        {
            papszCurImages[i] =
                reinterpret_cast<char **>(CPLCalloc(sizeof(char *), fetchnum));
        }
    }

    for (ulength i = 0; i < nRawColumnCount; i++)
    {
        if (object_index[i] == 0 && lob_index[i] == 0)
        {
            for (int num = 0; num < *rows; num++)
            {
                papszCurImages[i][num] = results[i][num];
            }
        }
        else if (object_index[i] == 1)
        {
            slength real_len, val_len;
            for (int num = 0; num < *rows; num++)
            {
                if (!DSQL_SUCCEEDED(dpi_get_obj_val((dhobj)objs[i][num], 1,
                                                    DSQL_C_BINARY, NULL, 0,
                                                    &real_len)))
                {
                    CPLError(CE_Debug, CPLE_AppDefined,
                             "failed to get object len or object is empty");
                    if (results[i][num])
                    {
                        CPLFree(results[i][num]);
                    }
                    results[i][num] = nullptr;
                    papszCurImages[i][num] = nullptr;
                    continue;
                }
                if (!results[i][num])
                {
                    results[i][num] = reinterpret_cast<char *>(CPLMalloc(1000));
                }
                if (real_len > 1000)
                {
                    CPLFree(results[i][num]);
                    results[i][num] =
                        reinterpret_cast<char *>(CPLMalloc(real_len));
                }
                else
                {
                    real_len = 1000;
                }
                if (!DSQL_SUCCEEDED(dpi_get_obj_val(
                        (dhobj)objs[i][num], 1, DSQL_C_BINARY, results[i][num],
                        (udint4)real_len, &val_len)))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "failed to get object value");
                    return nullptr;
                }
                papszCurImages[i][num] = results[i][num];
            }
        }
        else
        {
            slength real_len, val_len;
            for (int num = 0; num < *rows; num++)
            {
                if (!DSQL_SUCCEEDED(dpi_lob_get_length((dhloblctr)lobs[i][num],
                                                       &real_len)) ||
                    real_len == -1)
                {
                    CPLError(CE_Debug, CPLE_AppDefined,
                             "failed to get lob len or lob is empty");
                    if (results[i][num])
                    {
                        CPLFree(results[i][num]);
                    }
                    results[i][num] = nullptr;
                    papszCurImages[i][num] = nullptr;
                    continue;
                }
                if (results[i][num])
                {
                    CPLFree(results[i][num]);
                }
                char *objvalue =
                    reinterpret_cast<char *>(CPLMalloc(real_len + 3));
                if (lob_index[i] == 2)
                {
                    if (!DSQL_SUCCEEDED(dpi_lob_read((dhloblctr)lobs[i][num], 1,
                                                     DSQL_C_BINARY, 0, objvalue,
                                                     real_len + 1, &val_len)))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "failed to get object value");
                        CPLFree(objvalue);
                        return nullptr;
                    }
                    blob_lens[i][num] = static_cast<int>(val_len);
                }
                else
                {
                    if (!DSQL_SUCCEEDED(dpi_lob_read((dhloblctr)lobs[i][num], 1,
                                                     DSQL_C_NCHAR, 0, objvalue,
                                                     real_len + 1, &val_len)))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "failed to get object value");
                        CPLFree(objvalue);
                        return nullptr;
                    }
                }
                results[i][num] = objvalue;
                papszCurImages[i][num] = results[i][num];
            }
        }
    }

    return papszCurImages;
}

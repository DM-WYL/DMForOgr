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
#include <cmath>
#include <climits>
#include <cfloat>

#define DOUBLE_SIZE 8
#define FLOAT_SIZE 4
#define INT_SIZE 4
#define BYTE_SIZE 1

#define MAX_DEPTH 200

#define DM_POINT 1
#define DM_LINE 2
#define DM_POLYGON 3
#define DM_MULTIPOINT 4
#define DM_MULTILINE 5
#define DM_MULTIPOLYGON 6
#define DM_COLLECTION 7
#define DM_CIRCSTRING 8
#define DM_COMPOUND 9
#define DM_CURVEPOLY 10
#define DM_MULTICURVE 11
#define DM_MULTISURFACE 12
#define DM_POLYHEDRALSURFACE 13
#define DM_TRIANGLE 14
#define DM_TIN 15

#define NUMDMTYPES 16

#define WKB_POINT 1
#define WKB_LINESTRING 2
#define WKB_POLYGON 3
#define WKB_MULTIPOINT 4
#define WKB_MULTILINESTRING 5
#define WKB_MULTIPOLYGON 6
#define WKB_GEOMETRYCOLLECTION 7
#define WKB_CIRCULARSTRING 8
#define WKB_COMPOUNDCURVE 9
#define WKB_CURVEPOLYGON 10
#define WKB_MULTICURVE 11
#define WKB_MULTISURFACE 12
#define WKB_CURVE 13   /* from ISO draft, not sure is real */
#define WKB_SURFACE 14 /* from ISO draft, not sure is real */
#define WKB_POLYHEDRALSURFACE 15
#define WKB_TIN 16
#define WKB_TRIANGLE 17

#define WKB_Z 0x80000000
#define WKB_M 0x40000000
#define WKB_SRID 0x20000000

#define SRID_UNKNOWN 0
#define SRID_MAXIMUM 999999
#define SRID_USER_MAXIMUM 998999

#define DM_X_SOLID 0x00000001
#define DM_Z 0x01
#define DM_M 0x02
#define DM_BBOX 0x04
#define DM_GEODETIC 0x08
#define DM_EXTENDED 0x10
#define DM_VER 0x40

#define DM_GET_EXTENDED(gflags) ((gflags)&DM_EXTENDED) >> 4
#define DM_GET_BBOX(gflags) (((gflags)&DM_BBOX) >> 2)
#define DM_GET_GEODETIC(gflags) (((gflags)&DM_GEODETIC) >> 3)
#define DM_GET_Z(gflags) ((gflags)&DM_Z)
#define DM_GET_M(gflags) (((gflags)&DM_M) >> 1)

#define DM_SET_Z(gflags, value)                                                \
    ((gflags) = (value) ? ((gflags) | DM_Z) : ((gflags) & ~DM_Z))
#define DM_SET_M(gflags, value)                                                \
    ((gflags) = (value) ? ((gflags) | DM_M) : ((gflags) & ~DM_M))
#define DM_SET_BBOX(gflags, value)                                             \
    ((gflags) = (value) ? ((gflags) | DM_BBOX) : ((gflags) & ~DM_BBOX))
#define DM_SET_VERSION(gflags, value)                                          \
    ((gflags) = (value) ? ((gflags) | DM_VER) : ((gflags) & ~DM_VER))
#define DM_SIZE_SET(varsize, len) ((varsize) = (((uint32_t)(len)) << 2))

static float DoubleToFloatClamp(double val)
{
    if (val >= FLT_MAX)
        return FLT_MAX;
    if (val <= -FLT_MAX)
        return -FLT_MAX;
    return (float)val;
}

typedef struct
{
    GByte *wkb;       /* Points to start of WKB */
    size_t wkb_size;  /* Expected size of WKB */
    GByte *pos;       /* Current parse position */
    GInt8 swap_bytes; /* Do an endian flip? */
    GInt8 has_z;
    GInt8 has_m;
    GInt8 ndims;
    GInt8 need_box;
    GInt8 type;
    int srid;
} wkb_info;

typedef struct
{
    GUInt32 npoints; /* how many points we are currently storing */
    /* Array of POINT 2D, 3D or 4D, possibly misaligned. */
    GByte *serialized_pointlist;
} POINTARRAY;

/******************************************************************
* POINT2D, POINT3D
*/
typedef struct
{
    double x, y;
} POINT2D;

typedef struct
{
    double x, y, z;
} POINT3D;

#ifdef WORDS_BIGENDIAN
#define IS_BIG_ENDIAN 1
#else
#define IS_BIG_ENDIAN 0
#endif

size_t OGRDAMENG_Gser_From_WKB(wkb_info *wkb_info, GByte *buf, GByte *depth);

size_t OGRDAMENG_Gser_Get_Expected_Size(wkb_info *wkb_info, GByte *depth);

static GByte *OGRDAMENG_Wkb_From_Gserialized(GByte *data_ptr, GByte has_z,
                                             GByte has_m, size_t *g_size,
                                             int *wkb_size, int srid);

/************************************************************************/
/*                          next_float_down()                           */
/************************************************************************/

float next_float_down(double d)
{
    float result;
    result = DoubleToFloatClamp(d);

    if ((static_cast<double>(result)) <= d)
        return result;

    return nextafterf(result, -1 * FLT_MAX);
}

/************************************************************************/
/*                           next_float_up()                            */
/************************************************************************/

float next_float_up(double d)
{
    float result;
    result = DoubleToFloatClamp(d);

    if ((static_cast<double>(result)) <= d)
        return result;

    return nextafterf(result, FLT_MAX);
}

/************************************************************************/
/*                            swap_values()                             */
/************************************************************************/

void swap_values(GByte *value, int size)
{
    int i = 0;
    GByte tmp;

    for (i = 0; i < size / 2; i++)
    {
        tmp = value[i];
        value[i] = value[size - i - 1];
        value[size - i - 1] = tmp;
    }
    return;
}

/************************************************************************/
/*                       OGRDAMENG_Ptarray_Free()                       */
/************************************************************************/

void OGRDAMENG_Ptarray_Free(POINTARRAY *pa)
{
    if (pa)
    {
        if (pa->serialized_pointlist)
            CPLFree(pa->serialized_pointlist);
        CPLFree(pa);
    }
}

/************************************************************************/
/*                 OGRDAMENG_Ptarray_Construct_Empty()                  */
/************************************************************************/

POINTARRAY *OGRDAMENG_Ptarray_Construct_Empty(GUInt32 ndims, GUInt32 npoints)
{
    POINTARRAY *pa =
        reinterpret_cast<POINTARRAY *>(CPLMalloc(sizeof(POINTARRAY)));
    if (!pa)
        return NULL;

    pa->serialized_pointlist = NULL;

    /* We will be allocating a bit of room */
    pa->npoints = npoints;

    /* Allocate the coordinate array */
    if (npoints > 0)
    {
        /*
        * Size of point represeneted in the POINTARRAY
        * 16 for 2d, 24 for 3d, 32 for 4d
        */
        pa->serialized_pointlist = reinterpret_cast<GByte *>(
            CPLMalloc(npoints * sizeof(double) * ndims));
        if (!pa->serialized_pointlist)
        {
            CPLFree(pa);
            return NULL;
        }
    }
    else
        pa->serialized_pointlist = NULL;

    return pa;
}

/************************************************************************/
/*                    OGRDAMENG_Ptarray_Construct()                     */
/************************************************************************/

POINTARRAY *OGRDAMENG_Ptarray_Construct(GUInt32 ndims, GUInt32 npoints)
{
    POINTARRAY *pa = OGRDAMENG_Ptarray_Construct_Empty(ndims, npoints);

    pa->npoints = npoints;

    return pa;
}

/************************************************************************/
/*               OGRDAMENG_Ptarray_Construct_Copy_Data()                */
/************************************************************************/

POINTARRAY *OGRDAMENG_Ptarray_Construct_Copy_Data(GUInt32 ndims,
                                                  GUInt32 npoints,
                                                  GByte *ptlist)
{
    POINTARRAY *pa =
        reinterpret_cast<POINTARRAY *>(CPLMalloc(sizeof(POINTARRAY)));

    pa->npoints = npoints;

    if (npoints > 0)
    {
        pa->serialized_pointlist = reinterpret_cast<GByte *>(
            CPLMalloc(sizeof(double) * ndims * npoints));
        memcpy(pa->serialized_pointlist, ptlist,
               sizeof(double) * ndims * npoints);
    }
    else
    {
        pa->serialized_pointlist = NULL;
    }

    return pa;
}

/************************************************************************/
/*                    OGRDAMENG_GetPoint_Internal()                     */
/************************************************************************/

static inline GByte *OGRDAMENG_GetPoint_Internal(GUInt32 ndims, POINTARRAY *pa,
                                                 GUInt32 n)
{
    size_t size;
    GByte *ptr;

    size = sizeof(double) * ndims;
    ptr = pa->serialized_pointlist + size * n;

    return ptr;
}

/************************************************************************/
/*                   OGRDAMENG_Ptarray_Is_Closed_2D()                   */
/************************************************************************/

int OGRDAMENG_Ptarray_Is_Closed_2D(POINTARRAY *in)
{
    if (!in)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "OGRDAMENG_Ptarray_Is_Closed_2D: called with null point array");
        return 0;
    }
    if (in->npoints <= 1)
        return in->npoints; /* single-point are closed, empty not closed */

    return 0 == memcmp(OGRDAMENG_GetPoint_Internal(2, in, 0),
                       OGRDAMENG_GetPoint_Internal(2, in, in->npoints - 1),
                       sizeof(POINT2D));
}

/************************************************************************/
/*                   OGRDAMENG_Ptarray_Is_Closed_3D()                   */
/************************************************************************/

int OGRDAMENG_Ptarray_Is_Closed_3D(POINTARRAY *in)
{
    if (!in)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "OGRDAMENG_Ptarray_Is_Closed_3D: called with null point array");
        return 0;
    }
    if (in->npoints <= 1)
        return in->npoints; /* single-point are closed, empty not closed */

    return 0 == memcmp(OGRDAMENG_GetPoint_Internal(3, in, 0),
                       OGRDAMENG_GetPoint_Internal(3, in, in->npoints - 1),
                       sizeof(POINT3D));
}

/************************************************************************/
/*                    OGRDAMENG_Ptarray_Is_Closed()                     */
/************************************************************************/

int OGRDAMENG_Ptarray_Is_Closed(GInt8 has_z, POINTARRAY *in)
{
    if (has_z)
        return OGRDAMENG_Ptarray_Is_Closed_3D(in);
    else
        return OGRDAMENG_Ptarray_Is_Closed_2D(in);
}

/************************************************************************/
/*                     OGRDAMENG_WKB_Parse_Check()                      */
/************************************************************************/

static GInt8 OGRDAMENG_WKB_Parse_Check(wkb_info *wkb_info, size_t next)
{
    if ((wkb_info->pos + next) > (wkb_info->wkb + wkb_info->wkb_size))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "WKB structure does not match expected size!");
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                     OGRDAMENG_Integer_From_WKB()                     */
/************************************************************************/

static GUInt32 OGRDAMENG_Integer_From_WKB(wkb_info *wkb_info)
{
    GUInt32 int_value = 0;

    if (OGRDAMENG_WKB_Parse_Check(wkb_info, INT_SIZE))
        return 0;

    memcpy(&int_value, wkb_info->pos, INT_SIZE);

    /* Swap? Copy into a stack-allocated integer. */
    if (wkb_info->swap_bytes)
        CPL_SWAP32(int_value);

    wkb_info->pos += INT_SIZE;
    return int_value;
}

/************************************************************************/
/*                     OGRDAMENG_Double_From_WKB()                      */
/************************************************************************/

static double OGRDAMENG_Double_From_WKB(wkb_info *wkb_info)
{
    double double_value = 0;

    memcpy(&double_value, wkb_info->pos, DOUBLE_SIZE);

    /* Swap? Copy into a stack-allocated integer. */
    if (wkb_info->swap_bytes)
        CPL_SWAPDOUBLE(&double_value);

    wkb_info->pos += DOUBLE_SIZE;
    return double_value;
}

/************************************************************************/
/*                    OGRDAMENG_Gser_Head_From_WKB()                    */
/************************************************************************/

GByte *OGRDAMENG_Gser_Head_From_WKB(GByte *loc, GUInt32 type, GUInt32 ngeom)
{
    /* Write in the type. */
    memcpy(loc, &type, sizeof(GUInt32));
    loc += sizeof(GUInt32);
    /* Write in the number of points (0 => empty). */
    memcpy(loc, &ngeom, sizeof(GUInt32));
    loc += sizeof(GUInt32);

    return loc;
}

/************************************************************************/
/*                     OGRDAMENG_Ptarray_From_WKB()                     */
/************************************************************************/

static POINTARRAY *OGRDAMENG_Ptarray_From_WKB(wkb_info *wkb_info)
{
    POINTARRAY *pa = NULL;
    size_t pa_size;
    GUInt32 npoints = 0;

    /* Calculate the size of this point array. */
    npoints = OGRDAMENG_Integer_From_WKB(wkb_info);

    if (npoints > UINT_MAX / DOUBLE_SIZE / 4)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Pointarray length (%d) is too large", npoints);
        return NULL;
    }

    pa_size = npoints * wkb_info->ndims * DOUBLE_SIZE;

    /* Empty! */
    if (npoints == 0)
        return OGRDAMENG_Ptarray_Construct(wkb_info->ndims, npoints);

    /* Does the data we want to read exist? */
    if (OGRDAMENG_WKB_Parse_Check(wkb_info, pa_size))
        return NULL;

    /* If we're in a native endianness, we can just copy the data directly! */
    if (!wkb_info->swap_bytes)
    {
        pa = OGRDAMENG_Ptarray_Construct_Copy_Data(
            wkb_info->ndims, npoints, reinterpret_cast<GByte *>(wkb_info->pos));
        wkb_info->pos += pa_size;
    }
    /* Otherwise we have to read each double, separately. */
    else
    {
        GUInt32 i = 0;
        double *dlist;
        pa = OGRDAMENG_Ptarray_Construct(wkb_info->ndims, npoints);
        dlist = reinterpret_cast<double *>(pa->serialized_pointlist);
        for (i = 0; i < npoints * wkb_info->ndims; i++)
        {
            dlist[i] = OGRDAMENG_Double_From_WKB(wkb_info);
        }
    }

    return pa;
}

/************************************************************************/
/*                 OGRDAMENG_GserPoint_Size_From_WKB()                  */
/************************************************************************/

size_t OGRDAMENG_GserPoint_Size_From_WKB(wkb_info *wkb_info)
{
    size_t expected_size = INT_SIZE; /* Type number. */
    double x;

    /* Number of points. */
    expected_size += INT_SIZE;
    /* Does the data we want to read exist? */
    if (OGRDAMENG_WKB_Parse_Check(wkb_info, wkb_info->ndims * DOUBLE_SIZE))
        return expected_size;

    x = OGRDAMENG_Double_From_WKB(wkb_info);
    wkb_info->pos += (wkb_info->ndims - 1) * DOUBLE_SIZE;

    if (isnan(x))
        return expected_size;

    expected_size += DOUBLE_SIZE * wkb_info->ndims;

    return expected_size;
}

/************************************************************************/
/*                    OGRDAMENG_GserPoint_From_WKB()                    */
/************************************************************************/

static size_t OGRDAMENG_GserPoint_From_WKB(wkb_info *wkb_info, GByte *buf)
{
    POINTARRAY *pa = NULL;
    size_t pa_size;
    POINT2D *pt;
    GByte *loc;
    size_t ptsize;

    pa_size = wkb_info->ndims * DOUBLE_SIZE;

    /* Does the data we want to read exist? */
    if (OGRDAMENG_WKB_Parse_Check(wkb_info, pa_size))
        return NULL;

    /* If we're in a native endianness, we can just copy the data directly! */
    if (!wkb_info->swap_bytes)
    {
        pa = OGRDAMENG_Ptarray_Construct_Copy_Data(
            wkb_info->ndims, 1, reinterpret_cast<GByte *>(wkb_info->pos));
        wkb_info->pos += pa_size;
    }
    /* Otherwise we have to read each double, separately */
    else
    {
        GInt32 i = 0;
        double *dlist;
        pa = OGRDAMENG_Ptarray_Construct(wkb_info->ndims, 1);
        dlist = reinterpret_cast<double *>(pa->serialized_pointlist);
        for (i = 0; i < wkb_info->ndims; i++)
        {
            dlist[i] = OGRDAMENG_Double_From_WKB(wkb_info);
        }
    }

    /* Check for POINT(NaN NaN) ==> POINT EMPTY */
    pt = reinterpret_cast<POINT2D *>(OGRDAMENG_GetPoint_Internal(2, pa, 0));
    if (isnan(pt->x) && isnan(pt->y))
    {
        OGRDAMENG_Ptarray_Free(pa);
        pa = OGRDAMENG_Ptarray_Construct(wkb_info->ndims, 0);
    }

    ptsize = sizeof(double) * wkb_info->ndims;

    loc = buf;

    loc = OGRDAMENG_Gser_Head_From_WKB(loc, DM_POINT, pa->npoints);

    /* Copy in the ordinates. */
    if (pa->npoints > 0)
    {
        memcpy(loc, OGRDAMENG_GetPoint_Internal(wkb_info->ndims, pa, 0),
               ptsize);
        loc += ptsize;
    }
    OGRDAMENG_Ptarray_Free(pa);
    return static_cast<size_t>(loc - buf);
}

/************************************************************************/
/*                  OGRDAMENG_GserLine_Size_From_WKB()                  */
/************************************************************************/

size_t OGRDAMENG_GserLine_Size_From_WKB(wkb_info *wkb_info)
{
    size_t expected_size = INT_SIZE; /* Type number. */
    GUInt32 npoints = OGRDAMENG_Integer_From_WKB(wkb_info);

    /* Number of points. */
    expected_size += INT_SIZE;
    /* Does the data we want to read exist? */
    if (OGRDAMENG_WKB_Parse_Check(wkb_info,
                                  npoints * wkb_info->ndims * DOUBLE_SIZE))
        return expected_size;

    wkb_info->pos += npoints * wkb_info->ndims * DOUBLE_SIZE;
    expected_size += npoints * wkb_info->ndims * DOUBLE_SIZE;

    return expected_size;
}

/************************************************************************/
/*                    OGRDAMENG_GserLine_From_WKB()                     */
/************************************************************************/

static size_t OGRDAMENG_GserLine_From_WKB(wkb_info *wkb_info, GByte *buf)
{
    GByte *loc;
    size_t size;
    POINTARRAY *pa = OGRDAMENG_Ptarray_From_WKB(wkb_info);

    if (pa == NULL)
        return NULL;

    if (pa->npoints == 0)
    {
        loc = buf;
        loc = OGRDAMENG_Gser_Head_From_WKB(loc, DM_LINE, 0);
        OGRDAMENG_Ptarray_Free(pa);
        return static_cast<size_t>(loc - buf);
    }

    if (pa->npoints < 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "must have at least two points");
        return NULL;
    }

    loc = buf;

    loc = OGRDAMENG_Gser_Head_From_WKB(loc, DM_LINE, pa->npoints);

    /* Copy in the ordinates. */
    size = pa->npoints * wkb_info->ndims * sizeof(double);
    memcpy(loc, OGRDAMENG_GetPoint_Internal(wkb_info->ndims, pa, 0), size);
    loc += size;

    OGRDAMENG_Ptarray_Free(pa);
    return static_cast<size_t>(loc - buf);
}

/************************************************************************/
/*                 OGRDAMENG_GserCircstring_From_WKB()                  */
/************************************************************************/

static size_t OGRDAMENG_GserCircstring_From_WKB(wkb_info *wkb_info, GByte *buf)
{
    GByte *loc;
    size_t size;
    POINTARRAY *pa = OGRDAMENG_Ptarray_From_WKB(wkb_info);

    if (pa == NULL)
        return NULL;

    if (pa->npoints == 0)
    {
        loc = buf;
        loc = OGRDAMENG_Gser_Head_From_WKB(loc, DM_CIRCSTRING, 0);
        OGRDAMENG_Ptarray_Free(pa);
        return static_cast<size_t>(loc - buf);
    }

    if (pa->npoints < 3)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "must have at least three points");
        return NULL;
    }

    if (!(pa->npoints % 2))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "must have an odd number of points");
        return NULL;
    }

    loc = buf;

    loc = OGRDAMENG_Gser_Head_From_WKB(loc, DM_CIRCSTRING, pa->npoints);

    /* Copy in the ordinates. */
    size = pa->npoints * wkb_info->ndims * sizeof(double);
    memcpy(loc, OGRDAMENG_GetPoint_Internal(wkb_info->ndims, pa, 0), size);
    loc += size;

    OGRDAMENG_Ptarray_Free(pa);
    return static_cast<size_t>(loc - buf);
}

/************************************************************************/
/*                  OGRDAMENG_GserPoly_Size_From_WKB()                  */
/************************************************************************/

size_t OGRDAMENG_GserPoly_Size_From_WKB(wkb_info *wkb_info)
{
    size_t expected_size = INT_SIZE; /* Type number. */
    GUInt32 i;
    GUInt32 nrings = OGRDAMENG_Integer_From_WKB(wkb_info);
    GUInt32 npoints;

    /* Number of rings */
    expected_size += INT_SIZE;
    if (nrings != 0)
    {
        if (nrings % 2)
            expected_size += INT_SIZE; /* Padding to double alignment. */
        for (i = 0; i < nrings; i++)
        {
            /* Number of points */
            expected_size += INT_SIZE;
            if (OGRDAMENG_WKB_Parse_Check(wkb_info, INT_SIZE))
                return expected_size;
            npoints = OGRDAMENG_Integer_From_WKB(wkb_info);

            if (OGRDAMENG_WKB_Parse_Check(wkb_info,
                                          wkb_info->ndims * DOUBLE_SIZE))
                return expected_size;
            expected_size += npoints * wkb_info->ndims * DOUBLE_SIZE;
            wkb_info->pos += npoints * wkb_info->ndims * DOUBLE_SIZE;
        }
    }
    return expected_size;
}

/************************************************************************/
/*                    OGRDAMENG_GserPoly_From_WKB()                     */
/************************************************************************/

static size_t OGRDAMENG_GserPoly_From_WKB(wkb_info *wkb_info, GByte *buf)
{
    GUInt32 i, j;
    GByte *loc;
    GUInt32 nrings = OGRDAMENG_Integer_From_WKB(wkb_info);
    POINTARRAY **pas = NULL;

    loc = buf;

    loc = OGRDAMENG_Gser_Head_From_WKB(loc, DM_POLYGON, nrings);

    pas = reinterpret_cast<POINTARRAY **>(
        CPLMalloc(sizeof(POINTARRAY *) * nrings));

    /* Empty polygon? */
    if (nrings != 0)
    {
        for (i = 0; i < nrings; i++)
        {
            pas[i] = OGRDAMENG_Ptarray_From_WKB(wkb_info);
            if (pas[i] == NULL)
            {
                for (j = 0; j < i; j++)
                    OGRDAMENG_Ptarray_Free(pas[j]);
                CPLFree(pas);
                return NULL;
            }

            /* Check for at least four points. */
            if (pas[i]->npoints < 4)
            {
                for (j = 0; j < i; j++)
                    OGRDAMENG_Ptarray_Free(pas[j]);
                CPLFree(pas);
                CPLError(CE_Failure, CPLE_AppDefined,
                         "must have at least four points in each ring");
                return NULL;
            }

            /* Check that first and last points are the same. */
            if (!OGRDAMENG_Ptarray_Is_Closed(wkb_info->has_z, pas[i]))
            {
                for (j = 0; j < i; j++)
                    OGRDAMENG_Ptarray_Free(pas[j]);
                CPLFree(pas);
                CPLError(CE_Failure, CPLE_AppDefined, "must have closed rings");
                return NULL;
            }

            /* Write in the npoints per ring. */
            memcpy(loc, &(pas[i]->npoints), sizeof(GUInt32));
            loc += sizeof(GUInt32);
        }
    }

    /* Add in padding if necessary to remain double aligned. */
    if (nrings % 2)
    {
        memset(loc, 0, sizeof(GUInt32));
        loc += sizeof(GUInt32);
    }

    /* Copy in the ordinates. */
    for (i = 0; i < nrings; i++)
    {
        size_t pasize;

        pasize = pas[i]->npoints * wkb_info->ndims * sizeof(double);
        if (pas[i]->npoints > 0)
            memcpy(loc, OGRDAMENG_GetPoint_Internal(wkb_info->ndims, pas[i], 0),
                   pasize);
        loc += pasize;
    }
    for (i = 0; i < nrings; i++)
        OGRDAMENG_Ptarray_Free(pas[i]);
    CPLFree(pas);
    return static_cast<size_t>(loc - buf);
}

/************************************************************************/
/*                  OGRDAMENG_GserTriangle_From_WKB()                   */
/************************************************************************/

static size_t OGRDAMENG_GserTriangle_From_WKB(wkb_info *wkb_info, GByte *buf)
{
    GByte *loc;
    size_t size;
    GUInt32 nrings = OGRDAMENG_Integer_From_WKB(wkb_info);
    POINTARRAY *pa = nullptr;

    if (nrings == 0) /* Empty triangle? */
        pa = OGRDAMENG_Ptarray_Construct_Empty(wkb_info->ndims, 1);
    else if (nrings != 1) /* Should be only one ring. */
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Triangle has wrong number of rings: %d", nrings);
        return 0;
    }
    else
        pa = OGRDAMENG_Ptarray_From_WKB(wkb_info);

    /* If there's no points, return an empty triangle. */
    if (pa == nullptr)
        pa = OGRDAMENG_Ptarray_Construct_Empty(wkb_info->ndims, 1);

    /* Check for at least four points. */
    if (pa->npoints < 4)
    {
        OGRDAMENG_Ptarray_Free(pa);
        CPLError(CE_Failure, CPLE_AppDefined, "must have at least four points");
        return 0;
    }

    if (!OGRDAMENG_Ptarray_Is_Closed(wkb_info->has_z, pa))
    {
        OGRDAMENG_Ptarray_Free(pa);
        CPLError(CE_Failure, CPLE_AppDefined, "must have closed rings");
        return 0;
    }

    loc = buf;

    loc = OGRDAMENG_Gser_Head_From_WKB(loc, DM_TRIANGLE, pa->npoints);

    /* Copy in the ordinates. */
    if (pa->npoints > 0)
    {
        size = pa->npoints * wkb_info->ndims * sizeof(double);
        memcpy(loc, OGRDAMENG_GetPoint_Internal(wkb_info->ndims, pa, 0), size);
        loc += size;
    }
    OGRDAMENG_Ptarray_Free(pa);
    return static_cast<size_t>(loc - buf);
}

/************************************************************************/
/*                  OGRDAMENG_GserCurvepoly_From_WKB()                  */
/************************************************************************/

static size_t OGRDAMENG_GserCurvepoly_From_WKB(wkb_info *wkb_info, GByte *buf,
                                               GByte *depth)
{
    GByte *loc;
    GUInt32 i;
    GUInt32 ngeoms = OGRDAMENG_Integer_From_WKB(wkb_info);

    loc = buf;

    loc = OGRDAMENG_Gser_Head_From_WKB(loc, wkb_info->type, ngeoms);

    (*depth)++;
    /* Serialize subgeoms. */
    for (i = 0; i < ngeoms; i++)
        loc += OGRDAMENG_Gser_From_WKB(wkb_info, loc, depth);

    (*depth)--;
    return static_cast<size_t>(loc - buf);
}

/************************************************************************/
/*              OGRDAMENG_GserCollection__Size_From_WKB()               */
/************************************************************************/

size_t OGRDAMENG_GserCollection__Size_From_WKB(wkb_info *wkb_info, GByte *depth)
{
    size_t expected_size = INT_SIZE; /* Type number. */
    GUInt32 i;
    GUInt32 ngeoms = OGRDAMENG_Integer_From_WKB(wkb_info);

    (*depth)++;
    /* Number of geoms */
    expected_size += INT_SIZE;
    if (ngeoms != 0)
    {
        for (i = 0; i < ngeoms; i++)
            expected_size += OGRDAMENG_Gser_Get_Expected_Size(wkb_info, depth);
    }
    (*depth)--;
    return expected_size;
}

/************************************************************************/
/*                 OGRDAMENG_GserCollection_From_WKB()                  */
/************************************************************************/

static size_t OGRDAMENG_GserCollection_From_WKB(wkb_info *wkb_info, GByte *buf,
                                                GByte *depth)
{
    size_t subsize;
    GByte *loc;
    GUInt32 i;
    GUInt32 ngeoms = OGRDAMENG_Integer_From_WKB(wkb_info);

    (*depth)++;
    if (*depth >= MAX_DEPTH)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Geometry has too many chained collections");
        return NULL;
    }
    loc = buf;

    loc = OGRDAMENG_Gser_Head_From_WKB(loc, wkb_info->type, ngeoms);

    /* Serialize subgeoms. */
    for (i = 0; i < ngeoms; i++)
    {
        subsize = OGRDAMENG_Gser_From_WKB(wkb_info, loc, depth);
        loc += subsize;
    }
    (*depth)--;
    return static_cast<size_t>(loc - buf);
}

/************************************************************************/
/*                         OGRDAMENG_Fix_SRID()                         */
/************************************************************************/

int OGRDAMENG_Fix_SRID(int srid)
{
    int newsrid = srid;

    if (newsrid < 0)
    {
        newsrid = SRID_UNKNOWN;
    }
    else if (srid > SRID_MAXIMUM)
    {
        newsrid = SRID_USER_MAXIMUM + 1 +
                  (srid % (SRID_MAXIMUM - SRID_USER_MAXIMUM - 1));
        CPLError(CE_Warning, CPLE_AppDefined,
                 "SRID value %d > SRID_MAXIMUM converted to %d", srid, newsrid);
    }

    return newsrid;
}

/************************************************************************/
/*                   OGRDAMENG_Type_From_WKB_State()                    */
/************************************************************************/

CPLErr OGRDAMENG_Type_From_WKB_State(wkb_info *wkb_info)
{
    GUInt32 wkb_simple_type;
    GUInt32 wkb_type;
    GInt8 has_srid = FALSE;
    int srid;

    /* Fail when handed incorrect starting byte */
    if (OGRDAMENG_WKB_Parse_Check(wkb_info, INT_SIZE))
        return CE_Failure;

    memcpy(&wkb_type, wkb_info->pos, INT_SIZE);

    /* Swap? Copy into a stack-allocated integer. */
    if (wkb_info->swap_bytes)
        wkb_type = CPL_SWAP32(wkb_type);

    /* If any of the higher bits are set, this is probably an extended type. */
    if (wkb_type & 0xF0000000)
    {
        if (wkb_type & WKB_Z)
            wkb_info->has_z = TRUE;
        if (wkb_type & WKB_M)
            wkb_info->has_m = TRUE;
        if (wkb_type & WKB_SRID)
            has_srid = TRUE;
    }

    /* Mask off the flags */
    wkb_type = wkb_type & 0x0FFFFFFF;

    /* Catch strange Oracle WKB type numbers */
    if (wkb_type >= 4000)
    {
        return CE_Failure;
    }

    /* Strip out just the type number (1-12) from the ISO number (eg 3001-3012) */
    wkb_simple_type = wkb_type % 1000;

    /* Extract the Z/M information from ISO style numbers */
    if (wkb_type >= 3000)
    {
        wkb_info->has_z = TRUE;
        wkb_info->has_m = TRUE;
    }
    else if (wkb_type >= 2000)
    {
        wkb_info->has_m = TRUE;
    }
    else if (wkb_type >= 1000)
    {
        wkb_info->has_z = TRUE;
    }

    if (wkb_info->has_z)
        wkb_info->ndims++;
    if (wkb_info->has_m)
        wkb_info->ndims++;

    wkb_info->pos += INT_SIZE;
    /* Read the SRID, if necessary */
    if (has_srid)
    {
        if (OGRDAMENG_WKB_Parse_Check(wkb_info, INT_SIZE))
            return CE_Failure;

        memcpy(&srid, wkb_info->pos, INT_SIZE);

        /* Swap? Copy into a stack-allocated integer. */
        if (wkb_info->swap_bytes)
            srid = CPL_SWAP32(srid);

        wkb_info->pos += INT_SIZE;
        wkb_info->srid = OGRDAMENG_Fix_SRID(srid);
        if (srid == SRID_UNKNOWN)
            wkb_info->srid = 0;
    }

    switch (wkb_simple_type)
    {
        case WKB_POINT:
            wkb_info->type = DM_POINT;
            return CE_None;
            break;
        case WKB_LINESTRING:
            wkb_info->type = DM_LINE;
            return CE_None;
            break;
        case WKB_POLYGON:
            wkb_info->type = DM_POLYGON;
            return CE_None;
            break;
        case WKB_MULTIPOINT:
            wkb_info->type = DM_MULTIPOINT;
            return CE_None;
            break;
        case WKB_MULTILINESTRING:
            wkb_info->type = DM_MULTILINE;
            return CE_None;
            break;
        case WKB_MULTIPOLYGON:
            wkb_info->type = DM_MULTIPOLYGON;
            return CE_None;
            break;
        case WKB_GEOMETRYCOLLECTION:
            wkb_info->type = DM_COLLECTION;
            return CE_None;
            break;
        case WKB_CIRCULARSTRING:
            wkb_info->type = DM_CIRCSTRING;
            return CE_None;
            break;
        case WKB_COMPOUNDCURVE:
            wkb_info->type = DM_COMPOUND;
            return CE_None;
            break;
        case WKB_CURVEPOLYGON:
            wkb_info->type = DM_CURVEPOLY;
            return CE_None;
            break;
        case WKB_MULTICURVE:
            wkb_info->type = DM_MULTICURVE;
            return CE_None;
            break;
        case WKB_MULTISURFACE:
            wkb_info->type = DM_MULTISURFACE;
            return CE_None;
            break;
        case WKB_CURVE:
            wkb_info->type = DM_CURVEPOLY;
            return CE_None;
            break;
        case WKB_SURFACE:
            wkb_info->type = DM_MULTICURVE;
            return CE_None;
            break;
        case WKB_POLYHEDRALSURFACE:
            wkb_info->type = DM_POLYHEDRALSURFACE;
            return CE_None;
            break;
        case WKB_TIN:
            wkb_info->type = DM_TIN;
            return CE_None;
            break;
        case WKB_TRIANGLE:
            wkb_info->type = DM_TRIANGLE;
            return CE_None;
            break;

        default: /* Error! */
            break;
    }

    return CE_Failure;
}

/************************************************************************/
/*                      OGRDAMENG_Gser_From_GBox()                      */
/************************************************************************/

size_t OGRDAMENG_Gser_From_GBox(OGREnvelope3D *sEnvelope, GByte *buf,
                                GInt8 has_z, GInt8 has_m)
{
    GByte *loc = buf;
    float *f;
    GByte i = 0;
    size_t return_size;

    f = reinterpret_cast<float *>(buf);
    f[i++] = next_float_down(sEnvelope->MinX);
    f[i++] = next_float_up(sEnvelope->MaxX);
    f[i++] = next_float_down(sEnvelope->MinY);
    f[i++] = next_float_up(sEnvelope->MaxY);
    loc += 4 * sizeof(float);

    if (has_z)
    {
        f[i++] = next_float_down(sEnvelope->MinZ);
        f[i++] = next_float_up(sEnvelope->MaxZ);
        loc += 2 * sizeof(float);
    }

    return_size = static_cast<size_t>(loc - buf);
    return return_size;
}

/************************************************************************/
/*                     OGRDAMENG_Update_WKB_Info()                      */
/************************************************************************/

CPLErr OGRDAMENG_Update_WKB_Info(wkb_info *wkb_info)
{
    char wkb_little_endian;
    if (OGRDAMENG_WKB_Parse_Check(wkb_info, BYTE_SIZE))
        return CE_Failure;

    wkb_little_endian = wkb_info->pos[0];
    wkb_info->pos += BYTE_SIZE;

    if (wkb_little_endian != 1 && wkb_little_endian != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid endian flag value encountered.");
        return CE_Failure;
    }

    /* Machine arch is big endian, request is for little */
    if (IS_BIG_ENDIAN && wkb_little_endian)
        wkb_info->swap_bytes = TRUE;
    /* Machine arch is little endian, request is for big */
    else if ((!IS_BIG_ENDIAN) && (!wkb_little_endian))
        wkb_info->swap_bytes = TRUE;

    return OGRDAMENG_Type_From_WKB_State(wkb_info);
}

/************************************************************************/
/*                      OGRDAMENG_Gser_From_WKB()                       */
/************************************************************************/

size_t OGRDAMENG_Gser_From_WKB(wkb_info *wkb_info, GByte *buf, GByte *depth)
{
    if (*depth != 1)
        OGRDAMENG_Update_WKB_Info(wkb_info);

    /* Do the right thing */
    switch (wkb_info->type)
    {
        case DM_POINT:
            return OGRDAMENG_GserPoint_From_WKB(wkb_info, buf);
            break;
        case DM_LINE:
            return OGRDAMENG_GserLine_From_WKB(wkb_info, buf);
            break;
        case DM_CIRCSTRING:
            return OGRDAMENG_GserCircstring_From_WKB(wkb_info, buf);
            break;
        case DM_POLYGON:
            return OGRDAMENG_GserPoly_From_WKB(wkb_info, buf);
            break;
        case DM_TRIANGLE:
            return OGRDAMENG_GserTriangle_From_WKB(wkb_info, buf);
            break;
        case DM_CURVEPOLY:
            return OGRDAMENG_GserCurvepoly_From_WKB(wkb_info, buf, depth);
            break;
        case DM_MULTIPOINT:
        case DM_MULTILINE:
        case DM_MULTIPOLYGON:
        case DM_COMPOUND:
        case DM_MULTICURVE:
        case DM_MULTISURFACE:
        case DM_POLYHEDRALSURFACE:
        case DM_TIN:
        case DM_COLLECTION:
            return OGRDAMENG_GserCollection_From_WKB(wkb_info, buf, depth);
            break;

            /* Unknown type! */
        default:
            CPLError(CE_Failure, CPLE_AppDefined, "Unsupported geometry type");
    }
    return NULL;
}

/************************************************************************/
/*                  OGRDAMENG_Gser_Get_Expected_Size()                  */
/************************************************************************/

size_t OGRDAMENG_Gser_Get_Expected_Size(wkb_info *wkb_info, GByte *depth)
{
    if (*depth != 1)
        OGRDAMENG_Update_WKB_Info(wkb_info);

    switch (wkb_info->type)
    {
        case DM_POINT:
            return OGRDAMENG_GserPoint_Size_From_WKB(wkb_info);
            break;
        case DM_LINE:
        case DM_CIRCSTRING:
        case DM_TRIANGLE:
            return OGRDAMENG_GserLine_Size_From_WKB(wkb_info);
            break;
        case DM_POLYGON:
            return OGRDAMENG_GserPoly_Size_From_WKB(wkb_info);
            break;
        case DM_CURVEPOLY:
        case DM_MULTIPOINT:
        case DM_MULTILINE:
        case DM_MULTIPOLYGON:
        case DM_COMPOUND:
        case DM_MULTICURVE:
        case DM_MULTISURFACE:
        case DM_POLYHEDRALSURFACE:
        case DM_TIN:
        case DM_COLLECTION:
            return OGRDAMENG_GserCollection__Size_From_WKB(wkb_info, depth);
            break;
            /* Unknown type! */
        default:
            break;
    }
    return 1;
}

/************************************************************************/
/*                  OGRDAMENG_Get_WKB_Info_From_WKB()                   */
/************************************************************************/

CPLErr OGRDAMENG_Get_WKB_Info_From_WKB(wkb_info *wkb_info, GByte *wkb,
                                       size_t wkb_size,
                                       OGREnvelope3D *sEnvelope)
{
    wkb_info->wkb = wkb;
    wkb_info->wkb_size = wkb_size;
    wkb_info->swap_bytes = FALSE;
    wkb_info->pos = wkb;
    wkb_info->has_z = 0;
    wkb_info->has_m = 0;
    wkb_info->ndims = 2;
    wkb_info->srid = 0;
    wkb_info->type = 0;

    if (!isnan(sEnvelope->MaxX) && sEnvelope->MaxX != sEnvelope->MinX &&
        sEnvelope->MaxY != sEnvelope->MinY)
        wkb_info->need_box = 1;
    else
        wkb_info->need_box = 0;

    return OGRDAMENG_Update_WKB_Info(wkb_info);
}

/************************************************************************/
/*                   OGRDAMENG_Gserialized_From_WKB()                   */
/************************************************************************/

GSERIALIZED *OGRDAMENG_Gserialized_From_WKB(GByte *wkb, size_t wkb_size,
                                            size_t *size,
                                            OGREnvelope3D *sEnvelope)
{
    size_t expected_size = 0;
    size_t return_size = 0;
    wkb_info wkbinfo;
    wkb_info origin_info;
    GByte depth = 1;
    GByte *ptr = NULL;
    GSERIALIZED *g = NULL;

    if (OGRDAMENG_Get_WKB_Info_From_WKB(&wkbinfo, wkb, wkb_size, sEnvelope) ==
        CE_Failure)
        return NULL;

    origin_info = wkbinfo;
    /* size of gserialized head. */
    expected_size += 8;
    /* size of bbox. */
    if (wkbinfo.need_box)
        expected_size += 2 * (2 + wkbinfo.has_z + wkbinfo.has_m) * FLOAT_SIZE;
    /* get expected size of gserialized without head. */
    expected_size += OGRDAMENG_Gser_Get_Expected_Size(&wkbinfo, &depth);
    /* reset wkb_info. */
    wkbinfo = origin_info;

    ptr = reinterpret_cast<GByte *>(CPLMalloc(expected_size));
    g = reinterpret_cast<GSERIALIZED *>(ptr);

    g->gflags = 0;

    DM_SET_Z(g->gflags, wkbinfo.has_z);
    DM_SET_M(g->gflags, wkbinfo.has_m);
    DM_SET_VERSION(g->gflags, 1);

    g->srid[0] = static_cast<GByte>((wkbinfo.srid & 0x001F0000) >> 16);
    g->srid[1] = static_cast<GByte>((wkbinfo.srid & 0x0000FF00) >> 8);
    g->srid[2] = static_cast<GByte>((wkbinfo.srid & 0x000000FF));

    if (wkbinfo.need_box)
    {
        DM_SET_BBOX(g->gflags, 1);
        ptr += 8;
        ptr += OGRDAMENG_Gser_From_GBox(sEnvelope, ptr, wkbinfo.has_z,
                                        wkbinfo.has_m);
    }
    else
        ptr += 8;

    /* Write in the serialized form of the geometry. */
    ptr += OGRDAMENG_Gser_From_WKB(&wkbinfo, ptr, &depth);

    /* Calculate size as returned by data processing functions. */
    return_size = ptr - reinterpret_cast<GByte *>(g);

    /* Realloc serialized. */
    DM_SIZE_SET(g->size, return_size);
    if (expected_size != return_size)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error in expected_size!");
        CPLFree(g);
        return nullptr;
    }

    /* Return the output size. */
    *size = return_size;
    return g;
}

/************************************************************************/
/*                      OGRDAMENGGeo_From_Hexwkb()                      */
/************************************************************************/

GSERIALIZED *OGRDAMENGGeo_From_Hexwkb(const char *pszLEHex, size_t *size,
                                      OGREnvelope3D sEnvelope)
{
    if (pszLEHex == nullptr)
        return nullptr;

    int nWKBLength;
    GByte *pabyBin = CPLHexToBinary(pszLEHex, &nWKBLength);
    GSERIALIZED *result =
        OGRDAMENG_Gserialized_From_WKB(pabyBin, nWKBLength, size, &sEnvelope);

    CPLFree(pabyBin);

    return result;
}

/************************************************************************/
/*                       OGRDAMENG_Get_WKB_Type()                       */
/************************************************************************/

static GUInt32 OGRDAMENG_Get_WKB_Type(GByte type, GByte has_z, GByte has_m,
                                      GByte has_srid)
{
    GUInt32 wkb_type = 0;

    switch (type)
    {
        case DM_POINT:
            wkb_type = WKB_POINT;
            break;
        case DM_LINE:
            wkb_type = WKB_LINESTRING;
            break;
        case DM_POLYGON:
            wkb_type = WKB_POLYGON;
            break;
        case DM_MULTIPOINT:
            wkb_type = WKB_MULTIPOINT;
            break;
        case DM_MULTILINE:
            wkb_type = WKB_MULTILINESTRING;
            break;
        case DM_MULTIPOLYGON:
            wkb_type = WKB_MULTIPOLYGON;
            break;
        case DM_COLLECTION:
            wkb_type = WKB_GEOMETRYCOLLECTION;
            break;
        case DM_CIRCSTRING:
            wkb_type = WKB_CIRCULARSTRING;
            break;
        case DM_COMPOUND:
            wkb_type = WKB_COMPOUNDCURVE;
            break;
        case DM_CURVEPOLY:
            wkb_type = WKB_CURVEPOLYGON;
            break;
        case DM_MULTICURVE:
            wkb_type = WKB_MULTICURVE;
            break;
        case DM_MULTISURFACE:
            wkb_type = WKB_MULTISURFACE;
            break;
        case DM_POLYHEDRALSURFACE:
            wkb_type = WKB_POLYHEDRALSURFACE;
            break;
        case DM_TIN:
            wkb_type = WKB_TIN;
            break;
        case DM_TRIANGLE:
            wkb_type = WKB_TRIANGLE;
            break;
        default:
            break;
    }

    if (has_z)
        wkb_type |= WKB_Z;

    if (has_m)
        wkb_type |= WKB_M;
    if (has_srid)
        wkb_type |= WKB_SRID;

    return wkb_type;
}

/************************************************************************/
/*                    OGRDAMENG_Get_Point_Internal()                    */
/************************************************************************/

static GByte *OGRDAMENG_Get_Point_Internal(GByte *ptlist, GUInt32 n,
                                           GByte ndims)
{
    size_t size;
    GByte *ptr;

    size = sizeof(double) * ndims;
    ptr = ptlist + size * n;

    return ptr;
}

/************************************************************************/
/*                    OGRDAMENG_Integer_To_WKB_Buf()                    */
/************************************************************************/

static GByte *OGRDAMENG_Integer_To_WKB_Buf(GUInt32 ival, GByte *buf)
{
    GByte *iptr = reinterpret_cast<GByte *>(&ival);

    if (IS_BIG_ENDIAN)
    {
        for (int i = 0; i < INT_SIZE; i++)
        {
            buf[i] = iptr[INT_SIZE - 1 - i];
        }
    }
    else
    {
        memcpy(buf, iptr, INT_SIZE);
    }

    return buf + INT_SIZE;
}

/************************************************************************/
/*                    OGRDAMENG_Double_To_WKB_Buf()                     */
/************************************************************************/

static GByte *OGRDAMENG_Double_To_WKB_Buf(double d, GByte *buf)
{
    GByte *dptr = reinterpret_cast<GByte *>(&d);

    /* Machine/request arch mismatch, so flip byte order */
    if (IS_BIG_ENDIAN)
    {
        for (int i = 0; i < DOUBLE_SIZE; i++)
        {
            buf[i] = dptr[DOUBLE_SIZE - 1 - i];
        }
    }
    /* If machine arch and requested arch match, don't flip byte order */
    else
    {
        memcpy(buf, dptr, DOUBLE_SIZE);
    }

    return buf + DOUBLE_SIZE;
}

/************************************************************************/
/*                    OGRDAMENG_Ptarray_To_WKB_Buf()                    */
/************************************************************************/

static GByte *OGRDAMENG_Ptarray_To_WKB_Buf(GByte *ptlist, GUInt32 npoints,
                                           GByte *buf, GByte ndims,
                                           GByte need_npoints)
{
    double *dbl_ptr;

    if (need_npoints)
        buf = OGRDAMENG_Integer_To_WKB_Buf(npoints, buf);

    if (npoints && IS_BIG_ENDIAN)
    {
        size_t size = npoints * ndims * DOUBLE_SIZE;
        memcpy(buf, OGRDAMENG_Get_Point_Internal(ptlist, 0, ndims), size);
        buf += size;
    }
    else
    {
        for (int i = 0; i < static_cast<int>(npoints); i++)
        {
            dbl_ptr = reinterpret_cast<double *>(
                OGRDAMENG_Get_Point_Internal(ptlist, i, ndims));
            for (int j = 0; j < ndims; j++)
            {
                buf = OGRDAMENG_Double_To_WKB_Buf(dbl_ptr[j], buf);
            }
        }
    }

    return buf;
}

/************************************************************************/
/*                    OGRDAMENG_Empty_To_WKB_Size()                     */
/************************************************************************/

static size_t OGRDAMENG_Empty_To_WKB_Size(GByte type, GByte ndims,
                                          GByte has_srid)
{
    size_t size = BYTE_SIZE + INT_SIZE;

    if (has_srid)
        size += INT_SIZE;

    if (type == DM_POINT)
    {
        size += DOUBLE_SIZE * ndims;
    }
    else
    {
        size += INT_SIZE;
    }

    return size;
}

/************************************************************************/
/*                   OGRDAMENG_Ptarray_To_WKB_Size()                    */
/************************************************************************/

static size_t OGRDAMENG_Ptarray_To_WKB_Size(GUInt32 npoints, GByte dims,
                                            GByte need_npoints)
{
    size_t size = (need_npoints ? INT_SIZE : 0) + npoints * dims * DOUBLE_SIZE;

    return size;
}

/************************************************************************/
/*                    OGRDAMENG_Point_To_WKB_Size()                     */
/************************************************************************/

static size_t OGRDAMENG_Point_To_WKB_Size(GUInt32 npoints, GByte ndims,
                                          GByte has_srid)
{
    size_t size = BYTE_SIZE + INT_SIZE;
    if (npoints < 1)
        return OGRDAMENG_Empty_To_WKB_Size(DM_POINT, ndims, has_srid);

    if (has_srid)
        size += INT_SIZE;

    size += OGRDAMENG_Ptarray_To_WKB_Size(npoints, ndims, 0);
    return size;
}

/************************************************************************/
/*                     OGRDAMENG_Line_To_WKB_Size()                     */
/************************************************************************/

static size_t OGRDAMENG_Line_To_WKB_Size(GUInt32 npoints, GByte ndims,
                                         GByte has_srid)
{
    size_t size = BYTE_SIZE + INT_SIZE;
    if (npoints < 1)
        return OGRDAMENG_Empty_To_WKB_Size(DM_LINE, ndims, has_srid);

    if (has_srid)
        size += INT_SIZE;

    size += OGRDAMENG_Ptarray_To_WKB_Size(npoints, ndims, 1);
    return size;
}

/************************************************************************/
/*                     OGRDAMENG_Poly_To_WKB_Size()                     */
/************************************************************************/

static size_t OGRDAMENG_Poly_To_WKB_Size(GUInt32 allpoints, GUInt32 nrings,
                                         GByte ndims, GByte has_srid)
{
    size_t size = BYTE_SIZE + INT_SIZE + INT_SIZE;

    if (nrings < 1 || allpoints < 1)
        return OGRDAMENG_Empty_To_WKB_Size(DM_POLYGON, ndims, has_srid);

    if (has_srid)
        size += INT_SIZE;

    size += nrings * INT_SIZE;

    size += OGRDAMENG_Ptarray_To_WKB_Size(allpoints, ndims, 0);

    return size;
}

/************************************************************************/
/*                   OGRDAMENG_Triangle_To_WKB_Size()                   */
/************************************************************************/

static size_t OGRDAMENG_Triangle_To_WKB_Size(GUInt32 npoints, GByte ndims,
                                             GByte has_srid)
{
    size_t size = BYTE_SIZE + INT_SIZE;
    if (npoints < 1)
        return OGRDAMENG_Empty_To_WKB_Size(DM_TRIANGLE, ndims, has_srid);

    if (has_srid)
        size += INT_SIZE;

    size += OGRDAMENG_Ptarray_To_WKB_Size(npoints, ndims, 1);
    return size;
}

/************************************************************************/
/*                    OGRDAMENG_Endian_To_WKB_Buf()                     */
/************************************************************************/

static GByte *OGRDAMENG_Endian_To_WKB_Buf(GByte *buf)
{
    buf[0] = (IS_BIG_ENDIAN ? 0 : 1);
    return buf + 1;
}

/************************************************************************/
/*                  OGRDAMENG_Double_NAN_To_WKB_Buf()                   */
/************************************************************************/

static GByte *OGRDAMENG_Double_NAN_To_WKB_Buf(GByte *buf)
{
#define NAN_SIZE 8
    const GByte ndr_nan[NAN_SIZE] = {0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0xf8, 0x7f};
    const GByte xdr_nan[NAN_SIZE] = {0x7f, 0xf8, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00};
    int i;

    for (i = 0; i < NAN_SIZE; i++)
    {
        buf[i] = IS_BIG_ENDIAN ? xdr_nan[i] : ndr_nan[i];
    }

    return buf + NAN_SIZE;
}

/************************************************************************/
/*                     OGRDAMENG_Empty_To_WKB_Buf()                     */
/************************************************************************/

static GByte *OGRDAMENG_Empty_To_WKB_Buf(GByte type, GByte has_z, GByte has_m,
                                         int srid, GByte *buf)
{
    GUInt32 wkb_type =
        OGRDAMENG_Get_WKB_Type(type, has_z, has_m, srid != SRID_UNKNOWN);
    int i;

    buf = OGRDAMENG_Endian_To_WKB_Buf(buf);
    buf = OGRDAMENG_Integer_To_WKB_Buf(wkb_type, buf);
    if (srid != SRID_UNKNOWN)
        buf = OGRDAMENG_Integer_To_WKB_Buf(srid, buf);

    if (type == DM_POINT)
    {
        for (i = 0; i < (2 + has_z + has_m); i++)
            buf = OGRDAMENG_Double_NAN_To_WKB_Buf(buf);
    }
    else
    {
        buf = OGRDAMENG_Integer_To_WKB_Buf(0, buf);
    }

    return buf;
}

/************************************************************************/
/*                    OGRDAMENG_Header_To_WKB_Buf()                     */
/************************************************************************/

static GByte *OGRDAMENG_Header_To_WKB_Buf(GByte type, GByte has_z, GByte has_m,
                                          int srid, GByte *buf)
{
    buf = OGRDAMENG_Endian_To_WKB_Buf(buf);
    buf = OGRDAMENG_Integer_To_WKB_Buf(
        OGRDAMENG_Get_WKB_Type(type, has_z, has_m, srid != SRID_UNKNOWN), buf);
    if (srid != SRID_UNKNOWN)
        buf = OGRDAMENG_Integer_To_WKB_Buf(srid, buf);
    return buf;
}

/************************************************************************/
/*                OGRDAMENG_WKBPoint_From_Gserialized()                 */
/************************************************************************/

static GByte *OGRDAMENG_WKBPoint_From_Gserialized(GByte *data_ptr, GByte has_z,
                                                  GByte has_m, size_t *g_size,
                                                  int *wkb_size, int srid)
{
    GByte *start_ptr = data_ptr;
    GByte *buffer;
    GByte *start_buffer;
    GByte ndims = 2 + has_z + has_m;
    GUInt32 npoints = 0;
    size_t size = 0;

    data_ptr += INT_SIZE; /* Skip past the type. */
    npoints =
        *reinterpret_cast<GUInt32 *>(data_ptr); /* Zero => empty geometry */
    data_ptr += INT_SIZE;                       /* Skip past the npoints. */

    size = OGRDAMENG_Point_To_WKB_Size(npoints, ndims, srid != SRID_UNKNOWN);
    buffer = reinterpret_cast<GByte *>(CPLMalloc(size));

    if (!buffer)
        return NULL;

    start_buffer = buffer;

    if (npoints < 1)
        buffer =
            OGRDAMENG_Empty_To_WKB_Buf(DM_POINT, has_z, has_m, srid, buffer);
    else
    {
        buffer =
            OGRDAMENG_Header_To_WKB_Buf(DM_POINT, has_z, has_m, srid, buffer);
        buffer = OGRDAMENG_Ptarray_To_WKB_Buf(data_ptr, npoints, buffer,
                                              (2 + has_z + has_m), 0);
    }

    *wkb_size = static_cast<int>(buffer - start_buffer);

    data_ptr += npoints * ndims * sizeof(double);

    if (*wkb_size != size)
        return NULL;
    if (g_size)
        *g_size = data_ptr - start_ptr;

    return start_buffer;
}

/************************************************************************/
/*                 OGRDAMENG_WKBLine_From_Gserialized()                 */
/************************************************************************/

static GByte *OGRDAMENG_WKBLine_From_Gserialized(GByte *data_ptr, GByte has_z,
                                                 GByte has_m, size_t *g_size,
                                                 int *wkb_size, int srid)
{
    GByte *start_ptr = data_ptr;
    GByte *buffer;
    GByte *start_buffer;
    GByte ndims = 2 + has_z + has_m;
    GUInt32 npoints = 0;
    size_t size = 0;

    data_ptr += INT_SIZE; /* Skip past the type. */
    npoints =
        *reinterpret_cast<GUInt32 *>(data_ptr); /* Zero => empty geometry */
    data_ptr += INT_SIZE;                       /* Skip past the npoints. */

    size = OGRDAMENG_Line_To_WKB_Size(npoints, ndims, srid != SRID_UNKNOWN);
    buffer = reinterpret_cast<GByte *>(CPLMalloc(size));

    if (!buffer)
        return NULL;

    start_buffer = buffer;

    if (npoints < 1)
        buffer =
            OGRDAMENG_Empty_To_WKB_Buf(DM_LINE, has_z, has_m, srid, buffer);
    else
    {
        buffer =
            OGRDAMENG_Header_To_WKB_Buf(DM_LINE, has_z, has_m, srid, buffer);
        buffer = OGRDAMENG_Ptarray_To_WKB_Buf(data_ptr, npoints, buffer,
                                              (2 + has_z + has_m), 1);
    }

    *wkb_size = static_cast<int>(buffer - start_buffer);

    data_ptr += npoints * ndims * sizeof(double);

    if (*wkb_size != size)
        return NULL;
    if (g_size)
        *g_size = data_ptr - start_ptr;

    return start_buffer;
}

/************************************************************************/
/*              OGRDAMENG_WKBCircstring_From_Gserialized()              */
/************************************************************************/

static GByte *OGRDAMENG_WKBCircstring_From_Gserialized(GByte *data_ptr,
                                                       GByte has_z, GByte has_m,
                                                       size_t *g_size,
                                                       int *wkb_size, int srid)
{
    GByte *start_ptr = data_ptr;
    GByte *buffer;
    GByte *start_buffer;
    GByte ndims = 2 + has_z + has_m;
    GUInt32 npoints = 0;
    size_t size = 0;

    data_ptr += INT_SIZE; /* Skip past the type. */
    npoints =
        *reinterpret_cast<GUInt32 *>(data_ptr); /* Zero => empty geometry */
    data_ptr += INT_SIZE;                       /* Skip past the npoints. */

    size = OGRDAMENG_Line_To_WKB_Size(npoints, ndims, srid != SRID_UNKNOWN);
    buffer = reinterpret_cast<GByte *>(CPLMalloc(size));
    if (!buffer)
        return NULL;
    start_buffer = buffer;

    if (npoints < 1)
        buffer = OGRDAMENG_Empty_To_WKB_Buf(DM_CIRCSTRING, has_z, has_m, srid,
                                            buffer);
    else
    {
        buffer = OGRDAMENG_Header_To_WKB_Buf(DM_CIRCSTRING, has_z, has_m, srid,
                                             buffer);
        buffer = OGRDAMENG_Ptarray_To_WKB_Buf(data_ptr, npoints, buffer,
                                              (2 + has_z + has_m), 1);
    }

    *wkb_size = static_cast<int>(buffer - start_buffer);

    data_ptr += npoints * ndims * sizeof(double);

    if (*wkb_size != size)
        return NULL;
    if (g_size)
        *g_size = data_ptr - start_ptr;

    return start_buffer;
}

/************************************************************************/
/*                 OGRDAMENG_WKBPoly_From_Gserialized()                 */
/************************************************************************/

static GByte *OGRDAMENG_WKBPoly_From_Gserialized(GByte *data_ptr, GByte has_z,
                                                 GByte has_m, size_t *g_size,
                                                 int *wkb_size, int srid)
{
    GByte *start_ptr = data_ptr;
    GByte *buffer;
    GByte *start_buffer;
    GByte *ordinate_ptr;
    GByte ndims = 2 + has_z + has_m;
    GUInt32 nrings = 0;
    GUInt32 allpoints = 0;
    GUInt32 i = 0;
    size_t size = 0;

    data_ptr += INT_SIZE; /* Skip past the type. */
    nrings =
        *reinterpret_cast<GUInt32 *>(data_ptr); /* Zero => empty geometry */
    data_ptr += INT_SIZE;                       /* Skip past the npoints. */

    /* Move past all the npoints values.If there is padding, move past that too. */
    ordinate_ptr = data_ptr + nrings * 4 + ((nrings % 2) ? 4 : 0);

    for (i = 0; i < nrings; i++)
    {
        allpoints += *reinterpret_cast<GUInt32 *>(data_ptr);
    }

    size = OGRDAMENG_Poly_To_WKB_Size(allpoints, nrings, ndims,
                                      srid != SRID_UNKNOWN);
    buffer = reinterpret_cast<GByte *>(CPLMalloc(size));

    if (!buffer)
        return NULL;

    start_buffer = buffer;

    if (allpoints < 1)
        buffer =
            OGRDAMENG_Empty_To_WKB_Buf(DM_POLYGON, has_z, has_m, srid, buffer);
    else
    {
        buffer =
            OGRDAMENG_Header_To_WKB_Buf(DM_POLYGON, has_z, has_m, srid, buffer);
        buffer = OGRDAMENG_Integer_To_WKB_Buf(nrings, buffer);

        for (i = 0; i < nrings; i++)
        {
            GUInt32 npoints = 0;

            /* Read in the number of points. */
            npoints = *reinterpret_cast<GUInt32 *>(data_ptr);
            buffer = OGRDAMENG_Integer_To_WKB_Buf(npoints, buffer);
            data_ptr += 4;

            buffer = OGRDAMENG_Ptarray_To_WKB_Buf(ordinate_ptr, npoints, buffer,
                                                  (2 + has_z + has_m), 0);

            ordinate_ptr += sizeof(double) * ndims * npoints;
        }
    }

    *wkb_size = static_cast<int>(buffer - start_buffer);
    if (*wkb_size != size)
        return NULL;

    if (g_size)
        *g_size = ordinate_ptr - start_ptr;

    return start_buffer;
}

/************************************************************************/
/*               OGRDAMENG_WKBTriangle_From_Gserialized()               */
/************************************************************************/

static GByte *OGRDAMENG_WKBTriangle_From_Gserialized(GByte *data_ptr,
                                                     GByte has_z, GByte has_m,
                                                     size_t *g_size,
                                                     int *wkb_size, int srid)
{
    GByte *start_ptr = data_ptr;
    GByte *buffer;
    GByte *start_buffer;
    GByte ndims = 2 + has_z + has_m;
    GUInt32 npoints = 0;
    size_t size = 0;

    data_ptr += INT_SIZE; /* Skip past the type. */
    npoints =
        *reinterpret_cast<GUInt32 *>(data_ptr); /* Zero => empty geometry */
    data_ptr += INT_SIZE;                       /* Skip past the npoints. */

    size = OGRDAMENG_Triangle_To_WKB_Size(npoints, ndims, srid != SRID_UNKNOWN);
    buffer = reinterpret_cast<GByte *>(CPLMalloc(size));

    if (!buffer)
        return NULL;

    start_buffer = buffer;

    if (npoints < 1)
        buffer =
            OGRDAMENG_Empty_To_WKB_Buf(DM_TRIANGLE, has_z, has_m, srid, buffer);
    else
    {
        buffer = OGRDAMENG_Header_To_WKB_Buf(DM_TRIANGLE, has_z, has_m, srid,
                                             buffer);
        buffer = OGRDAMENG_Integer_To_WKB_Buf(1, buffer);
        buffer = OGRDAMENG_Ptarray_To_WKB_Buf(data_ptr, npoints, buffer,
                                              (2 + has_z + has_m), 1);
    }

    *wkb_size = static_cast<int>(buffer - start_buffer);

    data_ptr += npoints * ndims * sizeof(double);

    if (g_size)
        *g_size = data_ptr - start_ptr;

    return buffer;
}

/************************************************************************/
/*                OGRDAMENG_Collection_Allows_Subtype()                 */
/************************************************************************/

int OGRDAMENG_Collection_Allows_Subtype(int collectiontype, int subtype)
{
    if (collectiontype == DM_COLLECTION)
        return TRUE;
    if (collectiontype == DM_MULTIPOINT && subtype == DM_POINT)
        return TRUE;
    if (collectiontype == DM_MULTILINE && subtype == DM_LINE)
        return TRUE;
    if (collectiontype == DM_MULTIPOLYGON && subtype == DM_POLYGON)
        return TRUE;
    if (collectiontype == DM_COMPOUND &&
        (subtype == DM_LINE || subtype == DM_CIRCSTRING))
        return TRUE;
    if (collectiontype == DM_CURVEPOLY &&
        (subtype == DM_CIRCSTRING || subtype == DM_LINE ||
         subtype == DM_COMPOUND))
        return TRUE;
    if (collectiontype == DM_MULTICURVE &&
        (subtype == DM_CIRCSTRING || subtype == DM_LINE ||
         subtype == DM_COMPOUND))
        return TRUE;
    if (collectiontype == DM_MULTISURFACE &&
        (subtype == DM_POLYGON || subtype == DM_CURVEPOLY))
        return TRUE;
    if (collectiontype == DM_POLYHEDRALSURFACE && subtype == DM_POLYGON)
        return TRUE;
    if (collectiontype == DM_TIN && subtype == DM_TRIANGLE)
        return TRUE;

    /* Must be a bad combination! */
    return FALSE;
}

/************************************************************************/
/*              OGRDAMENG_WKBCollection_From_Gserialized()              */
/************************************************************************/

static GByte *OGRDAMENG_WKBCollection_From_Gserialized(GByte *data_ptr,
                                                       GByte has_z, GByte has_m,
                                                       size_t *g_size,
                                                       int *wkb_size, int srid)
{
    GUInt32 type;
    GByte *buffer = nullptr;
    GByte *header = nullptr;
    GUInt32 ngeoms = 0;
    int size = BYTE_SIZE + INT_SIZE + INT_SIZE;
    GUInt32 i;

    type = *reinterpret_cast<GUInt32 *>(data_ptr);
    data_ptr += INT_SIZE; /* Skip past the type. */
    ngeoms = *reinterpret_cast<GUInt32 *>(data_ptr);
    data_ptr += INT_SIZE; /* Skip past the ngeoms. */

    if (srid)
        size += INT_SIZE;

    buffer = reinterpret_cast<GByte *>(CPLMalloc(size));
    header = buffer;
    header = OGRDAMENG_Header_To_WKB_Buf(static_cast<GByte>(type), has_z, has_m,
                                         srid, header);
    OGRDAMENG_Integer_To_WKB_Buf(ngeoms, header);

    for (i = 0; i < ngeoms; i++)
    {
        GUInt32 subtype = *reinterpret_cast<GUInt32 *>(data_ptr);
        size_t subsize = 0;
        GByte *subbuffer = nullptr;
        GByte *tmp_buffer = nullptr;
        int subwritten_size = 0;

        if (!OGRDAMENG_Collection_Allows_Subtype(type, subtype))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Error subtype in collection!");
            if (buffer)
                CPLFree(buffer);
            return NULL;
        }
        subbuffer = OGRDAMENG_Wkb_From_Gserialized(
            data_ptr, has_z, has_m, &subsize, &subwritten_size, 0);
        if (!subbuffer)
        {
            if (buffer)
                CPLFree(buffer);
            return NULL;
        }

        tmp_buffer = reinterpret_cast<GByte *>(
            CPLRealloc(buffer, size + subwritten_size));
        if (!tmp_buffer)
        {
            CPLFree(buffer);
            return NULL;
        }
        else
            buffer = tmp_buffer;
        memcpy(buffer + size, subbuffer, subwritten_size);
        size += subwritten_size;
        data_ptr += subsize;
        CPLFree(subbuffer);
    }

    *wkb_size = size;

    return buffer;
}

/************************************************************************/
/*                   OGRDAMENG_Wkb_From_Gserialized()                   */
/************************************************************************/

static GByte *OGRDAMENG_Wkb_From_Gserialized(GByte *data_ptr, GByte has_z,
                                             GByte has_m, size_t *g_size,
                                             int *wkb_size, int srid)
{
    GUInt32 type;

    type = *reinterpret_cast<GUInt32 *>(data_ptr);

    switch (type)
    {
        case DM_POINT:
            return OGRDAMENG_WKBPoint_From_Gserialized(data_ptr, has_z, has_m,
                                                       g_size, wkb_size, srid);
        case DM_LINE:
            return OGRDAMENG_WKBLine_From_Gserialized(data_ptr, has_z, has_m,
                                                      g_size, wkb_size, srid);
        case DM_CIRCSTRING:
            return OGRDAMENG_WKBCircstring_From_Gserialized(
                data_ptr, has_z, has_m, g_size, wkb_size, srid);
        case DM_POLYGON:
            return OGRDAMENG_WKBPoly_From_Gserialized(data_ptr, has_z, has_m,
                                                      g_size, wkb_size, srid);
        case DM_TRIANGLE:
            return OGRDAMENG_WKBTriangle_From_Gserialized(
                data_ptr, has_z, has_m, g_size, wkb_size, srid);
        case DM_MULTIPOINT:
        case DM_MULTILINE:
        case DM_MULTIPOLYGON:
        case DM_COMPOUND:
        case DM_CURVEPOLY:
        case DM_MULTICURVE:
        case DM_MULTISURFACE:
        case DM_POLYHEDRALSURFACE:
        case DM_TIN:
        case DM_COLLECTION:
            return OGRDAMENG_WKBCollection_From_Gserialized(
                data_ptr, has_z, has_m, g_size, wkb_size, srid);
        default:
            return NULL;
    }
}

/************************************************************************/
/*                    OGRDAMENG_Gser_To_Wkb_Buffer()                    */
/************************************************************************/

GByte *OGRDAMENG_Gser_To_Wkb_Buffer(GSERIALIZED *gser, int *b_size)
{
    size_t xflags = 0;
    int srid = 0;
    GByte *data_ptr = NULL;
    GInt8 has_z;
    GInt8 has_m;
    GInt8 has_bbox;
    GInt8 is_geodetic;

    srid = srid | (gser->srid[0] << 16);
    srid = srid | (gser->srid[1] << 8);
    srid = srid | (gser->srid[2]);
    srid = (srid << 11) >> 11;
    if (srid == 0)
        srid = SRID_UNKNOWN;

    has_z = DM_GET_Z(gser->gflags);
    has_m = DM_GET_M(gser->gflags);
    has_bbox = DM_GET_BBOX(gser->gflags);
    is_geodetic = DM_GET_GEODETIC(gser->gflags);

    if (DM_GET_EXTENDED(gser->gflags))
        memcpy(&xflags, gser->data, sizeof(size_t));

    data_ptr = reinterpret_cast<GByte *>(gser->data);
    if (DM_GET_EXTENDED(gser->gflags))
        data_ptr += sizeof(size_t);

    if (has_bbox)
    {
        if (is_geodetic)
            data_ptr += 3 * 2 * sizeof(float);
        else
            data_ptr += (2 + has_z + has_m) * 2 * sizeof(float);
    }

    return OGRDAMENG_Wkb_From_Gserialized(data_ptr, has_z, has_m, 0, b_size,
                                          srid);
}

/************************************************************************/
/*                      OGRDAMENG_Geo_To_Hexwkb()                       */
/************************************************************************/

GByte *OGRDAMENG_Geo_To_Hexwkb(GSERIALIZED *geom, int *size)
{
    GByte *wkb;

    if (!geom)
        return NULL;

    wkb = OGRDAMENG_Gser_To_Wkb_Buffer(geom, size);

    return wkb;
}

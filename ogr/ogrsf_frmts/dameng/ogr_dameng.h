/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/DaMeng driver.
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

#ifndef OGR_DM_H_INCLUDED
#define OGR_DM_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "DPI.h"
#include "DPIext.h"
#include "DPItypes.h"

#define UNDETERMINED_SRID -2
#define NDCT_IDCLS_PACKAGE 14
#define NDCT_PKGID_DMGEO2 (NDCT_IDCLS_PACKAGE << 24 | 112)
#define NDCT_CLSID_GEO2_ST_GEOMETRY (NDCT_IDCLS_PACKAGE << 24 | 113)
#define NDCT_CLSID_GEO2_ST_POINT (NDCT_IDCLS_PACKAGE << 24 | 114)
#define NDCT_CLSID_GEO2_ST_LINE (NDCT_IDCLS_PACKAGE << 24 | 115)
#define NDCT_CLSID_GEO2_ST_POLYGON (NDCT_IDCLS_PACKAGE << 24 | 116)
#define NDCT_CLSID_GEO2_ST_MULTIPOINT (NDCT_IDCLS_PACKAGE << 24 | 117)
#define NDCT_CLSID_GEO2_ST_MULTILINE (NDCT_IDCLS_PACKAGE << 24 | 118)
#define NDCT_CLSID_GEO2_ST_MULTIPOLYGON (NDCT_IDCLS_PACKAGE << 24 | 119)
#define NDCT_CLSID_GEO2_ST_COLLECTION (NDCT_IDCLS_PACKAGE << 24 | 120)
#define NDCT_CLSID_GEO2_ST_CIRCSTRING (NDCT_IDCLS_PACKAGE << 24 | 121)
#define NDCT_CLSID_GEO2_ST_COMPOUND (NDCT_IDCLS_PACKAGE << 24 | 122)
#define NDCT_CLSID_GEO2_ST_CURVEPOLY (NDCT_IDCLS_PACKAGE << 24 | 123)
#define NDCT_CLSID_GEO2_ST_MULTICURVE (NDCT_IDCLS_PACKAGE << 24 | 124)
#define NDCT_CLSID_GEO2_ST_MULTISURFACE (NDCT_IDCLS_PACKAGE << 24 | 125)
#define NDCT_CLSID_GEO2_ST_POLYHEDRALSURFACE (NDCT_IDCLS_PACKAGE << 24 | 126)
#define NDCT_CLSID_GEO2_ST_TRIANGLE (NDCT_IDCLS_PACKAGE << 24 | 127)
#define NDCT_CLSID_GEO2_ST_TIN (NDCT_IDCLS_PACKAGE << 24 | 128)
#define NDCT_CLSID_GEO2_ST_GEOGRAPHY (NDCT_IDCLS_PACKAGE << 24 | 129)

#define fetchnum 100000
#define FORCED_INSERT_NUM 1

extern int ogr_DM_insertnum;
class OGRDAMENGDataSource;
class OGRDAMENGLayer;

typedef enum
{
    GEOM_TYPE_UNKNOWN = 0,
    GEOM_TYPE_GEOMETRY = 1,
    GEOM_TYPE_GEOGRAPHY = 2,
    GEOM_TYPE_WKB = 3
} DMGeoType;

typedef struct
{
    unsigned int size; /* For DMGEO2 use only, use VAR* macros to manipulate. */
    unsigned char srid[3]; /* 24 bits of SRID */
    unsigned char gflags;  /* HasZ, HasM, HasBBox, IsGeodetic */
    unsigned char data[1]; /* See gserialized.txt */
} GSERIALIZED;

typedef struct
{
    char *pszName;
    char *pszGeomType;
    int GeometryTypeFlags;
    int nSRID;
    DMGeoType eDAMENGGeoType;
    int bNullable;
} DMGeomColumnDesc;

class CPL_DLL OGRDAMENGConn
{
  public:
    dhenv hEnv;
    DPIRETURN hRtn;
    dhcon hCon;

    char *pszUserid;
    char *pszPassword;
    char *pszDatabase;

  public:
    OGRDAMENGConn();
    virtual ~OGRDAMENGConn();
    int EstablishConn(const char *pszUserid, const char *pszPassword,
                      const char *pszDatabase, const char *pszSchemaName);
};

typedef struct
{
    sdbyte name[128 + 1];
    sdint2 nameLen;

    sdint2 sql_type;
    ulength prec;
    sdint2 scale;
    sdint2 nullable;

    slength display_size;
} DmColDesc;

CPLString OGRDAMENGEscapeColumnName(const char *pszColumnName);

class OGRDAMENGGeomFieldDefn final : public OGRGeomFieldDefn
{
    OGRDAMENGGeomFieldDefn(const OGRDAMENGGeomFieldDefn &) = delete;
    OGRDAMENGGeomFieldDefn &operator=(const OGRDAMENGGeomFieldDefn &) = delete;

  protected:
    OGRDAMENGLayer *poLayer;

  public:
    OGRDAMENGGeomFieldDefn(OGRDAMENGLayer *poLayerIn,
                       const char *pszFieldName)
        : OGRGeomFieldDefn(pszFieldName, wkbUnknown), poLayer(poLayerIn),
          nSRSId(UNDETERMINED_SRID), GeometryTypeFlags(0),
          eDAMENGGeoType(GEOM_TYPE_UNKNOWN)
    {
    }

    virtual const OGRSpatialReference *GetSpatialRef() const override;

    void UnsetLayer()
    {
        poLayer = nullptr;
    }

    mutable int nSRSId;
    mutable int GeometryTypeFlags;
    mutable DMGeoType eDAMENGGeoType;
};

class OGRDAMENGFeatureDefn CPL_NON_FINAL : public OGRFeatureDefn
{
  public:
    explicit OGRDAMENGFeatureDefn(const char *pszName = nullptr)
        : OGRFeatureDefn(pszName)
    {
        SetGeomType(wkbNone);
    }

    virtual void UnsetLayer()
    {
        const int nGeomFieldCount = GetGeomFieldCount();
        for (int i = 0; i < nGeomFieldCount; i++)
            cpl::down_cast<OGRDAMENGGeomFieldDefn *>(apoGeomFieldDefn[i].get())
                ->UnsetLayer();
    }

    OGRDAMENGGeomFieldDefn *GetGeomFieldDefn(int i) override
    {
        return cpl::down_cast<OGRDAMENGGeomFieldDefn *>(
            OGRFeatureDefn::GetGeomFieldDefn(i));
    }

    const OGRDAMENGGeomFieldDefn *GetGeomFieldDefn(int i) const override
    {
        return cpl::down_cast<const OGRDAMENGGeomFieldDefn *>(
            OGRFeatureDefn::GetGeomFieldDefn(i));
    }
};

class CPL_DLL OGRDAMENGStatement
{
  public:
    explicit OGRDAMENGStatement(OGRDAMENGConn *);
    virtual ~OGRDAMENGStatement();
    dhstmt *GetStatement()
    {
        return &hStatement;
    }
    char *pszCommandText;
    CPLErr Prepare(const char *pszStatement);
    CPLErr ExecuteInsert(const char *pszSQLStatement, int nMode);
    CPLErr Execute(const char *pszStatement, int nMode = -1);
    CPLErr Excute_for_fetchmany(const char *pszStatement);
    void Clean();
    char **SimpleFetchRow();
    char ***Fetchmany(ulength *rows);
    int GetColCount() const
    {
        return nRawColumnCount;
    }
    int *blob_len;
    int **blob_lens;
    CPLErr Execute_for_insert(OGRDAMENGFeatureDefn *params,
                              OGRFeature *poFeature,
                              std::map<std::string, int> mymap);

  private:
    OGRDAMENGConn *poConn;
    dhstmt hStatement;
    int nRawColumnCount;
    char **result;
    char **papszCurImage;
    int *object_index;
    int *lob_index;
    dhobjdesc *objdesc;
    dhobj *obj;
    dhloblctr *lob;
    //used for fetchmany
    int is_fectmany;
    char ***results;
    dhobj **objs;
    dhloblctr **lobs;
    dhobjdesc **objdescs;
    char ***papszCurImages;
    int param_nums = 0;
    DmColDesc *paramdescs = nullptr;
    dhobj **insert_objs;
    dhobjdesc insert_objdesc;
    GSERIALIZED ***insert_geovalues;
    char ***insert_values;
    int geonum;
    int valuesnum;
    slength** col_len = nullptr;
    size_t gser_length = 0;
    int insert_num = 0;
};

class OGRDAMENGLayer CPL_NON_FINAL : public OGRLayer
{
    OGRDAMENGLayer(const OGRDAMENGLayer &) = delete;
    OGRDAMENGLayer &operator=(const OGRDAMENGLayer &) = delete;

  protected:
    OGRDAMENGFeatureDefn *poFeatureDefn = nullptr;

    int iNextShapeId = 0;
    int iFIDColumn = 0;
    int iGeomColumn = 0;

    static OGRGeometry *BlobToGeometry(const char *);
    static GByte *BlobToGByteArray(const char *pszBlob,
                                   int *pnLength);
    static char *GeometryToBlob(const OGRGeometry *);
    void SetInitialQuery();

    OGRDAMENGDataSource *poDS = nullptr;

    char *pszQueryStatement = nullptr;

    OGRDAMENGStatement *poStatement;
    int nResultOffset = 0;

    char *pszFIDColumn = nullptr;
    char *pszGeomColumn = nullptr;
    int *m_panMapFieldNameToIndex = nullptr;
    int *m_panMapFieldNameToGeomIndex = nullptr;

    virtual CPLString GetFromClauseForGetExtent() = 0;
    OGRErr RunGetExtentRequest(OGREnvelope &sExtent,
                               int bForce,
                               CPLString osCommand,
                               int bErrorAsDebug);
    static void CreateMapFromFieldNameToIndex(OGRDAMENGStatement *hStmt,
                                              OGRFeatureDefn *poFeatureDefn,
                                              int *&panMapFieldNameToIndex,
                                              int *&panMapFieldNameToGeomIndex);

    int ReadResultDefinition(OGRDAMENGStatement *hInitialResultIn);

    OGRFeature *RecordToFeature(OGRDAMENGStatement *hResult,
                                const int *panMapFieldNameToIndex,
                                const int *panMapFieldNameToGeomIndex,
                                int iRecord);

    OGRFeature *GetNextRawFeature();
    OGRDAMENGStatement **stmt = nullptr;
    int col_count;
    ulength rows = 0;
    ulength total_rows = 0;
    char ***result;
    int isfetchall = 0;

  public:
    OGRDAMENGLayer();
    ~OGRDAMENGLayer() override;

    void ResetReading() override;

    static char *GByteArrayToBlob(const GByte *pabyData,
                                  size_t nLen);
    const OGRDAMENGFeatureDefn *GetLayerDefn() const override
    {
        return poFeatureDefn;
    }

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    const char *GetFIDColumn() const override;

    //virtual OGRErr SetNextByIndex(GIntBig nIndex) override;

    OGRDAMENGDataSource *GetDS()
    {
        return poDS;
    }

    GDALDataset *GetDataset() override;
    virtual void ResolveSRID(const OGRDAMENGGeomFieldDefn *poGFldDefn) = 0;
};

class OGRDAMENGTableLayer final : public OGRDAMENGLayer
{
    OGRDAMENGTableLayer(const OGRDAMENGTableLayer &) = delete;
    OGRDAMENGTableLayer &operator=(const OGRDAMENGTableLayer &) = delete;

    int bUpdate = false;

    void BuildWhere();
    CPLString BuildFields();
    void BuildFullQueryStatement();

    char *pszTableName = nullptr;
    char *pszSchemaName = nullptr;
    char *m_pszTableDescription = nullptr;
    CPLString osForcedDescription{};
    char *pszSqlTableName = nullptr;
    int bTableDefinitionValid = -1;

    CPLString osPrimaryKey{};

    int bGeometryInformationSet = false;

    /* Name of the parent table with the geometry definition if it is a derived table or NULL */
    char *pszSqlGeomParentTableName = nullptr;

    char *pszGeomColForced = nullptr;

    CPLString osQuery{};
    CPLString osWHERE{};

    int bLaunderColumnNames = true;
    int bPreservePrecision = true;
    int bCopyActive = false;
    bool bFIDColumnInCopyFields = false;
    int bFirstInsertion = true;

    OGRErr CreateFeatureViaInsert(OGRFeature *poFeature);

    int bHasWarnedIncompatibleGeom = false;
    void CheckGeomTypeCompatibility(int iGeomField,
                                    OGRGeometry *poGeom);

    int bHasWarnedAlreadySetFID = false;

    char **papszOverrideColumnTypes = nullptr;
    int nForcedSRSId = UNDETERMINED_SRID;
    int nForcedGeometryTypeFlags = -1;
    int nForcedCommitCount = 0;
    int nForcedInsert = 0;
    bool bCreateSpatialIndexFlag = true;
    CPLString osSpatialIndexType = "GIST";
    int bInResetReading = false;
    CPLString InsertSQL;
    OGRDAMENGStatement *InsertStatement = nullptr;
    int bAutoFIDOnCreateViaCopy = false;

    int bDeferredCreation = false;
    CPLString osCreateTable{};

    int iFIDAsRegularColumnIndex = -1;

    CPLString m_osFirstGeometryFieldName{};

    int checkINI;

    virtual CPLString GetFromClauseForGetExtent() override
    {
        return pszSqlTableName;
    }
    OGRErr RunAddGeometryColumn(const OGRDAMENGGeomFieldDefn *poGeomField);

    CPLErr CheckINI(int *checkini);

  public:
    OGRDAMENGTableLayer(OGRDAMENGDataSource *,
                    CPLString &osCurrentSchema,
                    const char *pszTableName,
                    const char *pszSchemaName,
                    const char *pszDescriptionIn,
                    const char *pszGeomColForced,
                    int bUpdateIn);
    virtual ~OGRDAMENGTableLayer();
    std::map<std::string, int> mymap;
    void SetGeometryInformation(DMGeomColumnDesc *pasDesc,
                                int nGeomFieldCount);

    OGRFeature *GetFeature(GIntBig nFeatureId) override;
    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    GIntBig GetFeatureCount(int) override;

    OGRErr ISetSpatialFilter(int iGeomField,
                             const OGRGeometry *poGeom) override;

    OGRErr SetAttributeFilter(const char *) override;

    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr DeleteFeature(GIntBig nFID) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;
    virtual OGRErr CreateGeomField(const OGRGeomFieldDefn *poGeomField,
                                   int bApproxOK = TRUE) override;
    OGRErr DeleteField(int iField) override;
    virtual OGRErr AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                          int nFlags) override;
    int TestCapability(const char *) const override;
    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    const char *GetTableName()
    {
        return pszTableName;
    }
    const char *GetSchemaName()
    {
        return pszSchemaName;
    }

    const char *GetFIDColumn() const override;

    char **GetMetadataDomainList() override;
    CSLConstList GetMetadata(const char *pszDomain = "") override;
    virtual const char *GetMetadataItem(const char *pszName,
                                        const char *pszDomain = "") override;
    CPLErr SetMetadata(CSLConstList, const char *pszDomain = "") override;
    CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                           const char *pszDomain = "") override;

    OGRErr Rename(const char *pszNewName) override;

    // follow methods are not base class overrides
    void SetLaunderFlag(int bFlag)
    {
        bLaunderColumnNames = bFlag;
    }
    void SetPrecisionFlag(int bFlag)
    {
        bPreservePrecision = bFlag;
    }

    void SetOverrideColumnTypes(const char *pszOverrideColumnTypes);

    int ReadTableDefinition();
    int HasGeometryInformation()
    {
        return bGeometryInformationSet;
    }
    void SetTableDefinition(const char *pszFIDColumnName,
                            const char *pszGFldName,
                            OGRwkbGeometryType eType,
                            const char *pszGeomType,
                            int nSRSId,
                            int GeometryTypeFlags);

    void SetForcedSRSId(int nForcedSRSIdIn)
    {
        nForcedSRSId = nForcedSRSIdIn;
    }
    void SetForcedGeometryTypeFlags(int GeometryTypeFlagsIn)
    {
        nForcedGeometryTypeFlags = GeometryTypeFlagsIn;
    }
    void SetCreateSpatialIndex(bool bFlag, const char *pszSpatialIndexType)
    {
        bCreateSpatialIndexFlag = bFlag;
        osSpatialIndexType = pszSpatialIndexType;
    }
    void SetForcedDescription(const char *pszDescriptionIn);
    void AllowAutoFIDOnCreateViaCopy()
    {
        bAutoFIDOnCreateViaCopy = TRUE;
    }

    void SetDeferredCreation(CPLString osCreateTable);

    void ResolveSRID(const OGRDAMENGGeomFieldDefn *poGFldDefn) override;
};

class OGRDAMENGResultLayer final : public OGRDAMENGLayer
{
    OGRDAMENGResultLayer(const OGRDAMENGResultLayer &) = delete;
    OGRDAMENGResultLayer &operator=(const OGRDAMENGResultLayer &) = delete;

    void BuildFullQueryStatement();

    char *pszRawStatement = nullptr;

    char *pszGeomTableName = nullptr;
    char *pszGeomTableSchemaName = nullptr;

    CPLString osWHERE{};

    virtual CPLString GetFromClauseForGetExtent() override
    {
        return pszRawStatement;
    }

  public:
    OGRDAMENGResultLayer(OGRDAMENGDataSource *, const char *pszRawStatement,
                         OGRDAMENGStatement *hInitialResult);
    ~OGRDAMENGResultLayer() override;

    void ResetReading() override;
    GIntBig GetFeatureCount(int) override;

    OGRErr ISetSpatialFilter(int iGeomField,
                             const OGRGeometry *poGeom) override;

    int TestCapability(const char *) const override;

    OGRFeature *GetNextFeature() override;

    void ResolveSRID(const OGRDAMENGGeomFieldDefn *poGFldDefn) override;
};

/************************************************************************/
/*                           OGRDAMENGDataSource                        */
/************************************************************************/

class OGRDAMENGDataSource final : public OGRDataSource
{
    OGRDAMENGDataSource(const OGRDAMENGDataSource &) = delete;
    OGRDAMENGDataSource &operator=(const OGRDAMENGDataSource &) = delete;

    typedef struct
    {
        int nMajor;
        int nMinor;
        int nRelease;
    } DMver;

    OGRDAMENGTableLayer **papoLayers = nullptr;
    int nLayers = 0;

    char *pszName = nullptr;

    bool m_bUTF8ClientEncoding = false;

    int bDSUpdate = false;
    int bHaveGeography = true;

    int bUserTransactionActive = false;
    int bSavePointActive = false;
    int nSoftTransactionLevel = 0;

    OGRDAMENGConn *poSession = nullptr;

    OGRErr DeleteLayer(int iLayer) override;

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes.
    int nKnownSRID = 0;
    int *panSRID = nullptr;
    OGRSpatialReference **papoSRS = nullptr;

    OGRDAMENGTableLayer *poLayerInCopyMode = nullptr;

    CPLString osCurrentSchema{};

    int nUndefinedSRID = 0;

    char *pszForcedTables = nullptr;
    char **papszSchemaList = nullptr;
    int bHasLoadTables = false;
    CPLString osActiveSchema{};
    int bListAllTables = false;

    CPLString osDebugLastTransactionCommand{};

  public:
    int bBinaryTimeFormatIsInt8 = false;
    int bUseEscapeStringSyntax = false;

    bool m_bHasGeometryColumns = true;
    bool m_bHasSpatialRefSys = true;

    int GetUndefinedSRID() const
    {
        return nUndefinedSRID;
    }
    bool IsUTF8ClientEncoding() const
    {
        return m_bUTF8ClientEncoding;
    }

  public:
    OGRDAMENGDataSource();
    virtual ~OGRDAMENGDataSource();

    OGRDAMENGConn *GetDAMENGConn()
    {
        return poSession;
    }

    int FetchSRSId(const OGRSpatialReference *poSRS);
    OGRSpatialReference *FetchSRS(int nSRSId);

    int Open(const char *,
             int bUpdate,
             int bTestOpen,
             CSLConstList papszOpenOptions);
    OGRDAMENGTableLayer *OpenTable(CPLString &osCurrentSchema,
                               const char *pszTableName,
                               const char *pszSchemaName,
                               const char *pszDescription,
                               const char *pszGeomColForced,
                               int bUpdate,
                               int bTestOpen);

    const char *GetName() override
    {
        return pszName;
    }
    int GetLayerCount() const override;
    const OGRLayer *GetLayer(int) const override;
    OGRLayer *GetLayerByName(const char *pszName) override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    int TestCapability(const char *) const override;

    virtual OGRLayer *ExecuteSQL(const char *pszSQLCommand,
                                 OGRGeometry *poSpatialFilter,
                                 const char *pszDialect) override;
    virtual void ReleaseResultSet(OGRLayer *poLayer) override;

    virtual const char *GetMetadataItem(const char *pszKey,
                                        const char *pszDomain) override;
};

OGRDAMENGConn CPL_DLL *OGRGetDAMENGConnection(const char *pszUserid,
                                      const char *pszPassword,
                                      const char *pszDatabase,
                                      const char *pszSchemaName);

CPLString OGRDAMENGCommonLayerGetType(OGRFieldDefn &oField,
                                  bool bPreservePrecision,
                                  bool bApproxOK);

char *strToupper(char *str);

OGRwkbGeometryType OGRDAMENGCheckType(int typid);

char *OGRDAMENGCommonLaunderName(const char *pszSrcName,
                             const char *pszDebugPrefix = "OGR");

void OGRDAMENGCommonAppendFieldValue(CPLString &osCommand,
                                 OGRFeature *poFeature,
                                 int i);

GSERIALIZED* OGRDAMENGGeoFromHexwkb(const char* pszLEHex,
    size_t* size, OGREnvelope3D sEnvelope);

GByte* OGRDAMENGGeoToHexwkb(GSERIALIZED* geom,
    int* size);

#endif  // !OGR_DM_H_INCLUDED

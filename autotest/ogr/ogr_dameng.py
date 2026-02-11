#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Dameng driver functionality.
# Author:   Wu Yilun <wuyilun@dameng.com>
#
###############################################################################
# Copyright (c) 2025, Wu Yilun <wuyilun@dameng.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import os

import pytest
import gdaltest
import ogrtest

from osgeo import gdal, ogr, osr

pytestmark = [
    pytest.mark.skipif(
        "DAMENG_CONNECTION_STRING" not in os.environ,
        reason="DAMENG_CONNECTION_STRING not specified;"),
    pytest.mark.require_driver("DAMENG")
]

@pytest.fixture(scope="module", autouse=True)
def setup_and_cleanup():
    dm_dsname = os.environ.get("DAMENG_CONNECTION_STRING")
    if not dm_dsname:
        pytest.skip("Please set DAMENG_CONNECTION_STRING environment variable.")

    with gdaltest.disable_exceptions():
        gdaltest.dm_ds = ogr.Open(dm_dsname, update=1)

    if gdaltest.dm_ds is None:
        pytest.skip(f"Failed to connect to database: {dm_dsname}")

    yield  # Run tests

    # Cleanup after tests
    cleanup_tables()
    gdaltest.dm_ds = None
    gdaltest.shp_ds = None

def cleanup_tables():
    """Drop tables created during testing"""
    if gdaltest.dm_ds is None:
        return

    tables_to_drop = [
        "\"tpoly\"", "\"xpoly\"", "\"testsrs\"", "\"testsrs2\"",
        "\"geom_test\"", "\"testdate\"", "\"dm_test_20\"", "\"dm_test_21\"",
        "\"test_POINT\"", "\"test_LINESTRING\"", "\"test_POLYGON\"",
        "\"test_MULTIPOINT\"", "\"test_MULTILINESTRING\"",
        "\"test_MULTIPOLYGON\"", "\"test_GEOMETRYCOLLECTION\"", "\"test_NONE\""
    ]

    for table in tables_to_drop:
        try:
            with gdal.quiet_errors():
                gdaltest.dm_ds.ExecuteSQL(f"DROP TABLE IF EXISTS {table}")
        except Exception:
            pass


###############################################################################
# 1. Create Dameng table and import poly.shp data
@gdaltest.disable_exceptions()
def test_dameng_1_create_table_and_import():
    """Test creating a Dameng table and importing Shapefile data"""

    # Clean up any existing table
    with gdal.quiet_errors():
        gdaltest.dm_ds.ExecuteSQL('DROP TABLE IF EXISTS "tpoly"')

    # Create layer
    gdaltest.dm_lyr = gdaltest.dm_ds.CreateLayer("tpoly", options=["OVERWRITE=TRUE"])

    # Define fields
    ogrtest.quick_create_layer_def(
        gdaltest.dm_lyr,
        [
            ("AREA", ogr.OFTReal),
            ("EAS_ID", ogr.OFTInteger),
            ("PRFEDEA", ogr.OFTString)
        ]
    )

    # Import poly.shp data
    shp_ds = ogr.Open("data/poly.shp")
    if shp_ds is None:
        pytest.skip("Cannot find test data: data/poly.shp")

    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)

    gdaltest.poly_feat = []

    # Copy features
    for feat in shp_lyr:
        gdaltest.poly_feat.append(feat)

        dst_feat = ogr.Feature(feature_def=gdaltest.dm_lyr.GetLayerDefn())
        dst_feat.SetFrom(feat)
        gdaltest.dm_lyr.CreateFeature(dst_feat)

    # Test updating a non-existing feature
    shp_lyr.ResetReading()
    feat = shp_lyr.GetNextFeature()
    feat.SetFID(-10)
    assert (
        gdaltest.dm_lyr.SetFeature(feat) == ogr.OGRERR_NON_EXISTING_FEATURE
    ), "Expected failure of SetFeature()."

    # Test deleting a non-existing feature
    assert (
        gdaltest.dm_lyr.DeleteFeature(-10) == ogr.OGRERR_NON_EXISTING_FEATURE
    ), "Expected failure of DeleteFeature()."


###############################################################################
# 2. Verify imported data

def test_dameng_2_verify_imported_data():
    """Verify data imported into the Dameng table"""

    expected_ids = [168, 169, 166, 158, 165]

    # Set attribute filter
    gdaltest.dm_lyr.SetAttributeFilter("EAS_ID < 170")

    # Verify attribute values
    ogrtest.check_features_against_list(
        gdaltest.dm_lyr, "EAS_ID", expected_ids
    )

    gdaltest.dm_lyr.SetAttributeFilter(None)

    # Verify geometry and attributes
    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.dm_lyr.GetNextFeature()

        # Verify geometry
        ref_geom = orig_feat.GetGeometryRef()
        ref_geom.SetCoordinateDimension(2)
        ogrtest.check_feature_geometry(
            read_feat, ref_geom, max_error=0.000000001
        )

        # Verify attributes
        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), \
                f"Field {fld} value mismatch"

    gdaltest.poly_feat = None
    gdaltest.shp_ds = None


###############################################################################
# 3. Test various geometry types

def test_dameng_3_geometry_types():
    """Test writing and reading various WKT geometry types"""

    wkt_test_cases = ["10", "2", "1", "3d_1", "4", "5", "6"]

    for item in wkt_test_cases:
        # Create geometry object
        wkt = open("data/wkb_wkt/" + item + ".wkt").read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        # Create and write feature
        dst_feat = ogr.Feature(feature_def=gdaltest.dm_lyr.GetLayerDefn())
        dst_feat.SetGeometryDirectly(geom)
        dst_feat.SetField("PRFEDEA", item)
        gdaltest.dm_lyr.CreateFeature(dst_feat)

        # Read and verify
        gdaltest.dm_lyr.SetAttributeFilter("PRFEDEA = '%s'" % item)
        feat_read = gdaltest.dm_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(feat_read, geom)


###############################################################################
# 4. Test SQL queries

def test_dameng_4_sql_queries():
    """Test SQL query functionality in Dameng database"""

    expect = [None, 179, 173, 172, 171, 170, 169, 168, 166, 165, 158]

    sql_lyr = gdaltest.dm_ds.ExecuteSQL(
        "SELECT DISTINCT eas_id FROM \"tpoly\" ORDER BY eas_id DESC"
    )
    assert sql_lyr.GetLayerDefn().GetGeomFieldCount() == 0

    ogrtest.check_features_against_list(sql_lyr, "eas_id", expect)

    gdaltest.dm_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# 5. Test spatial SQL queries

def test_dameng_5_sql_queries_with_geometry():
    """Test SQL queries with geometry in Dameng database"""

    sql_lyr = gdaltest.dm_ds.ExecuteSQL("SELECT * FROM \"tpoly\" where prfedea = '2'")
    assert sql_lyr.GetLayerDefn().GetGeomFieldCount() == 1

    try:
        ogrtest.check_features_against_list(sql_lyr, "PRFEDEA", ['2'])
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        geom = feat_read.GetGeometryRef()
        ogrtest.check_feature_geometry(
            geom,
            "MULTILINESTRING ((5.00121349 2.99853132,5.00121349 1.99853133),(5.00121349 1.99853133,5.00121349 0.99853133),(3.00121351 1.99853127,5.00121349 1.99853133),(5.00121349 1.99853133,6.00121348 1.99853135))",
            max_error = 1e-3,
            )
    finally:
        gdaltest.dm_ds.ReleaseResultSet(sql_lyr)

###############################################################################
# 6. Test spatial filtering

def test_dameng_6_spatial_filter():
    """Test spatial filtering functionality in Dameng database"""

    # Create spatial filter geometry
    filter_geom = ogr.CreateGeometryFromWkt(
        "POLYGON ((479000 4763000, 481000 4763000, 481000 4764000, 479000 4764000, 479000 4763000))"
    )

    gdaltest.dm_lyr.SetSpatialFilter(filter_geom)

    # Get number of filtered features
    filtered_count = gdaltest.dm_lyr.GetFeatureCount()
    assert filtered_count >= 0, "Spatial filter query failed"

    # Reset spatial filter
    gdaltest.dm_lyr.SetSpatialFilter(None)

    # Verify all features can be retrieved after reset
    total_count = gdaltest.dm_lyr.GetFeatureCount()
    assert total_count >= filtered_count, "Feature count should increase after resetting filter"


###############################################################################
# 7. Test coordinate system support

def test_dameng_7_coordinate_systems():
    """Test coordinate system support in Dameng database"""

    # Clean up test table
    with gdal.quiet_errors():
        gdaltest.dm_ds.ExecuteSQL("DROP TABLE IF EXISTS testsrs")

    # Create WGS84 spatial reference
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("WGS84")
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    # Create layer with spatial reference
    dm_lyr2 = gdaltest.dm_ds.CreateLayer(
        "testsrs",
        srs=srs,
        geom_type=ogr.wkbPoint,
        options=["OVERWRITE=YES"]
    )

    # Create fields
    ogrtest.quick_create_layer_def(
        dm_lyr2,
        [("name", ogr.OFTString)]
    )

    # Add test feature
    feat = ogr.Feature(dm_lyr2.GetLayerDefn())
    feat.SetField("name", "test_point")
    point_geom = ogr.CreateGeometryFromWkt("POINT (116.0 39.0)")
    feat.SetGeometry(point_geom)
    dm_lyr2.CreateFeature(feat)

    # Verify spatial reference
    srs2 = dm_lyr2.GetSpatialRef()
    assert srs2 is not None, "Layer should have a spatial reference"

    # For Dameng, specific EPSG codes might be required
    wkt1 = srs.ExportToWkt()
    wkt2 = srs2.ExportToWkt()
    assert srs2.IsSame(srs), f"Spatial references do not match\n Original: {wkt1}\n Read: {wkt2}"


###############################################################################
# 8. Test date/time fields

def test_dameng_8_datetime_fields():
    """Test date/time field support in Dameng database"""

    # Clean up test table
    with gdal.quiet_errors():
        gdaltest.dm_ds.ExecuteSQL("DROP TABLE IF EXISTS testdate")

    # Create layer with date fields
    lyr = gdaltest.dm_ds.CreateLayer(
        "testdate",
        geom_type=ogr.wkbNone,
        options=["OVERWRITE=YES"]
    )

    # Add various date fields
    lyr.CreateField(ogr.FieldDefn("date_field", ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn("time_field", ogr.OFTTime))
    lyr.CreateField(ogr.FieldDefn("datetime_field", ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn("string_field", ogr.OFTString))

    # Add test data
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("date_field", "2023-12-01")
    feat.SetField("time_field", "14:30:45")
    feat.SetField("datetime_field", "2023-12-01 14:30:45")
    feat.SetField("string_field", "test")
    lyr.CreateFeature(feat)

    # Verify field types
    lyr_def = lyr.GetLayerDefn()
    assert lyr_def.GetFieldDefn(0).GetType() == ogr.OFTDate
    assert lyr_def.GetFieldDefn(1).GetType() == ogr.OFTTime
    assert lyr_def.GetFieldDefn(2).GetType() == ogr.OFTDateTime

    # Sync to disk
    lyr.SyncToDisk()


###############################################################################
# 9. Test NOT NULL constraints and default values

def test_dameng_9_constraints_and_defaults():
    """Test field constraints and default values in Dameng database"""

    # Clean up test table
    with gdal.quiet_errors():
        gdaltest.dm_ds.ExecuteSQL("DROP TABLE IF EXISTS dm_test_20")

    # Create layer with NOT NULL geometry
    lyr = gdaltest.dm_ds.CreateLayer(
        "dm_test_20",
        geom_type=ogr.wkbPoint,
        options=["OVERWRITE=YES", "GEOMETRY_NULLABLE=NO"]
    )

    # Create NOT NULL field
    field_defn = ogr.FieldDefn("required_field", ogr.OFTString)
    field_defn.SetNullable(0)  # NOT NULL
    lyr.CreateField(field_defn)

    # Create nullable field
    field_defn2 = ogr.FieldDefn("optional_field", ogr.OFTString)
    field_defn2.SetNullable(1)  # Nullable
    lyr.CreateField(field_defn2)

    # Add test feature (must provide geometry and required field)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("required_field", "required_value")
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    result = lyr.CreateFeature(feat)
    assert result == ogr.OGRERR_NONE, "Creating valid feature should succeed"

    lyr.SyncToDisk()


###############################################################################
# 10. Test error handling
@gdaltest.disable_exceptions()
def test_dameng_10_error_handling():
    """Test error handling in Dameng driver"""

    # Test invalid SQL
    with gdal.quiet_errors():
        invalid_lyr = gdaltest.dm_ds.ExecuteSQL("SELECT * FROM non_existent_table")

    assert invalid_lyr is None, "Invalid SQL should return None"

    # Test invalid geometry
    with gdal.quiet_errors():
        invalid_geom = ogr.CreateGeometryFromWkt("INVALID WKT")
        if invalid_geom is not None:
            feat = ogr.Feature(gdaltest.dm_lyr.GetLayerDefn())
            feat.SetGeometry(invalid_geom)
            result = gdaltest.dm_lyr.CreateFeature(feat)
            assert result != ogr.OGRERR_NONE, "Inserting invalid geometry should fail"

    # Test invalid connection
    with gdal.quiet_errors():
        invalid_ds = ogr.Open("DM:invalid_user/invalid_password@invalid_host:invalid_port")
    assert invalid_ds is None, "Invalid connection should return None."


###############################################################################
# 11. Comprehensive test: Create, Read, Update, Delete (CRUD)

@gdaltest.disable_exceptions()
def test_dameng_11_crud_operations():
    """Test CRUD operations in Dameng database"""

    # Clean up test table
    with gdal.quiet_errors():
        gdaltest.dm_ds.ExecuteSQL("DROP TABLE IF EXISTS crud_test")

    # 1. Create table
    crud_lyr = gdaltest.dm_ds.CreateLayer(
        "crud_test",
        geom_type=ogr.wkbPoint,
        options=["OVERWRITE=YES"]
    )

    crud_lyr.CreateField(ogr.FieldDefn("NAME", ogr.OFTString))
    crud_lyr.CreateField(ogr.FieldDefn("VALUE", ogr.OFTInteger))

    # 2. Insert data
    test_features = []
    for i in range(5):
        feat = ogr.Feature(crud_lyr.GetLayerDefn())
        feat.SetField("NAME", f"feature_{i}")
        feat.SetField("VALUE", i * 10)
        feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt(f"POINT ({i} {i})"))
        result = crud_lyr.CreateFeature(feat)
        assert result == ogr.OGRERR_NONE, f"CreateFeature{i} failed."
        test_features.append(i + 1)

    # 3. Query data
    crud_lyr.SetAttributeFilter("VALUE > 20")
    count = crud_lyr.GetFeatureCount()
    assert count == 2, f"GetFeatureCount() method error,expected 2,actually {count}"
    crud_lyr.SetAttributeFilter(None)

    # 4. Update data
    crud_lyr.ResetReading()
    feat = crud_lyr.GetNextFeature()
    if feat is not None:
        fid = feat.GetFID()
        feat.SetField("name", "updated_name")
        result = crud_lyr.SetFeature(feat)
        assert result == ogr.OGRERR_NONE, "SetFeature() method failed."

        # Verify update
        updated_feat = crud_lyr.GetFeature(fid)
        assert updated_feat.GetField("name") == "updated_name", "SetFeature() seems to have had no effect."

    # 5. Delete data
    if test_features:
        result = crud_lyr.DeleteFeature(test_features[0])
        assert result == ogr.OGRERR_NONE, "DeleteFeature() method failed."

        # Verify deletion
        deleted_feat = crud_lyr.GetFeature(test_features[0])
        assert deleted_feat is None, "DeleteFeature() seems to have had no effect."


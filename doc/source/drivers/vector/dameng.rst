.. _vector.dameng:

Dameng (DM)
=====

.. shortname:: Dameng

.. build_dependencies:: Dameng (DM) library

This driver implements read and write access for spatial data in
`Dameng <http://www.dameng.com/>`__ tables.

The connection string supports specific extensions for performance tuning and schema selection:
Basic Format: DAMENG:"username/password@host:port"

Schema Selection: Append ?schema_name to the end of the connection string to specify the target schema.

Example: DAMENG:"SYSDBA/Password_123@localhost:5236?MY_SCHEMA"

Batch Insert Size: Append ;batch_size after the port (or after the schema if specified) to enable batch insertion.(default 1)
Syntax: ...@host:port;N or ...@host:port?schema;N

Where N is the number of rows to insert in a single batch.
Note: Enabling batch insertion significantly improves data loading efficiency but requires sufficient server memory and CPU resources. If the machine performance is insufficient, reducing the batch size or disabling this feature is recommended.

Example (Batch of 1000 rows): DAMENG:"SYSDBA/Password_123@localhost:5236;1000"

Example (Schema + Batch): DAMENG:"SYSDBA/Password_123@localhost:5236?MY_SCHEMA;500"

If no schema is specified, the default schema associated with the user login is used.

Currently all regular user tables containing geometry columns are assumed to be layers from an OGR
point of view, with the table names as the layer names. Named views are
not currently supported.

If a single integer field is a primary key, it will be used as the FID;
otherwise, the FID will be assigned sequentially, and fetches by FID may be
slower.

By default, SQL statements are passed directly to the Dameng database
engine. It's also possible to request the driver to handle SQL commands
with :ref:`OGR SQL <ogr_sql_dialect>` engine, by passing **"OGRSQL"**
string to the ExecuteSQL() method, as name of the SQL dialect.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

Caveats
-------

-   In the case of a layer defined by a SQL statement, fields either
    named "OGC_FID" or those that are defined as NOT NULL, are a PRIMARY
    KEY, and are an integer-like field will be assumed to be the FID.
-   Geometry fields are read from Dameng using standard WKB or DM specific binary formats.
-   The OGR_FID column, which can be overridden with the FID layer
    creation option, is typically implemented as an IDENTITY or auto-incrementing integer field.
-   The geometry column, which defaults to SHAPE (or GEOMETRY) and can be overridden
    with the :lco:`GEOMETRY_NAME` layer creation option.
-   SRS information is stored using the OGC Simple Features for SQL
    layout. Dameng maintains metadata tables similar to *geometry_columns* and *spatial_ref_sys*.
    If no EPSG code is found for a given table, the system SRID will be used.
    Batch Insert Performance: When the batch insert size is configured via the connection string, the driver buffers features before committing. This reduces network round-trips and transaction overhead. However, large batch sizes may lead to high memory consumption on the client or server side during the operation.
    The Dameng driver opens a connection using the standard Dameng client protocol.
    Character encoding is handled according to the database instance configuration.

Creation Issues
---------------

The Dameng driver does not support creation of new databases (instances), but it does allow
creation of new layers (tables) within an existing schema. If a schema is specified in the
connection string, layers will be created within that schema.

By default, the Dameng driver will attempt to preserve the precision of
OGR features when creating and reading Dameng layers. For integer fields,
it will use appropriate numeric types. For real fields, it will use **DOUBLE**
or **FLOAT**. For string fields, **VARCHAR** or **VARCHAR2** will be used.

The Dameng driver supports transactions, leveraging Dameng's native transactional capabilities.

When batch insertion is enabled, commits occur after every N features (as specified).

Layer Creation Options
~~~~~~~~~~~~~~~~~~~~~~

|about-layer-creation-options|
The following layer creation options are supported:

-  .. lco:: OVERWRITE
      :choices: YES, NO

      This may be "YES" to force an existing layer of the
      desired name to be destroyed before creating the requested layer.

-  .. lco:: LAUNDER
      :choices: YES, NO
      :default: YES

      This may be "YES" to force new fields created on this
      layer to have their field names "laundered" into a form more
      compatible with Dameng (e.g., converting special characters). If "NO" exact names are
      preserved (subject to Dameng identifier rules).

-  .. lco:: PRECISION
      :choices: TRUE, FALSE
      :default: TRUE

      This may be "TRUE" to attempt to preserve field widths
      and precisions for the creation and reading of Dameng layers.

-  .. lco:: GEOMETRY_NAME
      :default: SHAPE

      This option specifies the name of the geometry
      column. In Dameng, this column must be of a spatial type (e.g., GEOMETRY).

-  .. lco:: FID
      :default: OGR_FID

      This option specifies the name of the FID column.

-  .. lco:: DIMENSION
      :choices: 2, 3
      :default: 2

      Specifies the dimension of the geometry column (2D or 3D).

**Examples**
------------

The following example datasource name opens the database with username *SYSDBA*,
password *Password_123* on localhost port *5236*, targeting the *TEST_SCHEMA* schema,
and enabling batch insertion of 1000 rows at a time.

::

   DAMENG:"SYSDBA/Password_123@localhost:5236?TEST_SCHEMA;1000"

The following example uses ogr2ogr to copy the world_borders layer from a shapefile
into a Dameng table. It targets the default schema, overwrites the existing table
*borders_dm*, and sets the geometry column name to *GEOMETRY*. Note that batch insertion
is controlled via the datasource name, not the ogr2ogr flags.

::

   ogr2ogr -f "Dameng" "DAMENG:SYSDBA/Password_123@localhost:5236;500" world_borders.shp -nln borders_dm -update -overwrite -lco GEOMETRY_NAME=GEOMETRY

The following example uses ogrinfo to return summary information about the
*borders_dm* layer located in the *DATA_SCHEMA* schema.

::

   ogrinfo "DAMENG:SYSDBA/Password_123@localhost:5236?DATA_SCHEMA" borders_dm -so

       Layer name: borders_dm
       Geometry: Polygon
       Feature Count: 3784
       Extent: (-180.000000, -90.000000) - (180.000000, 83.623596)
       Layer SRS WKT:
       GEOGCS["GCS_WGS_1984",
           DATUM["WGS_1984",
               SPHEROID["WGS_84",6378137,298.257223563]],
           PRIMEM["Greenwich",0],
           UNIT["Degree",0.017453292519943295]]
       FID Column = OGR_FID
       Geometry Column = GEOMETRY
       cat: Real (0.0)
       fips_cntry: String (80.0)
       cntry_name: String (80.0)
       area: Real (15.2)
       pop_cntry: Real (15.2)


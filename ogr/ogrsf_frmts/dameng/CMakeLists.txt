add_gdal_driver(
  TARGET ogr_DAMENG
  SOURCES ogrdamengdatasource.cpp
          ogrdamengconnection.cpp
          ogr_dameng.h
          ogrdamengdriver.cpp
          ogrdamenglayer.cpp
          ogrdamengstatement.cpp
          ogrdamengresultlayer.cpp
          ogrdamengtablelayer.cpp
          ogrdamengutility.cpp
          ogrdamengtransform.cpp
          PLUGIN_CAPABLE)
gdal_standard_includes(ogr_DAMENG)

if(NOT TARGET ogr_DAMENG)
    return()
endif()

gdal_standard_includes(ogr_DAMENG)
gdal_target_link_libraries(ogr_DAMENG PRIVATE Dameng::Dameng)

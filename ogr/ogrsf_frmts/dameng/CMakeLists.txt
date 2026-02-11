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

target_include_directories(ogr_DAMENG PRIVATE ${DAMENG_INCLUDE_DIRS} $<TARGET_PROPERTY:SOURCE_DIR>)
gdal_target_link_libraries(ogr_DAMENG PRIVATE ${DAMENG_LIBRARY})

if (OGR_ENABLE_DRIVER_DAMENG_PLUGIN)
    if(NOT MSVC AND CMAKE_THREAD_LIBS_INIT)
        gdal_target_link_libraries(ogr_DAMENG PRIVATE ${CMAKE_THREAD_LIBS_INIT})
    endif()
endif()

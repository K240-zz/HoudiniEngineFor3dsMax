
project ( HoudiniEngine3dsMax${Max_VERSION} ) 

add_definitions( -D_SCL_SECURE_NO_WARNINGS ) 
add_definitions( -D_CRT_SECURE_NO_WARNINGS )

if( Max_VERSION MATCHES "2013")
    add_definitions( -D_UNICODE ) 
    add_definitions( -DUNICODE ) 
    SET( 3DSMAX_DIR_ROOT $ENV{ADSK_3DSMAX_SDK_2013} ) 
endif()

if( Max_VERSION MATCHES "2014")
    add_definitions( -D_UNICODE ) 
    add_definitions( -DUNICODE ) 
    SET( 3DSMAX_DIR_ROOT $ENV{ADSK_3DSMAX_SDK_2014} ) 
endif()

if( Max_VERSION MATCHES "2015")
    add_definitions( -D_UNICODE ) 
    add_definitions( -DUNICODE ) 
    SET( 3DSMAX_DIR_ROOT $ENV{ADSK_3DSMAX_SDK_2015} ) 
endif()

if( Max_VERSION MATCHES "2016")
    add_definitions( -D_UNICODE ) 
    add_definitions( -DUNICODE ) 
    SET( 3DSMAX_DIR_ROOT $ENV{ADSK_3DSMAX_SDK_2016} ) 
endif()

if( NOT EXISTS ${3DSMAX_DIR_ROOT} )
    message(FATAL_ERROR "Can't find 3DSMAX_DIR_ROOT: ${3DSMAX_DIR_ROOT}" )
endif()

SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zm124" )

SET( HOUDINI_DIR_ROOT $ENV{HOUDINI_ROOT} )

if( NOT EXISTS ${HOUDINI_DIR_ROOT} )
    message(FATAL_ERROR "Can't find HOUDINI_DIR_ROOT: ${HOUDINI_DIR_ROOT}" )
endif()

SET( HOUDINI_TOOLKIT ${HOUDINI_DIR_ROOT}/toolkit} ) 


SET( 3DSMAX_DIR_INCLUDE ${3DSMAX_DIR_ROOT}/include )
if( NOT EXISTS ${3DSMAX_DIR_INCLUDE} )
    message(FATAL_ERROR "Can't find 3DSMAX_DIR_INCLUDE: ${3DSMAX_DIR_INCLUDE}" )
endif()

SET( HOUDINI_INCLUDE ${HOUDINI_DIR_ROOT}/toolkit/include )
if( NOT EXISTS ${HOUDINI_INCLUDE} )
    message(FATAL_ERROR "Can't find HOUDINI_INCLUDE: ${HOUDINI_INCLUDE}" )
endif()

get_filename_component( BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" PATH )

set( LOCAL_CMAKE_FILE_NAME "${BASE_DIR}/CMakeLists.txt" )

file(GLOB Sources ${BASE_DIR}/*.cpp)
file(GLOB Includes ${BASE_DIR}/*.h)

file(GLOB_RECURSE Scripts ${BASE_DIR}/MaxScripts/*.*)

SOURCE_GROUP("Source Files" FILES ${Sources})   
SOURCE_GROUP("Header Files" FILES ${Includes})
SOURCE_GROUP("_Maxscripts" FILES ${Scripts})

#setup_precompiled_header( ${BASE_DIR} ${Sources} )

if( Max_VERSION MATCHES "2013" )
    SET( 3DSMAX_DIR_LIBRARY ${3DSMAX_DIR_ROOT}/x64/lib )
else()
    SET( 3DSMAX_DIR_LIBRARY ${3DSMAX_DIR_ROOT}/lib/x64/Release )
endif()

SET( HOUDINI_LIBRARY ${HOUDINI_DIR_ROOT}/custom/houdini/dsolib )

SET( 3DSMAX_LIBRARIES comctl32 bmm core geom gfx mesh maxutil maxscrpt paramblk2 mnmath poly edmodel ) 

SET( HOUDINI_LIBRARIES libHAPI.a) 

set( Sources ${Sources} ${BASE_DIR}/HoudiniEngine.rc ${BASE_DIR}/HoudiniEngine.def )

include_directories( ${3DSMAX_DIR_INCLUDE} 
    ${HOUDINI_INCLUDE}
)

link_directories( ${3DSMAX_DIR_LIBRARY} 
    ${HOUDINI_LIBRARY}
)

add_library( ${PROJECT_NAME} SHARED ${Sources} ${Includes}  ${LOCAL_CMAKE_FILE_NAME} )

TARGET_LINK_LIBRARIES( ${PROJECT_NAME}
    ${3DSMAX_LIBRARIES}
    ${HOUDINI_LIBRARIES}
    )

SET_TARGET_PROPERTIES( ${PROJECT_NAME} PROPERTIES SUFFIX ".dlo" ) 


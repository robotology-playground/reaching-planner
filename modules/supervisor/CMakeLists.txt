# Copyright: (C) 2016 iCub Facility - Istituto Italiano di Tecnologia
# Author: Nguyen Dong Hai Phuong
# CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT

project(reaching-supervisor)

set(PROJECTNAME reaching-supervisor)
project(${PROJECTNAME})
aux_source_directory(. SRC_LIST)

include(YarpIDL)

set (include_files	${CMAKE_CURRENT_SOURCE_DIR}/include/reachingSupervisor.h
			${CMAKE_CURRENT_SOURCE_DIR}/include/multipleParticleThread.h
			${CMAKE_CURRENT_SOURCE_DIR}/include/particleWaypointThread.h
			${CMAKE_CURRENT_SOURCE_DIR}/include/particleThread.h)

set (source_files 	${CMAKE_CURRENT_SOURCE_DIR}/src/reachingSupervisor.cpp
		 	${CMAKE_CURRENT_SOURCE_DIR}/src/supervisorModule.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/src/multipleParticleThread.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/src/particleWaypointThread.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/src/particleThread.cpp)

set(idl_files    reachingSupervisor.thrift)

yarp_add_idl(IDL_GEN_FILES reachingSupervisor.thrift)

source_group("IDL Files" FILES ${idl_files})

include_directories(	${CMAKE_CURRENT_SOURCE_DIR}
                        ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_definitions(-D_USE_MATH_DEFINES)

add_executable(${PROJECTNAME} ${SRC_LIST} ${lib_files} ${source_files} ${include_files} ${IDL_GEN_FILES} ${idl_files})# ${local_lib_files})
target_link_libraries(${PROJECTNAME} ${YARP_LIBRARIES} ctrlLib)

install (TARGETS ${PROJECTNAME} DESTINATION bin)


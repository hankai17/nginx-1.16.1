cmake_minimum_required(VERSION 3.9)
project(nginx)
#set(CMAKE_CXX_STANDARD 11)
message("hi\n")
message(${nginx_SOURCE_DIR})
message(${PROJECT_BINARY_DIR})

#INCLUDE_DIRECTORIES(${nginx_SOURCE_DIR}/inc/)
#INCLUDE_DIRECTORIES(
#        ${nginx_SOURCE_DIR}/iocore
#        ${nginx_SOURCE_DIR}/lib
#        ${nginx_SOURCE_DIR}/mgmt
#        ${nginx_SOURCE_DIR}/rc
#        )
#AUX_SOURCE_DIRECTORY(proxy DIR_SRCS)
#SET(SPIDER
#        ${DIR_SRCS}
#        )
#ADD_EXECUTABLE(${1} ${TRAFFIC_SERVER})
#https://github.com/gnarlymedia/CLionScientificPlotting/blob/master/CMakeLists.txt
#

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -o0")

add_custom_target(nginx COMMAND make -C ${nginx_SOURCE_DIR}
  #CLION_EXE_DIR=${PROJECT_BINARY_DIR} && cp ${nginx_SOURCE_DIR}/proxy/.libs/nginx ./  #cp executing file to dir
        CLION_EXE_DIR=${PROJECT_BINARY_DIR} && cp ${nginx_SOURCE_DIR}/objs/nginx ./  
        )


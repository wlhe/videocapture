cmake_minimum_required(VERSION 2.8)

project(play)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -std=gnu++11 -g")

find_package(OpenCV REQUIRED)

set (sources
	capture.cpp
	videoutils.cpp
)
set (headers
	videoutils.h
)

add_executable(${CMAKE_PROJECT_NAME} 
				${sources}	
				${headers}
)

target_link_libraries(${CMAKE_PROJECT_NAME} ${OpenCV_LIBS})


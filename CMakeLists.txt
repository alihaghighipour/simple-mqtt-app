cmake_minimum_required(VERSION 3.5)
project(Application VERSION 1.0.0)
add_definitions("-std=c++14")

find_package (Threads)
find_package(PahoMqttCpp REQUIRED)

add_executable(Application Application.cpp)
target_link_libraries(Application paho-mqttpp3 paho-mqtt3as)
target_link_libraries (Application Threads::Threads)

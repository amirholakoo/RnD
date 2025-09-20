# The following lines of boilerplate have to be in your project's
# CMakeLists.txt in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Set the project name
project(Sensor-Acquisition)

# Include the ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Add the esp32-dht component
set(EXTRA_COMPONENT_DIRS "managed_components")

# Create the project
project(Sensor-Acquisition)

# SPDX-License-Identifier: Apache-2.0

# Moonlite is a header-only helper library. We still create a Zephyr library
# unit so the include path is propagated when CONFIG_MOONLITE is enabled.
zephyr_library()
zephyr_library_sources(Moonlite.cpp)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

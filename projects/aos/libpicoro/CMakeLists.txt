#
# Copyright 2019, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the GNU General Public License version 2. Note that NO WARRANTY is provided.
# See "LICENSE_GPLv2.txt" for details.
#
# @TAG(DATA61_GPL)
#
cmake_minimum_required(VERSION 3.7.2)

project(libpicoro C)

add_library(picoro EXCLUDE_FROM_ALL src/picoro.c)
target_include_directories(picoro PUBLIC include)
target_link_libraries(picoro Configuration muslc sel4 utils)

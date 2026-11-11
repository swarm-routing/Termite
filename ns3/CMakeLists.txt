# Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
# SPDX-License-Identifier: GPL-2.0-only
#
# The `termite` ns-3 contrib module (#6): Termite's simulator-free core/ (one
# directory up, MIT OR GPL-2.0-only) compiled into ns-3 on swarm-core, with the
# adapter that carries out its decisions (model/), a helper, the harness arm
# (helper/termite-bench-arms.cc, compiled only when routing-bench is installed
# beside it) and the test suite. Install it next to swarm-core:
#
#   ns3-build-contrib --test swarm-core=/src/ns3-swarm-core termite=/src/termite/ns3
#
# ns-3 reaches this directory through contrib/termite, a link to the
# checkout's ns3/: core/ is found beside the link's target, not beside the link.
get_filename_component(termite_ns3_dir "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)  # CMake < 3.19 too
get_filename_component(termite_core_dir "${termite_ns3_dir}/../core" ABSOLUTE)
# core/ includes swarm-core's primitives without the ns3/ prefix (they are the
# same files as ns3/swarm-*.h), so it builds alone as well.
set(termite_swarm_core_dir "${PROJECT_SOURCE_DIR}/contrib/swarm-core/core")
foreach(d "${termite_core_dir}" "${termite_swarm_core_dir}")
  if(NOT EXISTS "${d}")
    message(FATAL_ERROR "termite: ${d} not found (install swarm-core as contrib/swarm-core)")
  endif()
endforeach()
include_directories("${termite_core_dir}" "${termite_swarm_core_dir}")

# The harness arm, only where swarm-routing/ns3-tools' routing-bench is
# installed and enabled. Its library is named through a variable: ns-3 scans
# module CMakeLists for library tokens, and `termite` must also build alone
# (routing-bench README, "Registering an arm").
set(termite_bench_sources)
set(termite_bench_libs)
set(termite_bench_module routing-bench)
if(EXISTS ${PROJECT_SOURCE_DIR}/contrib/${termite_bench_module}/CMakeLists.txt
   AND termite_bench_module IN_LIST ns3-all-enabled-modules)
  set(termite_bench_sources helper/termite-bench-arms.cc)
  set(termite_bench_lib lib${termite_bench_module})
  set(termite_bench_libs ${${termite_bench_lib}})
endif()

build_lib(
  LIBNAME termite
  SOURCE_FILES
    ${termite_core_dir}/termite-packet.cc
    ${termite_core_dir}/termite-router.cc
    model/termite-header.cc
    model/termite-routing-protocol.cc
    helper/termite-helper.cc
    ${termite_bench_sources}
  HEADER_FILES
    model/termite-routing-protocol.h
    helper/termite-helper.h
  LIBRARIES_TO_LINK ${libswarm-core} ${libcore} ${libnetwork} ${libinternet} ${termite_bench_libs}
  TEST_SOURCES
    ${termite_core_dir}/test/all-tests.cc
    ${termite_core_dir}/test/test-packet-codec.cc
    ${termite_core_dir}/test/test-network.cc
    ${termite_core_dir}/test/test-router.cc
    test/termite-test-suite.cc
)

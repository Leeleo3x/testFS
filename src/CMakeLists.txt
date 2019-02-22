include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${SPDK_DIR}/include)

link_directories(${SPDK_DIR}/build/lib)
link_directories(${SPDK_DIR}/dpdk/build/lib)
link_directories(${SPDK_DIR}/isa-l/.libs)

add_library(testFSLib
  bitmap.c
  block.c
  csum.c
  dir.c
  file.c
  inode.c
  mktestfs.c
  super.c
  tx.c
)

add_library(testFSSPDKLib
  bitmap.c
  spdk.c
  csum.c
  dir.c
  file.c
  inode.c
  mktestfs.c
  super.c
  tx.c
)

target_link_libraries(testFSSPDKLib
                      -Wl,--whole-archive
                      ${SPDK_LIBS}
                      dpdk
                      -Wl,--no-whole-archive
                      pthread
                      isal
                      rt
                      uuid
                      aio
                      crypto
                      numa
                      dl
                      )

add_dependencies(testFSSPDKLib LibSPDK)

add_library(testFSSPDK
  spdk.c
)

add_executable(testFSExe
  testfs.c
)

add_executable(testFSSPDKExe
  testfs_spdk.c
)

target_link_libraries(testFSExe testFSLib)
target_link_libraries(testFSSPDKExe testFSSPDKLib)
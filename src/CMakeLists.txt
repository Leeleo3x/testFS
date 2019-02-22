include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${SPDK_DIR}/include)

link_directories(${SPDK_DIR}/build/lib)
link_directories(${SPDK_DIR}/dpdk/build/lib)
link_directories(${SPDK_DIR}/isa-l/.libs)

add_library(testFSLib
  bitmap.c
  block_file.c
  csum.c
  dir.c
  file.c
  device_file.c
  inode.c
  super.c
  tx.c
)
set_target_properties(testFSLib PROPERTIES COMPILE_DEFINITIONS "FILE_DEVICE=1")

add_library(testFSSPDKLib
  bitmap.c
  device_nvme.c
  block_nvme.c
  csum.c
  dir.c
  file.c
  inode.c
  super.c
  tx.c
)
add_dependencies(testFSSPDKLib LibSPDK)
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


add_executable(testFSExe
  testfs.c
)
set_target_properties(testFSExe PROPERTIES COMPILE_DEFINITIONS "FILE_DEVICE=1")
target_link_libraries(testFSExe testFSLib)

add_executable(testFSSPDKExe
  testfs.c
)
target_link_libraries(testFSSPDKExe testFSSPDKLib)
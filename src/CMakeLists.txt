include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${SPDK_DIR}/include)

link_directories(${SPDK_DIR}/build/lib)
link_directories(${SPDK_DIR}/dpdk/build/lib)

set(testFSCommon
  bitmap.c
  csum.c
  dir.c
  file.c
  inode.c
  super.c
  tx.c
)

set(SPDKFlags 
  -Wl,--whole-archive
  ${SPDK_LIBS}
  dpdk
  -Wl,--no-whole-archive
  pthread
  rt
  uuid
  aio
  crypto
  numa
  dl
  isal
  m
)


add_library(testfs-bdev-lib
        device.c
        block.c
  ${testFSCommon}
)
add_dependencies(testfs-bdev-lib LibSPDK)
target_link_libraries(testfs-bdev-lib ${SPDKFlags})


add_executable(testfs-bdev testfs.c)
target_link_libraries(testfs-bdev testfs-bdev-lib)

add_executable(testfs-perf perf_test.c)
target_link_libraries(testfs-perf testfs-bdev-lib)

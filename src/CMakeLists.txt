include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${SPDK_DIR}/include)

link_directories(${SPDK_DIR}/build/lib)
link_directories(${SPDK_DIR}/dpdk/build/lib)
link_directories(${SPDK_DIR}/isa-l/.libs)

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
  isal
  rt
  uuid
  aio
  crypto
  numa
  dl
)

add_library(testfs-file-lib
  block_file.c
  device_file.c
  ${testFSCommon}
)
set_target_properties(testfs-file-lib PROPERTIES COMPILE_DEFINITIONS "FILE_DEVICE=1")

add_library(testfs-nvme-lib
  device_nvme.c
  block_nvme.c
  ${testFSCommon}
)
add_dependencies(testfs-nvme-lib LibSPDK)
target_link_libraries(testfs-nvme-lib ${SPDKFlags})

add_library(testfs-bdev-lib
  device_bdev.c
  block_bdev.c
  ${testFSCommon}
)
add_dependencies(testfs-bdev-lib LibSPDK)
set_target_properties(testfs-bdev-lib PROPERTIES COMPILE_DEFINITIONS "BDEV_DEVICE=1")
target_link_libraries(testfs-bdev-lib ${SPDKFlags})


add_executable(testfs-file testfs.c)
set_target_properties(testfs-file PROPERTIES COMPILE_DEFINITIONS "FILE_DEVICE=1")
target_link_libraries(testfs-file testfs-file-lib)

add_executable(testfs-nvme testfs.c)
target_link_libraries(testfs-nvme testfs-nvme-lib)

add_executable(testfs-bdev testfs.c)
set_target_properties(testfs-bdev PROPERTIES COMPILE_DEFINITIONS "BDEV_DEVICE=1")
target_link_libraries(testfs-bdev testfs-bdev-lib)

include(ExternalProject)

find_package(Git REQUIRED)

ExternalProject_Add(LibSPDK
    GIT_REPOSITORY https://github.com/spdk/spdk
    GIT_TAG df517550cc9c35971008bde4010f20082f4ecbe4
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND ./configure 
    INSTALL_COMMAND ""
    BUILD_IN_SOURCE ON
)

ExternalProject_Get_property(LibSPDK SOURCE_DIR)
set(SPDK_DIR ${SOURCE_DIR})
list(APPEND SPDK_LIBS spdk_bdev_lvol spdk_blobfs spdk_blob spdk_blob_bdev spdk_lvol spdk_bdev_malloc spdk_bdev_null spdk_bdev_nvme spdk_nvme spdk_bdev_passthru spdk_bdev_error spdk_bdev_gpt spdk_bdev_split spdk_bdev_raid spdk_bdev_aio spdk_bdev_virtio spdk_virtio spdk_copy_ioat spdk_ioat spdk_sock_posix spdk_event_bdev spdk_event_copy spdk_bdev spdk_copy spdk_event spdk_thread spdk_util spdk_conf spdk_trace spdk_log spdk_jsonrpc spdk_json spdk_rpc spdk_sock spdk_env_dpdk)
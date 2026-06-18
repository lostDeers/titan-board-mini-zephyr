# SPDX-License-Identifier: Apache-2.0

if(CONFIG_VIDEO_TITAN_RA_MIPI_CSI)
  zephyr_library()
  zephyr_library_include_directories(fsp ${ZEPHYR_BASE}/drivers/video)
  zephyr_library_sources(
    fsp/r_mipi_phy.c
    fsp/r_mipi_csi.c
    fsp/r_vin.c
    video_titan_ra_mipi_csi.c
  )
endif()

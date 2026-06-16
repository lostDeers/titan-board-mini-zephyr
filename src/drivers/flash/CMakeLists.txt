# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_TITAN_W25Q64_RA_OSPI_B flash_titan_w25q64_ra_ospi_b.c)

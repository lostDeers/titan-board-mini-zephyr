# SPDX-License-Identifier: Apache-2.0
# Project-local Zephyr source overrides for this sample.
#
# These overrides are generated into the build directory from the active
# ZEPHYR_BASE sources so the upstream Zephyr checkout remains read-only.

function(titan_uvc_replace_target_source target old_rel old_abs new_src)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "Required Zephyr target ${target} is not available")
  endif()

  get_target_property(_sources ${target} SOURCES)
  if(NOT _sources)
    message(FATAL_ERROR "Target ${target} has no source list to patch")
  endif()

  set(_new_sources)
  set(_removed FALSE)

  foreach(_src IN LISTS _sources)
    if(_src STREQUAL "${old_rel}" OR _src STREQUAL "${old_abs}")
      set(_removed TRUE)
    else()
      list(APPEND _new_sources "${_src}")
    endif()
  endforeach()

  if(NOT _removed)
    message(FATAL_ERROR "Did not find ${old_rel} in ${target}; refusing silent Zephyr override")
  endif()

  set_property(TARGET ${target} PROPERTY SOURCES ${_new_sources})
  target_sources(${target} PRIVATE "${new_src}")
  message(STATUS "Titan UVC sample overrides ${old_rel} with ${new_src}")
endfunction()

function(titan_uvc_apply_zephyr_overrides)
  set(_override_dir "${CMAKE_BINARY_DIR}/titan_zephyr_overrides")
  set(_override_generator "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../scripts/generate_zephyr_overrides.py")
  set(_uvc_source "${ZEPHYR_BASE}/subsys/usb/device_next/class/usbd_uvc.c")
  set(_udc_source "${ZEPHYR_BASE}/drivers/usb/udc/udc_renesas_ra.c")
  set(_uvc_override "${_override_dir}/subsys/usb/device_next/class/usbd_uvc.c")
  set(_udc_override "${_override_dir}/drivers/usb/udc/udc_renesas_ra.c")

  add_custom_command(
    OUTPUT "${_uvc_override}" "${_udc_override}"
    COMMAND "${Python3_EXECUTABLE}"
            "${_override_generator}"
            --zephyr-base "${ZEPHYR_BASE}"
            --out-dir "${_override_dir}"
    DEPENDS "${_override_generator}" "${_uvc_source}" "${_udc_source}"
    COMMENT "Generating Titan UVC Zephyr override sources"
    VERBATIM
  )

  set_source_files_properties("${_uvc_override}" "${_udc_override}" PROPERTIES GENERATED TRUE)
  add_custom_target(titan_zephyr_overrides DEPENDS "${_uvc_override}" "${_udc_override}")
  add_dependencies(subsys__usb__device_next titan_zephyr_overrides)
  add_dependencies(drivers__usb__udc titan_zephyr_overrides)

  target_include_directories(drivers__usb__udc PRIVATE "${ZEPHYR_BASE}/drivers/usb/udc")
  titan_uvc_replace_target_source(
    subsys__usb__device_next
    "class/usbd_uvc.c"
    "${_uvc_source}"
    "${_uvc_override}"
  )

  titan_uvc_replace_target_source(
    drivers__usb__udc
    "udc_renesas_ra.c"
    "${_udc_source}"
    "${_udc_override}"
  )
endfunction()

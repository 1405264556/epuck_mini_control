# Findlibusb-1.0
# Locate libusb headers and library
#
# LIBUSB_FOUND       - True if libusb-1.0 found
# LIBUSB_INCLUDE_DIR - Include directory
# LIBUSB_LIBRARY     - Library file

find_path(LIBUSB_INCLUDE_DIR
    NAMES libusb-1.0/libusb.h
    PATHS "$ENV{VCPKG_ROOT}/installed/x64-windows/include"
          "$ENV{VCPKG_ROOT}/packages/libusb_x64-windows/include"
          /usr/include/libusb-1.0
          /usr/local/include/libusb-1.0
)

find_library(LIBUSB_LIBRARY
    NAMES usb-1.0 libusb-1.0
    PATHS "$ENV{VCPKG_ROOT}/installed/x64-windows/lib"
          "$ENV{VCPKG_ROOT}/packages/libusb_x64-windows/lib"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libusb-1.0
    REQUIRED_VARS LIBUSB_LIBRARY LIBUSB_INCLUDE_DIR
)

if(libusb-1.0_FOUND AND NOT TARGET libusb-1.0)
    add_library(libusb-1.0 UNKNOWN IMPORTED)
    set_target_properties(libusb-1.0 PROPERTIES
        IMPORTED_LOCATION "${LIBUSB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBUSB_INCLUDE_DIR}"
    )
endif()

# Find SpiderMonkey 24

find_library(SPIDERMONKEY_LIBRARY NAMES mozjs-24)
find_path(SPIDERMONKEY_INCLUDE_DIR NAMES jsapi.h PATH_SUFFIXES mozjs-24 js)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SpiderMonkey DEFAULT_MSG
    SPIDERMONKEY_LIBRARY
    SPIDERMONKEY_INCLUDE_DIR)

if(SPIDERMONKEY_FOUND)
  set(SPIDERMONKEY_LIBRARIES ${SPIDERMONKEY_LIBRARY})
  set(SPIDERMONKEY_INCLUDE_DIRS ${SPIDERMONKEY_INCLUDE_DIR})
endif()

# vim:et sw=4 ts=4

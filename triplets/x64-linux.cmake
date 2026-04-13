set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

string(APPEND VCPKG_C_FLAGS " -Wno-error=unterminated-string-initialization")
string(APPEND VCPKG_CXX_FLAGS " -Wno-error=unterminated-string-initialization")

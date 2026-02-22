if(EXISTS "${SRC}")
    cmake_path(GET SRC FILENAME FILENAME)
    file(COPY_FILE "${SRC}" "${DST}/${FILENAME}" ONLY_IF_DIFFERENT)
endif()

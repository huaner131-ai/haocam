# Embeds a source file (e.g. HLSL shader) into the build as a C++ header.
# The generated header lives in ${CMAKE_BINARY_DIR}/generated/ and exposes:
#   haocam::shaders::k_<name>_<ext>[] / k_<name>_<ext>_size
# The actual byte packing is done by EmbedFile.cmake in script mode.

function(haocam_embed_shader shader_path)
    get_filename_component(name_we "${shader_path}" NAME_WE)
    get_filename_component(ext "${shader_path}" EXT)
    string(REPLACE "." "_" array_name "k_${name_we}${ext}")

    set(gen_dir "${CMAKE_BINARY_DIR}/generated/shaders")
    set(gen_header "${gen_dir}/${name_we}${ext}.h")

    add_custom_command(
        OUTPUT "${gen_header}"
        COMMAND ${CMAKE_COMMAND}
            -DINPUT="${shader_path}"
            -DOUTPUT="${gen_header}"
            -DARRAY_NAME="${array_name}"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/EmbedFile.cmake"
        DEPENDS "${shader_path}"
        COMMENT "Embedding ${name_we}${ext}"
        VERBATIM
    )

    target_sources(haocam_core PRIVATE "${gen_header}")
    set_source_files_properties("${gen_header}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
    target_include_directories(haocam_core PRIVATE "${gen_dir}")
endfunction()

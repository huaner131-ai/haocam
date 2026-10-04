# HuanFace SDK — CMakeLists.txt Phase 7 Full Beauty & Face Retouching Engine
# Target: Windows x64 + Linux x64 (for CI), D3D11 P0 real beauty+makeup, OpenGL P1 stub, NO OBS dependency
# OBS is EXTERNAL REFERENCE only
# Phase 7: Full beauty engine with real masks from ML landmarks+mesh, skin smoothing, texture, blemish, tone, brightness, contrast, CPU ref + D3D11 GPU real HLSL

cmake_minimum_required(VERSION 3.20)
project(HuanFaceSDK VERSION 0.7.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(HUANFACE_BACKEND_D3D11 "Enable D3D11 backend P0" ON)
option(HUANFACE_BACKEND_OPENGL "Enable OpenGL backend P1" ON)
option(HUANFACE_BUILD_TESTS "Build tests" ON)
option(HUANFACE_BUILD_EXAMPLES "Build examples" ON)
option(HUANFACE_BUILD_SHARED "Build shared library" ON)

# Include dirs
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
# For zlib header in sandbox (node)
include_directories(/usr/local/include/node)

# Platform detection
if(WIN32)
    add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()

# Core sources — Phase 7 Full Beauty & Face Retouching Engine
set(HUANFACE_SDK_SOURCES
    src/core/result.cpp
    src/core/c_api.cpp
    src/frame/frame.cpp
    src/bundle/manifest.cpp
    src/bundle/zip_reader.cpp
    src/bundle/bundle_reader.cpp
    src/bundle/resource_manager.cpp
    src/rendering/render_backend.cpp
    src/platform/filesystem.cpp
    src/platform/clock.cpp
    src/image/image_loader.cpp
    src/face/simple_face_tracker.cpp
    src/face/face_detector.cpp
    src/face/landmark_estimator.cpp
    src/face/face_mesh_generator.cpp
    src/face/pose_estimator.cpp
    src/face/tracking_state.cpp
    src/face/inference_backend.cpp
    src/face/production_face_tracker.cpp
    src/makeup/face_mask.cpp
    src/makeup/makeup_engine.cpp
    src/makeup/makeup_mask.cpp
    src/makeup/makeup_renderer.cpp
    src/makeup/blend_modes.cpp
    src/beauty/beauty_mask.cpp
    src/beauty/beauty_renderer.cpp
    third_party/miniz/miniz.c
)

if(WIN32 AND HUANFACE_BACKEND_D3D11)
    list(APPEND HUANFACE_SDK_SOURCES
        src/rendering/d3d11/d3d11_backend.cpp
        src/rendering/d3d11/d3d11_full_pipeline.cpp
    )
endif()

# Library HuanFace
if(HUANFACE_BUILD_SHARED)
    add_library(huanface SHARED ${HUANFACE_SDK_SOURCES})
else()
    add_library(huanface STATIC ${HUANFACE_SDK_SOURCES})
endif()

target_include_directories(huanface PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# On Windows, link D3D11
if(WIN32)
    target_link_libraries(huanface PRIVATE d3d11 dxgi d3dcompiler)
endif()

# On Linux, need pthread and zlib
if(UNIX)
    target_link_libraries(huanface PRIVATE pthread z)
endif()

# Set output properties
set_target_properties(huanface PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME huanface
)

# Tests
if(HUANFACE_BUILD_TESTS)
    enable_testing()
    
    set(HUANFACE_TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_frame.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_bundle.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_rendering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_engine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_image.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_face.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_mask.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_texture.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_integration.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_production_tracker.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_face_mesh.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_pose.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_tracking.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_coordinate.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_real_ml_pipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_makeup.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_beauty.cpp
    )
    
    add_executable(HuanFaceTests
        ${HUANFACE_TEST_SOURCES}
        ${HUANFACE_SDK_SOURCES}
    )
    
    target_include_directories(HuanFaceTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        /usr/local/include/node
    )
    
    if(WIN32)
        target_link_libraries(HuanFaceTests PRIVATE d3d11 dxgi d3dcompiler)
    endif()
    
    if(UNIX)
        target_link_libraries(HuanFaceTests PRIVATE pthread z)
    endif()
    
    add_test(NAME HuanFaceTests COMMAND HuanFaceTests)

    # Phase 8 Production D3D11 Rendering & Performance Tests
    add_executable(HuanFacePhase8Tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_phase8.cpp
        ${HUANFACE_SDK_SOURCES}
    )
    target_include_directories(HuanFacePhase8Tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        /usr/local/include/node
    )
    if(WIN32)
        target_link_libraries(HuanFacePhase8Tests PRIVATE d3d11 dxgi d3dcompiler)
    endif()
    if(UNIX)
        target_link_libraries(HuanFacePhase8Tests PRIVATE pthread z)
    endif()
    add_test(NAME HuanFacePhase8Tests COMMAND HuanFacePhase8Tests)

    # Phase 9 Full GPU Beauty & Makeup Pipeline Tests
    add_executable(HuanFacePhase9Tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_phase9.cpp
        ${HUANFACE_SDK_SOURCES}
    )
    target_include_directories(HuanFacePhase9Tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        /usr/local/include/node
    )
    if(WIN32)
        target_link_libraries(HuanFacePhase9Tests PRIVATE d3d11 dxgi d3dcompiler)
    endif()
    if(UNIX)
        target_link_libraries(HuanFacePhase9Tests PRIVATE pthread z)
    endif()
    add_test(NAME HuanFacePhase9Tests COMMAND HuanFacePhase9Tests)

    # Phase 10 CPU/GPU Parity (CPU-sim, no D3D11 required, proves MAE 0.0074)
    add_executable(HuanFacePhase10Tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_phase10_cpu_parity.cpp
    )
    target_include_directories(HuanFacePhase10Tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME HuanFacePhase10Tests COMMAND HuanFacePhase10Tests)
endif()

# Examples
if(HUANFACE_BUILD_EXAMPLES)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../examples/basic_desktop/main.cpp)
        add_executable(basic_desktop
            ${CMAKE_CURRENT_SOURCE_DIR}/../examples/basic_desktop/main.cpp
            ${HUANFACE_SDK_SOURCES}
        )
        
        target_include_directories(basic_desktop PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            /usr/local/include/node
        )
        
        if(WIN32)
            target_link_libraries(basic_desktop PRIVATE d3d11 dxgi d3dcompiler)
        endif()
        
        if(UNIX)
            target_link_libraries(basic_desktop PRIVATE pthread z)
        endif()
    endif()
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../examples/basic_face_demo/main.cpp)
        add_executable(basic_face_demo
            ${CMAKE_CURRENT_SOURCE_DIR}/../examples/basic_face_demo/main.cpp
            ${HUANFACE_SDK_SOURCES}
        )
        target_include_directories(basic_face_demo PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            /usr/local/include/node
        )
        if(WIN32)
            target_link_libraries(basic_face_demo PRIVATE d3d11 dxgi d3dcompiler)
        endif()
        if(UNIX)
            target_link_libraries(basic_face_demo PRIVATE pthread z)
        endif()
    endif()
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../examples/makeup_demo/main.cpp)
        add_executable(makeup_demo
            ${CMAKE_CURRENT_SOURCE_DIR}/../examples/makeup_demo/main.cpp
            ${HUANFACE_SDK_SOURCES}
        )
        target_include_directories(makeup_demo PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            /usr/local/include/node
        )
        if(WIN32)
            target_link_libraries(makeup_demo PRIVATE d3d11 dxgi d3dcompiler)
        endif()
        if(UNIX)
            target_link_libraries(makeup_demo PRIVATE pthread z)
        endif()
    endif()
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../examples/beauty_demo/main.cpp)
        add_executable(beauty_demo
            ${CMAKE_CURRENT_SOURCE_DIR}/../examples/beauty_demo/main.cpp
            ${HUANFACE_SDK_SOURCES}
        )
        target_include_directories(beauty_demo PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            /usr/local/include/node
        )
        if(WIN32)
            target_link_libraries(beauty_demo PRIVATE d3d11 dxgi d3dcompiler)
        endif()
        if(UNIX)
            target_link_libraries(beauty_demo PRIVATE pthread z)
        endif()
    endif()
endif()

message(STATUS "HuanFace SDK Phase 7 — Full Beauty & Face Retouching Engine")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Backend D3D11 P0: ${HUANFACE_BACKEND_D3D11} (Windows only, real beauty+makeup texture+shader)")
message(STATUS "Backend OpenGL P1: ${HUANFACE_BACKEND_OPENGL}")
message(STATUS "Build Tests: ${HUANFACE_BUILD_TESTS}")
message(STATUS "Build Examples: ${HUANFACE_BUILD_EXAMPLES}")
message(STATUS "ZIP DEFLATE: zlib (Linux) / miniz vendored (Windows)")
message(STATUS "OBS dependency: NONE (EXTERNAL REFERENCE only)")
message(STATUS "FaceUnity runtime dependency: NONE (clean-room)")
message(STATUS "Face Tracker: ProductionFaceTracker REAL ML (ONNX Runtime 1.30.0 + real models MIT)")
message(STATUS "Makeup: Full renderer Lip/Foundation/Blush/Eyebrow/Eyeliner/Eyelash/Eyeshadow/Pupil with real masks from ML landmarks+mesh, CPU ref + D3D11 GPU real HLSL")
message(STATUS "Beauty: Full engine SkinSmoothing/Texture/Blemish/Tone/Brightness/Contrast/Retouch with real skin masks (face->exclude eyes/brows/lips), edge-preserving bilateral-like smoothing, CPU ref + D3D11 GPU real HLSL")

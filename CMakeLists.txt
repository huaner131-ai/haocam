cmake_minimum_required(VERSION 3.21)

project(HaoCam
    VERSION 0.2.0
    DESCRIPTION "HaoCam - a modern Windows beauty camera"
    LANGUAGES C CXX) # C: miniz.c in the optional HuanFace SDK drop

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HAOCAM_BUILD_APP        "Build the Qt/QML desktop application"            ON)
option(HAOCAM_BUILD_TESTS      "Build unit tests"                                ON)
option(HAOCAM_ENABLE_FACEBETTER     "Enable the Facebetter beauty provider"      OFF)
option(HAOCAM_ENABLE_MEDIAPIPE      "Enable the MediaPipe face tracker"          OFF)
option(HAOCAM_ENABLE_HUANFACE       "Enable the HuanFace beauty provider"        OFF)
option(HAOCAM_ENABLE_OPENMAKEUP     "Enable the OpenMakeup provider (web)"       OFF)
option(HAOCAM_ENABLE_SNAP           "Enable the Snap Camera Kit AR provider"     OFF)
option(HAOCAM_ENABLE_FFMPEG         "Enable the FFmpeg recording backend"        OFF)
option(HAOCAM_ENABLE_VIRTUAL_CAMERA "Enable the virtual camera output"           OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8 /MP)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(EmbedShader)

# ---------------------------------------------------------------------------
# Optional SDK drop-in detection (spec section 37: an enabled-but-missing SDK
# falls back to the null providers with a clear warning - the core build must
# never break. See sdk/README.md for the expected layouts.)
# ---------------------------------------------------------------------------
if(HAOCAM_ENABLE_FACEBETTER)
    set(HAOCAM_FACEBETTER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdk/facebetter")
    if(WIN32 AND
       EXISTS "${HAOCAM_FACEBETTER_DIR}/include/facebetter/beauty_effect_engine.h" AND
       EXISTS "${HAOCAM_FACEBETTER_DIR}/lib/facebetter.lib")
        set(HAOCAM_HAS_FACEBETTER ON)
        message(STATUS "Facebetter SDK found - FacebetterProvider enabled")
    else()
        message(WARNING
            "HAOCAM_ENABLE_FACEBETTER=ON but the SDK was not found under "
            "sdk/facebetter (expected include/facebetter/*.h + lib/facebetter.lib). "
            "Falling back to NullBeautyProvider (Beauty reports Unavailable).")
    endif()
endif()

if(HAOCAM_ENABLE_MEDIAPIPE)
    set(HAOCAM_MEDIAPIPE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdk/mediapipe")
    if(EXISTS "${HAOCAM_MEDIAPIPE_DIR}/include/mediapipe/tasks/cc/vision/face_landmarker/face_landmarker.h")
        set(HAOCAM_HAS_MEDIAPIPE ON)
        message(STATUS "MediaPipe SDK found - MediaPipeFaceTracker enabled")
    else()
        message(WARNING
            "HAOCAM_ENABLE_MEDIAPIPE=ON but the SDK was not found under "
            "sdk/mediapipe. Falling back to NullFaceTracker (Tracking reports "
            "Unavailable).")
    endif()
endif()

# ---------------------------------------------------------------------------
# haocam_core: platform-neutral engine library (no Qt dependency).
# Windows-specific sources (Media Foundation, D3D11) are only compiled on WIN32.
# ---------------------------------------------------------------------------
set(HAOCAM_CORE_SOURCES
    src/core/logging/Logger.cpp
    src/core/config/Json.cpp
    src/core/config/AppConfig.cpp
    src/core/settings/AppSettings.cpp
    src/core/events/EventBus.cpp
    src/frame/FramePool.cpp
    src/face/FaceLandmarks.cpp
    src/face/FaceTracking.cpp
    src/face/TrackingWorker.cpp
    src/effects/EffectGraph.cpp
    src/effects/EffectManager.cpp
    src/effects/beauty/BeautyConfig.cpp
    src/effects/beauty/NullBeautyProvider.cpp
    src/camera/CameraFormat.cpp
    src/camera/CameraManager.cpp
    src/camera/NullCameraSource.cpp
)

set(HAOCAM_CORE_HEADERS
    src/core/logging/Logger.h
    src/core/config/Json.h
    src/core/config/AppConfig.h
    src/core/settings/AppSettings.h
    src/core/threading/FrameQueue.h
    src/core/threading/NamedThread.h
    src/core/events/EventBus.h
    src/core/events/Events.h
    src/frame/FrameMetadata.h
    src/frame/Frame.h
    src/frame/FrameBuffer.h
    src/frame/FramePool.h
    src/frame/GPUTexture.h
    src/frame/TexturePool.h
    src/face/FaceData.h
    src/face/FaceLandmarks.h
    src/face/FaceTracker.h
    src/face/LandmarkSmoother.h
    src/face/MediaPipeFaceTracker.h
    src/face/TrackingWorker.h
    src/effects/EffectProvider.h
    src/effects/EffectContext.h
    src/effects/EffectParameter.h
    src/effects/EffectNode.h
    src/effects/EffectGraph.h
    src/effects/EffectManager.h
    src/effects/beauty/BeautyProvider.h
    src/effects/beauty/NullBeautyProvider.h
    src/camera/CameraFormat.h
    src/camera/CameraDevice.h
    src/camera/ICameraSource.h
    src/camera/CameraManager.h
    src/camera/NullCameraSource.h
    src/graphics/RenderTypes.h
    src/graphics/IRenderDevice.h
    src/graphics/compositor/Compositor.h
    src/graphics/compositor/RenderGraph.h
)

if(WIN32)
    list(APPEND HAOCAM_CORE_SOURCES
        src/camera/MediaFoundationCapture.cpp
        src/face/MediaPipePixelSource.cpp
        src/graphics/D3D11/D3D11Context.cpp
        src/graphics/D3D11/D3D11Texture.cpp
        src/graphics/D3D11/D3D11Shader.cpp
        src/graphics/D3D11/D3D11TexturePool.cpp
        src/graphics/D3D11/GpuFrameCopier.cpp
        src/graphics/compositor/Compositor.cpp
        src/graphics/compositor/RenderGraph.cpp
    )
    list(APPEND HAOCAM_CORE_HEADERS
        src/camera/MediaFoundationCapture.h
        src/face/MediaPipePixelSource.h
        src/graphics/D3D11/D3D11Context.h
        src/graphics/D3D11/D3D11Texture.h
        src/graphics/D3D11/D3D11Shader.h
        src/graphics/D3D11/D3D11TexturePool.h
        src/graphics/D3D11/GpuFrameCopier.h
        src/graphics/D3D11/D3D11MultithreadCompat.h
    )
    if(HAOCAM_HAS_MEDIAPIPE)
        list(APPEND HAOCAM_CORE_SOURCES src/face/MediaPipeFaceTracker.cpp)
    endif()
    if(HAOCAM_HAS_FACEBETTER)
        list(APPEND HAOCAM_CORE_SOURCES src/effects/beauty/FacebetterProvider.cpp)
    endif()
endif()

add_library(haocam_core STATIC ${HAOCAM_CORE_SOURCES} ${HAOCAM_CORE_HEADERS})
target_include_directories(haocam_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(haocam_core PRIVATE HAOCAM_CORE_LIBRARY)

# ---- HuanFace SDK (optional; user's own SDK, drop-in under sdk/huanface/sdk) ----
set(HAOCAM_HUANFACE_DROP "${CMAKE_CURRENT_SOURCE_DIR}/sdk/huanface/sdk")
if(HAOCAM_ENABLE_HUANFACE AND NOT EXISTS ${HAOCAM_HUANFACE_DROP}/include/huanface_c_api.h)
    message(FATAL_ERROR "HAOCAM_ENABLE_HUANFACE=ON but the SDK drop is missing: "
        "expected ${HAOCAM_HUANFACE_DROP}/include/huanface_c_api.h (see sdk/huanface/README.md)")
endif()
if(HAOCAM_ENABLE_HUANFACE)
    set(HUANFACE_SDK_SOURCES
        ${HAOCAM_HUANFACE_DROP}/src/core/result.cpp
        ${HAOCAM_HUANFACE_DROP}/src/core/c_api.cpp
        ${HAOCAM_HUANFACE_DROP}/src/frame/frame.cpp
        ${HAOCAM_HUANFACE_DROP}/src/bundle/manifest.cpp
        ${HAOCAM_HUANFACE_DROP}/src/bundle/zip_reader.cpp
        ${HAOCAM_HUANFACE_DROP}/src/bundle/bundle_reader.cpp
        ${HAOCAM_HUANFACE_DROP}/src/bundle/resource_manager.cpp
        ${HAOCAM_HUANFACE_DROP}/src/rendering/render_backend.cpp
        ${HAOCAM_HUANFACE_DROP}/src/platform/filesystem.cpp
        ${HAOCAM_HUANFACE_DROP}/src/platform/clock.cpp
        ${HAOCAM_HUANFACE_DROP}/src/image/image_loader.cpp
        ${HAOCAM_HUANFACE_DROP}/src/face/simple_face_tracker.cpp
        ${HAOCAM_HUANFACE_DROP}/src/face/face_detector.cpp
        ${HAOCAM_HUANFACE_DROP}/src/face/landmark_estimator.cpp
        ${HAOCAM_HUANFACE_DROP}/src/face/face_mesh_generator.cpp
        ${HAOCAM_HUANFACE_DROP}/src/face/pose_estimator.cpp
        ${HAOCAM_HUANFACE_DROP}/src/face/tracking_state.cpp
        ${HAOCAM_HUANFACE_DROP}/src/face/inference_backend.cpp
        ${HAOCAM_HUANFACE_DROP}/src/face/production_face_tracker.cpp
        ${HAOCAM_HUANFACE_DROP}/src/makeup/face_mask.cpp
        ${HAOCAM_HUANFACE_DROP}/src/makeup/makeup_engine.cpp
        ${HAOCAM_HUANFACE_DROP}/src/makeup/makeup_mask.cpp
        ${HAOCAM_HUANFACE_DROP}/src/makeup/makeup_renderer.cpp
        ${HAOCAM_HUANFACE_DROP}/src/makeup/blend_modes.cpp
        ${HAOCAM_HUANFACE_DROP}/src/beauty/beauty_mask.cpp
        ${HAOCAM_HUANFACE_DROP}/src/beauty/beauty_renderer.cpp
        ${HAOCAM_HUANFACE_DROP}/src/beauty/beauty_effect.cpp
        ${HAOCAM_HUANFACE_DROP}/src/beauty/skin_filter.cpp
        ${HAOCAM_HUANFACE_DROP}/src/beauty/beauty_pipeline.cpp
        ${HAOCAM_HUANFACE_DROP}/third_party/miniz/miniz.c
    )
    add_library(huanface_sdk STATIC ${HUANFACE_SDK_SOURCES})
    target_include_directories(huanface_sdk PUBLIC
        ${HAOCAM_HUANFACE_DROP}/include
        ${HAOCAM_HUANFACE_DROP}/src)
    # miniz: use the vendored tinfl (no system zlib dependency).
    target_compile_definitions(huanface_sdk PUBLIC HF_USE_ZLIB_FOR_MINIZ=0 NOMINMAX WIN32_LEAN_AND_MEAN)
    if(WIN32)
        target_link_libraries(huanface_sdk PUBLIC d3d11 dxgi d3dcompiler)
    endif()
    target_link_libraries(haocam_core PUBLIC huanface_sdk)
    target_compile_definitions(haocam_core PUBLIC HAOCAM_HAS_HUANFACE)
    target_sources(haocam_core PRIVATE src/effects/beauty/huanface/HuanFaceProvider.cpp)
    message(STATUS "HuanFace SDK found - beauty provider enabled (HAOCAM_HAS_HUANFACE)")
endif()

if(WIN32)
    # Embed HLSL shaders as C headers so the binary has no runtime file
    # dependency for its built-in passes. (Must run after the target exists.)
    haocam_embed_shader(${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders/fullscreen_vs.hlsl)
    haocam_embed_shader(${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders/color_ps.hlsl)
    haocam_embed_shader(${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders/composite_ps.hlsl)
    haocam_embed_shader(${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders/tracker_ps.hlsl)

    target_compile_definitions(haocam_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_link_libraries(haocam_core PUBLIC mfplat mf mfuuid mfreadwrite shlwapi d3d11 dxgi d3dcompiler)
endif()

if(HAOCAM_HAS_FACEBETTER)
    target_compile_definitions(haocam_core PUBLIC HAOCAM_HAS_FACEBETTER)
    target_include_directories(haocam_core PUBLIC "${HAOCAM_FACEBETTER_DIR}/include")
    target_link_libraries(haocam_core PUBLIC "${HAOCAM_FACEBETTER_DIR}/lib/facebetter.lib")
    # Ship the runtime DLL beside the app executable when present.
    if(EXISTS "${HAOCAM_FACEBETTER_DIR}/lib/facebetter.dll")
        if(HAOCAM_BUILD_APP)
            set(HAOCAM_FB_DLL_DEST "$<TARGET_FILE_DIR:haocam>")
        else()
            set(HAOCAM_FB_DLL_DEST "$<TARGET_FILE_DIR:haocam_core>")
        endif()
        add_custom_command(TARGET haocam_core POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${HAOCAM_FACEBETTER_DIR}/lib/facebetter.dll"
                "${HAOCAM_FB_DLL_DEST}/facebetter.dll")
    endif()
endif()

if(HAOCAM_HAS_MEDIAPIPE)
    target_compile_definitions(haocam_core PUBLIC HAOCAM_HAS_MEDIAPIPE)
    target_include_directories(haocam_core PUBLIC "${HAOCAM_MEDIAPIPE_DIR}/include")
    file(GLOB HAOCAM_MEDIAPIPE_LIBS "${HAOCAM_MEDIAPIPE_DIR}/lib/*.lib")
    if(HAOCAM_MEDIAPIPE_LIBS)
        target_link_libraries(haocam_core PUBLIC ${HAOCAM_MEDIAPIPE_LIBS})
    endif()
endif()

if(HAOCAM_ENABLE_OPENMAKEUP)
    message(STATUS "HAOCAM_ENABLE_OPENMAKEUP=ON (Phase 3 - provider not yet implemented)")
endif()
if(HAOCAM_ENABLE_SNAP)
    message(STATUS "HAOCAM_ENABLE_SNAP=ON (Phase 4 - provider not yet implemented)")
endif()
if(HAOCAM_ENABLE_FFMPEG)
    message(STATUS "HAOCAM_ENABLE_FFMPEG=ON (Phase 5 - not yet implemented)")
endif()
if(HAOCAM_ENABLE_VIRTUAL_CAMERA)
    message(STATUS "HAOCAM_ENABLE_VIRTUAL_CAMERA=ON (Phase 6 - not yet implemented)")
endif()

# ---------------------------------------------------------------------------
# Qt / QML application
# ---------------------------------------------------------------------------
if(HAOCAM_BUILD_APP)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Gui Qml Quick QuickControls2)
    # Qt >= 6.10 moved private-module targets (Qt6::GuiPrivate, used by
    # VideoView for <rhi/qrhi.h>) into their own find_package component.
    # Older Qt versions define the target directly - handle both.
    if(NOT TARGET Qt6::GuiPrivate)
        find_package(Qt6 QUIET COMPONENTS GuiPrivate)
    endif()

    qt_standard_project_setup()
    # Pin the QML module layout to the modern ":/qt/qml/" resource prefix -
    # what loadFromModule() resolves in Qt 6.11 (silences QTP0001/QTP0004).
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)

    # NOTE: source paths must be ABSOLUTE - Qt 6.11's qt_add_executable
    # resolves relative paths against the build directory.
    set(HAOCAM_APP_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/app/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/app/App.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/app/App.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/controllers/EngineController.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/controllers/EngineController.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/controllers/CameraController.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/controllers/CameraController.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/controllers/BeautyController.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/controllers/BeautyController.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/controllers/FilterController.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/controllers/FilterController.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/controllers/DiagnosticsController.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/controllers/DiagnosticsController.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/controllers/VideoView.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/controllers/VideoView.cpp"
    )

    qt_add_executable(haocam ${HAOCAM_APP_SOURCES})

    # Embed the checked-out commit into the binary so every log proves which
    # build produced it. Refreshed on EVERY build (a configure-time stamp went
    # stale once and mislabeled a user-side log).
    add_custom_target(haocam_build_stamp ALL
        BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/haocam_build_rev.h"
        COMMAND ${CMAKE_COMMAND}
            -DREV_OUT=${CMAKE_CURRENT_BINARY_DIR}/haocam_build_rev.h
            -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteBuildRev.cmake)
    add_dependencies(haocam haocam_build_stamp)
    target_include_directories(haocam PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

    target_link_libraries(haocam PRIVATE
        haocam_core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
    )
    if(TARGET Qt6::GuiPrivate)
        target_link_libraries(haocam PRIVATE Qt6::GuiPrivate)
    else()
        target_link_libraries(haocam PRIVATE Qt6::Gui)
    endif()

    qt_add_qml_module(haocam
        URI HaoCam
        VERSION 1.0
        # NOTE: QML_FILES must stay RELATIVE - absolute paths make Qt's
        # resource system demand QT_RESOURCE_ALIAS on every file (Qt 6.11).
        QML_FILES
            ui/qml/Main.qml
            ui/qml/CameraView.qml
            ui/qml/CameraPanel.qml
            ui/qml/BeautyPanel.qml
            ui/qml/MakeupPanel.qml
            ui/qml/ARPanel.qml
            ui/qml/FilterPanel.qml
            ui/qml/RecordingPanel.qml
            ui/qml/SettingsPanel.qml
        NO_PLUGIN
    )

    set_target_properties(haocam PROPERTIES
        MACOSX_BUNDLE FALSE
        WIN32_EXECUTABLE TRUE
    )
endif()

# ---------------------------------------------------------------------------
# Tests (platform-neutral core only)
# ---------------------------------------------------------------------------
if(HAOCAM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

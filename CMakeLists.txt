cmake_minimum_required(VERSION 3.21)

project(HaoCam
    VERSION 0.1.0
    DESCRIPTION "HaoCam - a modern Windows beauty camera"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HAOCAM_BUILD_APP        "Build the Qt/QML desktop application"            ON)
option(HAOCAM_BUILD_TESTS      "Build unit tests"                                ON)
option(HAOCAM_ENABLE_FACEBETTER     "Enable the Facebetter beauty provider"      OFF)
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
# haocam_core: platform-neutral engine library (no Qt dependency).
# Windows-specific sources (Media Foundation, D3D11) are only compiled on WIN32.
# ---------------------------------------------------------------------------
set(HAOCAM_CORE_SOURCES
    src/core/logging/Logger.cpp
    src/core/config/Json.cpp
    src/core/settings/AppSettings.cpp
    src/core/events/EventBus.cpp
    src/frame/FramePool.cpp
    src/face/FaceTracking.cpp
    src/effects/EffectGraph.cpp
    src/effects/EffectManager.cpp
    src/camera/CameraFormat.cpp
    src/camera/CameraManager.cpp
    src/camera/NullCameraSource.cpp
)

set(HAOCAM_CORE_HEADERS
    src/core/logging/Logger.h
    src/core/config/Json.h
    src/core/settings/AppSettings.h
    src/core/threading/FrameQueue.h
    src/core/threading/NamedThread.h
    src/core/events/EventBus.h
    src/frame/FrameMetadata.h
    src/frame/Frame.h
    src/frame/FrameBuffer.h
    src/frame/FramePool.h
    src/frame/GPUTexture.h
    src/frame/TexturePool.h
    src/face/FaceData.h
    src/face/FaceLandmarks.h
    src/face/FaceTracker.h
    src/effects/EffectProvider.h
    src/effects/EffectContext.h
    src/effects/EffectParameter.h
    src/effects/EffectNode.h
    src/effects/EffectGraph.h
    src/effects/EffectManager.h
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
        src/graphics/D3D11/D3D11Context.cpp
        src/graphics/D3D11/D3D11Texture.cpp
        src/graphics/D3D11/D3D11Shader.cpp
        src/graphics/D3D11/D3D11TexturePool.cpp
        src/graphics/compositor/Compositor.cpp
        src/graphics/compositor/RenderGraph.cpp
    )
    list(APPEND HAOCAM_CORE_HEADERS
        src/camera/MediaFoundationCapture.h
        src/graphics/D3D11/D3D11Context.h
        src/graphics/D3D11/D3D11Texture.h
        src/graphics/D3D11/D3D11Shader.h
        src/graphics/D3D11/D3D11TexturePool.h
    )
endif()

add_library(haocam_core STATIC ${HAOCAM_CORE_SOURCES} ${HAOCAM_CORE_HEADERS})
target_include_directories(haocam_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(haocam_core PRIVATE HAOCAM_CORE_LIBRARY)

if(WIN32)
    # Embed HLSL shaders as C headers so the binary has no runtime file
    # dependency for its built-in passes. (Must run after the target exists.)
    haocam_embed_shader(${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders/fullscreen_vs.hlsl)
    haocam_embed_shader(${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders/color_ps.hlsl)
    haocam_embed_shader(${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders/composite_ps.hlsl)

    target_compile_definitions(haocam_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_link_libraries(haocam_core PUBLIC mfplat mfuuid mfreadwrite shlwapi d3d11 dxgi d3dcompiler)
endif()

if(HAOCAM_ENABLE_FACEBETTER)
    message(STATUS "HAOCAM_ENABLE_FACEBETTER=ON (Phase 2 - provider not yet implemented)")
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

    qt_standard_project_setup()

    qt_add_executable(haocam
        app/main.cpp
        app/App.h
        app/App.cpp
        app/MainWindow.h
        app/MainWindow.cpp
        ui/controllers/EngineController.h
        ui/controllers/EngineController.cpp
        ui/controllers/CameraController.h
        ui/controllers/CameraController.cpp
        ui/controllers/FilterController.h
        ui/controllers/FilterController.cpp
        ui/controllers/DiagnosticsController.h
        ui/controllers/DiagnosticsController.cpp
        ui/controllers/VideoView.h
        ui/controllers/VideoView.cpp
    )

    target_link_libraries(haocam PRIVATE
        haocam_core
        Qt6::GuiPrivate   # rhi/qrhi.h (QRhiTexture::createFrom import) - used by VideoView
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
    )

    qt_add_qml_module(haocam
        URI HaoCam
        VERSION 1.0
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

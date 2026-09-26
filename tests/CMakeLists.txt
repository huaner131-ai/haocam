# HaoCam unit tests (platform-neutral core).

add_executable(haocam_tests
    test_main.h
    core/test_main.cpp
    core/test_Json.cpp
    core/test_FrameQueue.cpp
    core/test_FrameBufferPool.cpp
    core/test_CameraFormat.cpp
    core/test_AppSettings.cpp
    core/test_EventBus.cpp
    core/test_EffectGraph.cpp
    FaceDataTests.cpp
    LandmarkSmootherTests.cpp
    FaceTrackingTests.cpp
    BeautyProviderTests.cpp
    AppConfigTests.cpp
)

target_link_libraries(haocam_tests PRIVATE haocam_core)
target_include_directories(haocam_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# The preset sample shipped in assets/presets is parsed by test_Json.
target_compile_definitions(haocam_tests PRIVATE
    HAOCAM_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
)

add_test(NAME haocam_tests COMMAND haocam_tests)

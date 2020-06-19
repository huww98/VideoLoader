include(GoogleTest)
add_executable(videoloader_tests
    video_tests.cpp
    tar_iterator_tests.cpp
)
target_link_libraries(videoloader_tests videoloader GTest::GTest GTest::Main)
gtest_discover_tests(videoloader_tests
                     WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/../..")

if(WITH_PYTHON)
    target_link_libraries(videoloader_tests Python::Python)
endif()

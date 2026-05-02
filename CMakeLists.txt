cmake_minimum_required(VERSION 3.19)
project(LanDrop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Widgets)

qt_standard_project_setup()

set(CORE_SOURCES
    src/core/protocol_constants.h
    src/core/device_info.h
    src/core/protocol.h
    src/core/protocol.cpp
    src/core/discovery_service.h
    src/core/discovery_service.cpp
    src/core/transfer_server.h
    src/core/transfer_server.cpp
    src/core/file_receiver.h
    src/core/file_receiver.cpp
    src/core/file_sender.h
    src/core/file_sender.cpp
)

set(UI_SOURCES
    src/ui/main_window.h
    src/ui/main_window.cpp
    src/ui/device_list_widget.h
    src/ui/device_list_widget.cpp
    src/ui/receive_dialog.h
    src/ui/receive_dialog.cpp
)

qt_add_executable(LanDrop
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    resources/icon.rc
)

target_include_directories(LanDrop PRIVATE src)

target_link_libraries(LanDrop
    PRIVATE
        Qt::Core
        Qt::Network
        Qt::Widgets
)

install(TARGETS LanDrop
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET LanDrop
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

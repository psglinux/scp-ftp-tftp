cmake_minimum_required(VERSION 3.16)
project(GotiKinesis VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)
find_package(CURL REQUIRED)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
set(APP_SOURCES
    src/main.cpp
    src/mainwindow.h    src/mainwindow.cpp
    src/transferengine.h src/transferengine.cpp
    src/tftpclient.h     src/tftpclient.cpp
)

if(WIN32)
    list(APPEND APP_SOURCES packaging/windows/gotikinesis.rc)
endif()

add_executable(gotikinesis ${APP_SOURCES})

target_include_directories(gotikinesis PRIVATE
    ${CURL_INCLUDE_DIRS}
    $<$<PLATFORM_ID:Windows>:${CMAKE_SOURCE_DIR}/packaging/windows>
)

target_link_libraries(gotikinesis PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    CURL::libcurl
)

# On Windows: build as a GUI app (no console flash); CLI mode attaches console.
if(WIN32)
    set_target_properties(gotikinesis PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# On macOS: create an app bundle.
if(APPLE)
    set_target_properties(gotikinesis PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST
            "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME       "GotiKinesis"
        MACOSX_BUNDLE_BUNDLE_VERSION    "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER    "com.gotikinesis.app"
    )
endif()

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
install(TARGETS gotikinesis
    RUNTIME DESTINATION bin        # Windows / Linux
    BUNDLE  DESTINATION .          # macOS .app
)

if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/gotikinesis.desktop
            DESTINATION share/applications)
    install(FILES packaging/linux/gotikinesis_48.png
            DESTINATION share/icons/hicolor/48x48/apps
            RENAME gotikinesis.png)
    install(FILES packaging/linux/gotikinesis_128.png
            DESTINATION share/icons/hicolor/128x128/apps
            RENAME gotikinesis.png)
    install(FILES packaging/linux/gotikinesis_256.png
            DESTINATION share/icons/hicolor/256x256/apps
            RENAME gotikinesis.png)
endif()

# On Windows — run windeployqt at install time to bundle Qt DLLs.
if(WIN32)
    get_target_property(_qt_qmake Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    if(WINDEPLOYQT_EXECUTABLE)
        install(CODE "
            message(STATUS \"Running windeployqt...\")
            execute_process(
                COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                    --no-compiler-runtime
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    --dir \"\${CMAKE_INSTALL_PREFIX}/bin\"
                    \"\${CMAKE_INSTALL_PREFIX}/bin/gotikinesis.exe\"
            )
        ")
    endif()
endif()

# ---------------------------------------------------------------------------
# CPack — cross-platform packaging
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME          "GotiKinesis")
set(CPACK_PACKAGE_VERSION       ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR        "GotiKinesis")
set(CPACK_PACKAGE_CONTACT       "maintainer@example.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Cross-platform SCP / FTP / TFTP file transfer utility")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

# ---- Windows NSIS installer ----
if(WIN32)
    set(CPACK_GENERATOR "NSIS")

    set(CPACK_NSIS_DISPLAY_NAME    "GotiKinesis")
    set(CPACK_NSIS_PACKAGE_NAME    "GotiKinesis ${PROJECT_VERSION}")
    set(CPACK_NSIS_INSTALL_ROOT    "$PROGRAMFILES64")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\gotikinesis.exe")
    set(CPACK_NSIS_URL_INFO_ABOUT  "https://github.com/example/gotikinesis")
    set(CPACK_NSIS_CONTACT         "maintainer@example.com")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH     ON)

    # Installer / uninstaller icon
    set(CPACK_NSIS_MUI_ICON
        "${CMAKE_SOURCE_DIR}/packaging/windows/app.ico")
    set(CPACK_NSIS_MUI_UNIICON
        "${CMAKE_SOURCE_DIR}/packaging/windows/app.ico")

    # Welcome / finish page sidebar bitmap (164 x 314)
    set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP
        "${CMAKE_SOURCE_DIR}/packaging/windows/nsis_welcome.bmp")
    # Header bitmap shown on every page (150 x 57)
    set(CPACK_NSIS_MUI_HEADERIMAGE_BITMAP
        "${CMAKE_SOURCE_DIR}/packaging/windows/nsis_header.bmp")

    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\GotiKinesis.lnk' '$INSTDIR\\\\bin\\\\gotikinesis.exe'
         CreateShortCut '$DESKTOP\\\\GotiKinesis.lnk' '$INSTDIR\\\\bin\\\\gotikinesis.exe'")

    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\GotiKinesis.lnk'
         Delete '$DESKTOP\\\\GotiKinesis.lnk'")
endif()

# ---- Linux DEB package ----
if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_NAME "gotikinesis")

    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "maintainer@example.com")
    set(CPACK_DEBIAN_PACKAGE_SECTION    "net")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY   "optional")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE
        "https://github.com/example/gotikinesis")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
        "Cross-platform SCP/FTP/TFTP file transfer utility.\n Desktop GUI and CLI for uploading and downloading files\n over SCP (SSH), FTP, and TFTP.")

    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_SOURCE_DIR}/packaging/linux/postinst;${CMAKE_SOURCE_DIR}/packaging/linux/postrm")
endif()

# ---- macOS DMG ----
if(APPLE)
    set(CPACK_GENERATOR            "DragNDrop")
    set(CPACK_DMG_FORMAT           "UDBZ")
    set(CPACK_DMG_VOLUME_NAME      "GotiKinesis")
endif()

include(CPack)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
option(BUILD_TESTS "Build unit and integration tests" ON)

if(BUILD_TESTS)
    enable_testing()
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

    add_library(app_core STATIC
        src/transferengine.h  src/transferengine.cpp
        src/tftpclient.h      src/tftpclient.cpp
    )
    target_include_directories(app_core PUBLIC
        ${CMAKE_SOURCE_DIR}/src  ${CURL_INCLUDE_DIRS})
    target_link_libraries(app_core PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        CURL::libcurl
    )

    # -- TFTP packet encoding tests --
    add_executable(test_tftp_packets  tests/test_tftp_packets.cpp)
    target_link_libraries(test_tftp_packets PRIVATE
        app_core  Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME TftpPackets COMMAND test_tftp_packets)

    # -- TransferEngine logic tests --
    add_executable(test_transferengine  tests/test_transferengine.cpp)
    target_link_libraries(test_transferengine PRIVATE
        app_core  Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME TransferEngine COMMAND test_transferengine)

    # -- TFTP integration (mock server) --
    add_executable(test_tftp_integration  tests/test_tftp_integration.cpp)
    target_include_directories(test_tftp_integration PRIVATE
        ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(test_tftp_integration PRIVATE
        app_core  Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME TftpIntegration COMMAND test_tftp_integration)

    # -- MainWindow GUI tests --
    add_executable(test_mainwindow
        tests/test_mainwindow.cpp
        src/mainwindow.h  src/mainwindow.cpp
    )
    target_include_directories(test_mainwindow PRIVATE
        ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_mainwindow PRIVATE
        app_core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
    )

    add_test(NAME MainWindow COMMAND test_mainwindow)

    set_tests_properties(TftpPackets TransferEngine TftpIntegration MainWindow
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

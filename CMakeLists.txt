cmake_minimum_required(VERSION 3.16)

project(ScreenSniper VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt packages
find_package(Qt6 COMPONENTS Core Gui Widgets Network QUIET)
if (NOT Qt6_FOUND)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets Network)
endif()

# Find OpenCV
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui imgcodecs videoio dnn objdetect)

# Source files
set(SOURCES
    aiconfigmanager.cpp
    aimanager.cpp
    main.cpp
    mainwindow.cpp
    pinwidget.cpp
    screenshotwidget.cpp
    ocrmanager.cpp
    ocrresultdialog.cpp
    scrollcapture.cpp
    watermark_robust.cpp
    i18nmanager.cpp
    facedetector.cpp
)

# Header files
set(HEADERS
    aiconfigmanager.h
    aimanager.h
    mainwindow.h
    pinwidget.h
    screenshotwidget.h
    ocrmanager.h
    ocrresultdialog.h
    scrollcapture.h
    watermark_robust.h
    i18nmanager.h
    facedetector.h
)

# UI files
set(FORMS
    mainwindow.ui
)

# Resource files
set(RESOURCES
    resources.qrc
)

# Platform-specific configurations
if(APPLE)
    # macOS specific
    list(APPEND SOURCES macocr.mm)
    list(APPEND HEADERS macocr.h)
    
    find_library(COREGRAPHICS_LIBRARY CoreGraphics REQUIRED)
    find_library(VISION_LIBRARY Vision REQUIRED)
    find_library(APPKIT_LIBRARY AppKit REQUIRED)
    
    set(MACOS_FRAMEWORKS
        ${COREGRAPHICS_LIBRARY}
        ${VISION_LIBRARY}
        ${APPKIT_LIBRARY}
    )
    
    # Tesseract support (optional)
    option(USE_TESSERACT "Use Tesseract OCR instead of native Vision" ON)
    if(USE_TESSERACT)
        add_definitions(-DUSE_TESSERACT)
        # Try to find Tesseract
        find_path(TESSERACT_INCLUDE_DIR tesseract/baseapi.h
            HINTS /opt/homebrew/include /usr/local/include
        )
        find_library(TESSERACT_LIBRARY tesseract
            HINTS /opt/homebrew/lib /usr/local/lib
        )
        find_library(LEPTONICA_LIBRARY leptonica
            HINTS /opt/homebrew/lib /usr/local/lib
        )
        
        if(TESSERACT_INCLUDE_DIR AND TESSERACT_LIBRARY AND LEPTONICA_LIBRARY)
            message(STATUS "Found Tesseract: ${TESSERACT_LIBRARY}")
            include_directories(${TESSERACT_INCLUDE_DIR})
            set(TESSERACT_LIBRARIES ${TESSERACT_LIBRARY} ${LEPTONICA_LIBRARY})
        else()
            message(WARNING "Tesseract not found, will use native Vision framework")
            remove_definitions(-DUSE_TESSERACT)
            set(TESSERACT_LIBRARIES "")
        endif()
    endif()
    
    # Set bundle properties
    set(MACOSX_BUNDLE_BUNDLE_NAME "ScreenSniper")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.screensniper.app")
    set(MACOSX_BUNDLE_ICON_FILE "")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}")
    
elseif(WIN32)
    # Windows specific
    set(WINDOWS_LIBRARIES
        Psapi
        Dwmapi
        user32
        gdi32
    )
    
elseif(UNIX)
    # Linux specific
    find_package(X11 REQUIRED)
    set(LINUX_LIBRARIES
        ${X11_LIBRARIES}
        ${X11_Xfixes_LIB}
        ${X11_Xext_LIB}
    )
endif()

# Create executable
if(APPLE)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${FORMS} ${RESOURCES})
    
    # Set Objective-C++ for .mm files
    set_source_files_properties(macocr.mm PROPERTIES
        COMPILE_FLAGS "-x objective-c++"
    )
else()
    if(WIN32)
        add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${FORMS} ${RESOURCES})
    else()
        add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${FORMS} ${RESOURCES})
    endif()
endif()

# Link libraries
if(Qt6_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::Network
    )
endif()

# Link OpenCV
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS})

# Link platform-specific libraries
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${MACOS_FRAMEWORKS})
    if(TESSERACT_LIBRARIES)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${TESSERACT_LIBRARIES})
    endif()
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${WINDOWS_LIBRARIES})
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LINUX_LIBRARIES})
endif()

# Installation
if(APPLE)
    install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
    )
    # Copy config.ini to bundle
    install(FILES config.ini
        DESTINATION ${PROJECT_NAME}.app/Contents/MacOS
    )
elseif(UNIX)
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
    )
    install(FILES config.ini
        DESTINATION bin
    )
elseif(WIN32)
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION .
    )
    install(FILES config.ini
        DESTINATION .
    )
endif()

# Copy resources
install(DIRECTORY icons/ DESTINATION icons)
install(DIRECTORY locales/ DESTINATION locales)
install(DIRECTORY models/ DESTINATION models)

# Optional: Include local configuration
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/local_config.cmake")
    include(local_config.cmake)
endif()

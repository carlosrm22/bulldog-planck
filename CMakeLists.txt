cmake_minimum_required(VERSION 3.20)

project(
    bulldog-planck
    VERSION 0.2.0
    DESCRIPTION "Mascota animada de escritorio para Linux y KDE"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
find_package(X11 REQUIRED COMPONENTS xcb)

qt_standard_project_setup()

qt_add_executable(planck-pet
    src/main.cpp
)

target_link_libraries(planck-pet PRIVATE Qt6::Widgets X11::xcb)
target_compile_definitions(planck-pet PRIVATE PLANCK_VERSION="${PROJECT_VERSION}")

install(TARGETS planck-pet
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY assets/frames
    DESTINATION ${CMAKE_INSTALL_DATADIR}/planck-pet
    FILES_MATCHING PATTERN "*.png"
)

install(
    FILES planck-pet.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(
    FILES assets/frames/idle/idle-01.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
    RENAME planck-pet.png
)

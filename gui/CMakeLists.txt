set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(volsa2-gui
    main.cpp
    mainwindow.hpp
    mainwindow.cpp
    device_worker.hpp
    device_worker.cpp
    waveform_widget.hpp
    waveform_widget.cpp
    upload_dialog.hpp
    upload_dialog.cpp
    sample_chopper_dialog.hpp
    sample_chopper_dialog.cpp
    audio_player.hpp
    audio_player.cpp
)

target_include_directories(volsa2-gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(volsa2-gui PRIVATE
    volsa2_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
)

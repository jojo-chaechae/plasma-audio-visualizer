set(PLUGIN_NAME "processplugin")
set(LIB_NAME "process")

include(KDEInstallDirs)
include(KDECMakeSettings)
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED Core Gui GuiPrivate Qml Quick ShaderTools)

add_library(${PLUGIN_NAME} SHARED
    ${PLUGIN_NAME}.cpp
    ${LIB_NAME}.cpp
    ${LIB_NAME}.h
    spectrumitem.cpp
    spectrumitem.h
)

target_link_libraries(${PLUGIN_NAME}
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Qml
    Qt6::Quick
)

qt_add_shaders(${PLUGIN_NAME} "spectrum_shaders"
    PRECOMPILE
    OPTIMIZED
    PREFIX "/spectrum"
    FILES
        shaders/spectrum.vert
        shaders/spectrum.frag
)

install(TARGETS ${PLUGIN_NAME} DESTINATION ${KDE_INSTALL_QMLDIR}/com/github/luisbocanegra/audiovisualizer/${LIB_NAME})
install(FILES qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/com/github/luisbocanegra/audiovisualizer/${LIB_NAME})

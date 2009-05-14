add_subdirectory(pics)

########### next target ###############

set(kbackup_SRCS
    Archiver.cxx
    MainWindow.cxx
    Selector.cxx
    main.cxx
    MainWidget.cxx
    SettingsDialog.cxx
    )

kde4_add_ui_files(kbackup_SRCS MainWidgetBase.ui SettingsDialog.ui)

kde4_add_executable(kbackup ${kbackup_SRCS})

target_link_libraries(kbackup ${KDE4_KDECORE_LIBS} ${KDE4_KIO_LIBS} ${KDE4_KDEUI_LIBS} ${QT_QT3SUPPORT_LIBRARY})

install(TARGETS kbackup ${INSTALL_TARGETS_DEFAULT_ARGS})

find_package(SharedMimeInfo REQUIRED)

install(FILES kbackup.xml DESTINATION ${XDG_MIME_INSTALL_DIR})
update_xdg_mimetypes(${XDG_MIME_INSTALL_DIR})

install(FILES  kbackup.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install(FILES  kbackupui.rc DESTINATION ${DATA_INSTALL_DIR}/kbackup)

kde4_install_icons(${ICON_INSTALL_DIR})

add_subdirectory(pics)
add_executable(kbackup)

target_sources(kbackup PRIVATE
    Archiver.cxx
    MainWindow.cxx
    Selector.cxx
    main.cxx
    MainWidget.cxx
    SettingsDialog.cxx

    Archiver.hxx
    Selector.hxx
    MainWidget.hxx
    MainWindow.hxx
    SettingsDialog.hxx
    kbackup.qrc
    )

ki18n_wrap_ui(kbackup MainWidgetBase.ui SettingsDialog.ui)

find_package(LibArchive REQUIRED)

target_link_libraries(kbackup
                      archive
                      Qt${QT_MAJOR_VERSION}::Core
                      Qt${QT_MAJOR_VERSION}::Widgets
                      KF${KF_MAJOR_VERSION}::GuiAddons
                      KF${KF_MAJOR_VERSION}::I18n
                      KF${KF_MAJOR_VERSION}::KIOCore
                      KF${KF_MAJOR_VERSION}::KIOFileWidgets
                      KF${KF_MAJOR_VERSION}::KIOWidgets
                      KF${KF_MAJOR_VERSION}::WidgetsAddons
                      KF${KF_MAJOR_VERSION}::XmlGui
                      KF${KF_MAJOR_VERSION}::Notifications
                      KF${KF_MAJOR_VERSION}::IconThemes
                      KF${KF_MAJOR_VERSION}::Archive
)

if (TARGET Qt::Core5Compat)
    target_link_libraries(kbackup Qt::Core5Compat)
endif()

install(TARGETS kbackup ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

find_package(SharedMimeInfo REQUIRED)

install(FILES kbackup.xml DESTINATION ${KDE_INSTALL_MIMEDIR})
update_xdg_mimetypes(${KDE_INSTALL_MIMEDIR})

install(FILES  org.kde.kbackup.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.kde.kbackup.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

ecm_install_icons(ICONS
                  16-apps-kbackup.png
                  32-apps-kbackup.png
                  16-mimetypes-text-x-kbp.png
                  32-mimetypes-text-x-kbp.png
                  DESTINATION ${KDE_INSTALL_ICONDIR}
                  THEME hicolor)

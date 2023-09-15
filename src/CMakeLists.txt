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

target_link_libraries(kbackup
                      archive
                      Qt6::Core
                      Qt6::Widgets
                      Qt6::Core5Compat
                      KF6::GuiAddons
                      KF6::I18n
                      KF6::KIOCore
                      KF6::KIOFileWidgets
                      KF6::KIOWidgets
                      KF6::WidgetsAddons
                      KF6::XmlGui
                      KF6::Notifications
                      KF6::IconThemes
                      KF6::Archive
                      KF6::StatusNotifierItem
)


install(TARGETS kbackup ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

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

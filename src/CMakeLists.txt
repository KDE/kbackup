add_subdirectory(pics)

set(kbackup_SRCS
    Archiver.cxx
    MainWindow.cxx
    Selector.cxx
    main.cxx
    MainWidget.cxx
    SettingsDialog.cxx
    )

ki18n_wrap_ui(kbackup_SRCS MainWidgetBase.ui SettingsDialog.ui)

add_executable(kbackup ${kbackup_SRCS})
target_link_libraries(kbackup
                      Qt::Core
                      Qt::Widgets
                      KF5::GuiAddons
                      KF5::I18n
                      KF5::KIOCore
                      KF5::KIOFileWidgets
                      KF5::KIOWidgets
                      KF5::WidgetsAddons
                      KF5::XmlGui
                      KF5::Notifications
                      KF5::IconThemes
                      KF5::Archive
)

install(TARGETS kbackup ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

find_package(SharedMimeInfo REQUIRED)

install(FILES kbackup.xml DESTINATION ${KDE_INSTALL_MIMEDIR})
update_xdg_mimetypes(${KDE_INSTALL_MIMEDIR})

install(FILES  org.kde.kbackup.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES  kbackupui.rc DESTINATION ${KDE_INSTALL_KXMLGUI5DIR}/kbackup)
install(FILES org.kde.kbackup.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

ecm_install_icons(ICONS
                  16-apps-kbackup.png
                  32-apps-kbackup.png
                  16-mimetypes-text-x-kbp.png
                  32-mimetypes-text-x-kbp.png
                  DESTINATION ${KDE_INSTALL_ICONDIR}
                  THEME hicolor)

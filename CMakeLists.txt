cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

find_package(ECM 1.3.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

add_definitions(-DTRANSLATION_DOMAIN="kbackup")

include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)
include(ECMInstallIcons)
include(FeatureSummary)

find_package(Qt5 CONFIG REQUIRED COMPONENTS
    Core
    Widgets
)

find_package(KF5 REQUIRED COMPONENTS
    Notifications
    I18n
    KIO
    DocTools
    XmlGui
    IconThemes
    Archive
    WidgetsAddons
    KDELibs4Support
)

add_subdirectory(src)
add_subdirectory(doc)
add_subdirectory(po)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

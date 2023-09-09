cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

# KDE Application Version, managed by release script
set(RELEASE_SERVICE_VERSION_MAJOR "23")
set(RELEASE_SERVICE_VERSION_MINOR "08")
set(RELEASE_SERVICE_VERSION_MICRO "1")
set(RELEASE_SERVICE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")

project(kbackup VERSION ${RELEASE_SERVICE_VERSION})

set(QT_MIN_VERSION "5.15.2")
set(KF_MIN_VERSION "5.100.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

add_definitions(-DTRANSLATION_DOMAIN="kbackup" -DKBACKUP_VERSION="${RELEASE_SERVICE_VERSION}")

include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)
include(ECMInstallIcons)
include(FeatureSummary)
include(ECMDeprecationSettings)
if (QT_MAJOR_VERSION STREQUAL "6")
    set(QT_REQUIRED_VERSION "6.4.0")
    set(KF_MIN_VERSION "5.240.0")
    set(KF_MAJOR_VERSION "6")
else()
    set(KF_MAJOR_VERSION "5")
endif()
find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    Widgets
)
if (QT_MAJOR_VERSION STREQUAL "6")
    find_package(Qt6Core5Compat)
endif()
find_package(KF${KF_MAJOR_VERSION} ${KF_MIN_VERSION} REQUIRED COMPONENTS
    GuiAddons
    Notifications
    I18n
    KIO
    XmlGui
    IconThemes
    Archive
    WidgetsAddons
)
find_package(KF${KF_MAJOR_VERSION}DocTools ${KF_MIN_VERSION})
set_package_properties(KF${KF_MAJOR_VERSION}DocTools PROPERTIES DESCRIPTION
    "Tools to generate documentation"
    TYPE OPTIONAL
)
ecm_set_disabled_deprecation_versions(QT 5.15.2  KF 5.99.0)


add_subdirectory(src)

ki18n_install(po)
if (KF5DocTools_FOUND)
    kdoctools_install(po)
    add_subdirectory(doc)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

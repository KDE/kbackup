## Makefile.am for kbackup

SUBDIRS = pics

# this is the program that gets installed.  it's name is used for all
# of the other Makefile.am variables
bin_PROGRAMS = kbackup

# set the include path for X, qt and KDE
INCLUDES         = $(all_includes)

# the library search path. 
kbackup_LDFLAGS = $(KDE_RPATH) $(all_libraries)

# the libraries to link against.
kbackup_LDADD   = $(LIB_KDECORE) $(LIB_KIO) $(LIB_KDEUI)

# which sources should be compiled for kbackup
kbackup_SOURCES = Archiver.cxx MainWindow.cxx Selector.cxx main.cxx MainWidget.ui

# these are the headers for your project
noinst_HEADERS = Archiver.hxx MainWindow.hxx Selector.hxx MainWidget.ui.h

# let automoc handle all of the meta source files (moc)
METASOURCES = AUTO

messages: rc.cpp
	$(EXTRACTRC) `find . -name \*.ui -o -name \*.rc` > rc.cpp
	$(XGETTEXT) *.cxx *.cpp *.h *.hxx -o $(podir)/kbackup.pot

KDE_ICON = AUTO

xdg_apps_DATA = kbackup.desktop

# this is where the XML-GUI resource file goes
rcdir = $(kde_datadir)/kbackup
rc_DATA = kbackupui.rc

mimedir = $(kde_mimedir)/text
mime_DATA = x-kbp.desktop

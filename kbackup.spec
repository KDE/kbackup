Summary: kbackup is an application which lets you back up your data in a simple, user friendly way.
Name: kbackup
Version: 0.4.2
Release: 1
Copyright: GPL
Group: Applications/Archiving
Source: http://members.aon.at/m.koller/kbackup-0.4.2.tar.bz2

%description
KBackup is a program that lets you back up any directories or files,
whereby it uses an easy to use directory tree to select the things to back up.
 
The program was designed to be very simple in its use
so that it can be used by non-computer experts.
 
The storage format is the well known TAR format, whereby the data
is still stored in compressed format (bzip2 or gzip).

Included Languages:
- User interface:
  English, German, French, Italian, Russian
- Handbook:
  English, German

%prep
%setup
./configure

%build
make

%install
make install

%files
%defattr(-,root,root)

/opt/kde3/bin/kbackup
/opt/kde3/share/applications/kde/kbackup.desktop
/opt/kde3/share/doc/HTML/en/kbackup/
/opt/kde3/share/doc/HTML/de/kbackup/
/opt/kde3/share/mimelnk/text/x-kbp.desktop
/opt/kde3/share/icons/hicolor/16x16/mimetypes/kbackup.png
/opt/kde3/share/icons/hicolor/32x32/mimetypes/kbackup.png
/opt/kde3/share/icons/hicolor/16x16/apps/kbackup.png
/opt/kde3/share/icons/hicolor/32x32/apps/kbackup.png
/opt/kde3/share/apps/kbackup/
/opt/kde3/share/locale/de/LC_MESSAGES/kbackup.mo
/opt/kde3/share/locale/fr/LC_MESSAGES/kbackup.mo
/opt/kde3/share/locale/it/LC_MESSAGES/kbackup.mo
/opt/kde3/share/locale/ru/LC_MESSAGES/kbackup.mo

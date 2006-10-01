Summary: kbackup is an application which lets you back up your data in a simple, user friendly way.
Name: kbackup
Version: 0.5.1
Release: 1
Copyright: GPL
Group: Applications/Archiving
Source: http://members.aon.at/m.koller/kbackup-%{version}.tar.bz2

%description
KBackup is a program that lets you back up any directories or files,
whereby it uses an easy to use directory tree to select the things to back up.
 
The program was designed to be very simple in its use
so that it can be used by non-computer experts.
 
The storage format is the well known TAR format, whereby the data
is still stored in compressed format (bzip2 or gzip).

Included Languages:
- User interface:
  English, German, French, Italian, Russian, Slovak
- Handbook:
  English, German, French

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
/opt/kde3/share/doc/HTML/*/kbackup/
/opt/kde3/share/mimelnk/text/x-kbp.desktop
/opt/kde3/share/icons/hicolor/16x16/mimetypes/kbackup.png
/opt/kde3/share/icons/hicolor/32x32/mimetypes/kbackup.png
/opt/kde3/share/icons/hicolor/16x16/apps/kbackup.png
/opt/kde3/share/icons/hicolor/32x32/apps/kbackup.png
/opt/kde3/share/apps/kbackup/
/opt/kde3/share/locale/*/LC_MESSAGES/kbackup.mo

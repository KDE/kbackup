Summary: kbackup is an application which lets you back up your data in a simple, user friendly way.
Name: kbackup
Version: 0.6
Release: 1
License: GPL
Group: Applications/Archiving
Source: http://members.aon.at/m.koller/kbackup-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-buildroot

%description
KBackup is a program that lets you back up any directories or files,
whereby it uses an easy to use directory tree to select the things to back up.
 
The program was designed to be very simple in its use
so that it can be used by non-computer experts.
 
The storage format is the well known TAR format, whereby the data
is still stored in compressed format (bzip2 or gzip).

Included Languages:
- User interface:
  English, German, French, Italian, Russian, Slovak, Portuguese, Spanish, Swedish
- Handbook:
  English, German, French

%prep
%setup
mkdir kbackup-build
cd kbackup-build && cmake -DCMAKE_INSTALL_PREFIX=${RPM_BUILD_ROOT}/usr ..

%build
cd kbackup-build && make

%install
cd kbackup-build && make install

%clean
if [ "${RPM_BUILD_ROOT}" != "/" -a ! -z "${RPM_BUILD_ROOT}" ]
then
  rm -Rf ${RPM_BUILD_ROOT}
fi

%files
%defattr(-,root,root)

/usr/bin/kbackup
/usr/share/mime/packages/kbackup.xml
/usr/share/applications/kde4/kbackup.desktop
/usr/share/icons/hicolor/16x16/mimetypes/text-x-kbp.png
/usr/share/icons/hicolor/32x32/mimetypes/text-x-kbp.png
/usr/share/icons/hicolor/16x16/apps/kbackup.png
/usr/share/icons/hicolor/32x32/apps/kbackup.png
/usr/share/kde4/apps/kbackup/
%doc /usr/share/doc/kde/HTML/*/kbackup/
/usr/share/locale/*/LC_MESSAGES/kbackup.mo

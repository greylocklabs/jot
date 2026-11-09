# Link-time optimization would redo the whole core library for every test program, doubling the
# build for little gain in a GUI app.
%global _lto_cflags %{nil}

Name:           jot
Version:        0.4.1
Release:        1%{?dist}
Summary:        Plain-file notes for Linux

# The bundled Nunito and Fraunces fonts are OFL-1.1; the bundled Pele is MIT.
License:        MIT AND OFL-1.1
URL:            https://jotwriter.org
Source0:        https://github.com/greylocklabs/jot/releases/download/v%{version}/%{name}-%{version}.tar.xz
# Draws Mermaid diagrams; built into the program for Qt's JavaScript engine.
Provides:       bundled(js-pele) = 1.13.0

BuildRequires:  cmake >= 3.28
BuildRequires:  gcc-c++
BuildRequires:  ninja-build
BuildRequires:  cmake(Qt6Core)
BuildRequires:  cmake(Qt6DBus)
BuildRequires:  cmake(Qt6Gui)
BuildRequires:  cmake(Qt6Network)
BuildRequires:  cmake(Qt6Qml)
BuildRequires:  cmake(Qt6Quick)
BuildRequires:  cmake(Qt6QuickControls2)
BuildRequires:  cmake(Qt6Sql)
BuildRequires:  cmake(Qt6Test)
BuildRequires:  cmake(tomlplusplus)
BuildRequires:  cmake(KF6SyntaxHighlighting)
BuildRequires:  cmake(KF6Sonnet)
BuildRequires:  cmake(Qt6Multimedia)
BuildRequires:  cmake(Qt6Keychain)
BuildRequires:  pkgconf-pkg-config
BuildRequires:  pkgconfig(yaml-0.1)
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib
# Diagrams are drawn as SVG, which Qt reads through this plugin.
BuildRequires:  qt6-qtsvg

Requires:       hicolor-icon-theme
Requires:       qt6-qtsvg
# Recording and playing sounds, from QML too.
Requires:       qt6-qtmultimedia
# Syncing with a git repository runs git, and Git LFS for large files.
Requires:       git-core
Requires:       git-lfs
# Exporting a note runs Pandoc.
Recommends:     pandoc
# Spellcheck reads the desktop's Hunspell dictionaries.
Recommends:     kf6-sonnet-hunspell

%description
Jot is a Markdown notes app for Linux. Notes are plain files in a folder
you choose, and sync is optional. It shares its look and its themes with
Callie, the calendar from the same makers. A command-line interface is
included.

%prep
%autosetup

%build
%cmake -GNinja
%cmake_build

%install
%cmake_install

%check
%ctest
desktop-file-validate %{buildroot}%{_datadir}/applications/org.callieapp.Jot.desktop
appstream-util validate-relax --nonet \
    %{buildroot}%{_datadir}/metainfo/org.callieapp.Jot.metainfo.xml

%files
%license LICENSE data/fonts/Nunito-OFL.txt data/fonts/Fraunces-OFL.txt
%license third_party/pele/LICENSE
%doc README.md
%{_bindir}/jot
%{_bindir}/jot-gui
%{_datadir}/bash-completion/completions/jot
%{_datadir}/zsh/site-functions/_jot
%{_datadir}/fish/vendor_completions.d/jot.fish
%{_datadir}/applications/org.callieapp.Jot.desktop
%{_datadir}/metainfo/org.callieapp.Jot.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/org.callieapp.Jot.svg
%{_datadir}/icons/hicolor/16x16/apps/org.callieapp.Jot.png
%{_datadir}/icons/hicolor/32x32/apps/org.callieapp.Jot.png
%{_datadir}/jot/

%changelog
* Sat Nov 07 2026 Lara Kelley <hello@greylocklabs.org> - 0.4.1-1
- Update to 0.4.1

* Sat Nov 07 2026 Lara Kelley <hello@greylocklabs.org> - 0.4.0-1
- Update to 0.4.0

* Fri Nov 06 2026 Lara Kelley <hello@greylocklabs.org> - 0.3.0-1
- Update to 0.3.0

* Tue Nov 03 2026 Lara Kelley <hello@greylocklabs.org> - 0.2.0-1
- Update to 0.2.0

* Tue Nov 03 2026 Lara Kelley <hello@greylocklabs.org> - 0.1.2-1
- Update to 0.1.2

* Mon Nov 02 2026 Lara Kelley <hello@greylocklabs.org> - 0.1.1-1
- Update to 0.1.1

* Sat Oct 31 2026 Lara Kelley <hello@greylocklabs.org> - 0.1.0-1
- Update to 0.1.0

* Tue Oct 27 2026 Lara Kelley <hello@greylocklabs.org> - 0.0.12-1
- Update to 0.0.12

* Tue Oct 27 2026 Lara Kelley <hello@greylocklabs.org> - 0.0.11-1
- Update to 0.0.11

* Tue Oct 27 2026 Lara Kelley <hello@greylocklabs.org> - 0.0.10-1
- Update to 0.0.10

* Sun Oct 25 2026 Lara Kelley <hello@greylocklabs.org> - 0.0.9-1
- Update to 0.0.9

* Mon Oct 19 2026 Lara Kelley <hello@greylocklabs.org> - 0.0.8-1
- Update to 0.0.8

* Sat Oct 17 2026 Lara Kelley <hello@greylocklabs.org> - 0.0.7-1
- Update to 0.0.7

* Sat Oct 17 2026 Lara Kelley <hello@greylocklabs.org> - 0.0.6-1
- Update to 0.0.6

* Fri Oct 16 2026 Lara Kelley <hello@greylocklabs.org> - 0.0.5-1
- Update to 0.0.5

* Thu Oct 15 2026 Lara Kelley <hello@greylocklabs.org> - 0.0.4-1
- Update to 0.0.4

* Thu Oct 15 2026 Lara Kelley <hello@greylocklabs.org> - 0.0.3-1
- Update to 0.0.3

* Wed Oct 14 2026 Lara Kelley <hello@greylocklabs.org> - 0.0.2-1
- Update to 0.0.2

* Wed Oct 14 2026 Lara Kelley <hello@greylocklabs.org> - 0.0.1-1
- Update to 0.0.1

* Wed Oct 14 2026 Lara Kelley <hello@greylocklabs.org> - 0.0.0-1
- Initial package

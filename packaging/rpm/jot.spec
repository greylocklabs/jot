Name:           jot
Version:        0.0.6
Release:        1%{?dist}
Summary:        Plain-file notes for Linux

# The bundled Nunito and Fraunces fonts are OFL-1.1.
License:        MIT AND OFL-1.1
URL:            https://jotwriter.org
Source0:        https://github.com/greylocklabs/jot/releases/download/v%{version}/%{name}-%{version}.tar.xz

BuildRequires:  cmake >= 3.28
BuildRequires:  gcc-c++
BuildRequires:  ninja-build
BuildRequires:  cmake(Qt6Core)
BuildRequires:  cmake(Qt6Gui)
BuildRequires:  cmake(Qt6Network)
BuildRequires:  cmake(Qt6Qml)
BuildRequires:  cmake(Qt6Quick)
BuildRequires:  cmake(Qt6QuickControls2)
BuildRequires:  cmake(Qt6Sql)
BuildRequires:  cmake(Qt6Test)
BuildRequires:  cmake(tomlplusplus)
BuildRequires:  cmake(KF6SyntaxHighlighting)
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib

Requires:       hicolor-icon-theme

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
* Sat Oct 17 2026 Lara Kelley <larakelley@higharc.ai> - 0.0.6-1
- Update to 0.0.6

* Fri Oct 16 2026 Lara Kelley <larakelley@higharc.ai> - 0.0.5-1
- Update to 0.0.5

* Thu Oct 15 2026 Lara Kelley <larakelley@higharc.ai> - 0.0.4-1
- Update to 0.0.4

* Thu Oct 15 2026 Lara Kelley <larakelley@higharc.ai> - 0.0.3-1
- Update to 0.0.3

* Wed Oct 14 2026 Lara Kelley <larakelley@higharc.ai> - 0.0.2-1
- Update to 0.0.2

* Wed Oct 14 2026 Lara Kelley <larakelley@higharc.ai> - 0.0.1-1
- Update to 0.0.1

* Wed Oct 14 2026 Lara Kelley <larakelley@higharc.ai> - 0.0.0-1
- Initial package

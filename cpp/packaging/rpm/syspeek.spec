Name:           syspeek
Version:        0.3.2
Release:        1%{?dist}
Summary:        Compact always-on-top HUD for focused-process and infra status

License:        MIT
URL:            https://github.com/codeafridi/Syspeek
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  gcc-c++
BuildRequires:  cmake
BuildRequires:  qt6-qtbase-devel
Requires:       qt6-qtbase

%description
Syspeek shows the focused application's PID, CPU, and RSS (aggregated
across its process group) plus optional Docker and Kubernetes health in a small
always-on-top overlay.

%prep
%autosetup

%build
%cmake -S cpp -B cpp/build
%cmake_build

%install
install -Dm0755 cpp/build/syspeek %{buildroot}%{_bindir}/syspeek
sed 's|@BINARY@|%{_bindir}/syspeek|g' \
  cpp/packaging/syspeek.service.in \
  > syspeek.service
install -Dm0644 syspeek.service %{buildroot}%{_userunitdir}/syspeek.service
install -Dm0644 cpp/packaging/syspeek.desktop.in \
  %{buildroot}%{_datadir}/applications/syspeek.desktop

%files
%{_bindir}/syspeek
%{_userunitdir}/syspeek.service
%{_datadir}/applications/syspeek.desktop

%changelog
* Sun Oct 11 2026 Syspeek <noreply@example.com> - 0.3.2-1
- AppImage packaging
* Thu Jan 01 2026 Syspeek <noreply@example.com> - 0.1.0-1
- Initial package

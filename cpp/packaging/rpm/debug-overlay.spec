Name:           debug-overlay
Version:        0.1.0
Release:        1%{?dist}
Summary:        Compact always-on-top HUD for focused-process and infra status

License:        MIT
URL:            https://github.com/codeafridi/Debug-Overlay-App
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  gcc-c++
BuildRequires:  cmake
BuildRequires:  qt6-qtbase-devel
Requires:       qt6-qtbase

%description
Debug Overlay shows the focused application's PID, CPU, and RSS (aggregated
across its process group) plus optional Docker and Kubernetes health in a small
always-on-top overlay.

%prep
%autosetup

%build
%cmake -S cpp -B cpp/build
%cmake_build

%install
install -Dm0755 cpp/build/debug_overlay_hud %{buildroot}%{_bindir}/debug-overlay
sed 's|@BINARY@|%{_bindir}/debug-overlay|g' \
  cpp/packaging/debug-overlay.service.in \
  > debug-overlay.service
install -Dm0644 debug-overlay.service %{buildroot}%{_userunitdir}/debug-overlay.service
install -Dm0644 cpp/packaging/debug-overlay.desktop.in \
  %{buildroot}%{_datadir}/applications/debug-overlay.desktop

%files
%{_bindir}/debug-overlay
%{_userunitdir}/debug-overlay.service
%{_datadir}/applications/debug-overlay.desktop

%changelog
* Thu Jan 01 2026 Debug Overlay <noreply@example.com> - 0.1.0-1
- Initial package

Name:       ru.nighteugene.MyTravelPass
Summary:    Мой проездной
Version:    1.3.2
Release:    1
Group:      Qt/Qt
License:    BSD-3-Clause
URL:        https://github.com/NightEugene/aurora-mytravelpass
Source0:    %{name}-%{version}.tar.bz2

Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   aurora-controls
Requires:   qt5-qtgraphicaleffects
Requires:   nfcd
BuildRequires:  pkgconfig(auroraapp)
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)

%description
«Мой проездной» показывает баланс, данные о поездках и проездных
при поднесении транспортной карты к телефону. Карта читается по NFC
как MIFARE Classic через D-Bus API nfcd.

Пока поддерживается только карта «Подорожник» (Санкт-Петербург).

%prep
%autosetup

%build
%qmake5
%make_build

%install
%make_install

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%defattr(644,root,root,-)
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png

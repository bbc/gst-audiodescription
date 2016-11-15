Name:    gst-audiodescription
Version: 1.0.0
Release: 1%{?dist}
Summary: Gstreamer elements for processing audio description metadata
URL: https://github.com/dholroyd/gst-audiodescription

Requires: gstreamer1, gstreamer1-plugins-base
BuildRequires: gstreamer1-devel, gstreamer1-plugins-base-devel

License: LGPL
Source0: %{name}-%{version}.tar.gz

%description
The audiodescription Gstreamer plugin provides the elements
 - whp198dec - decodes audio-description metadata itself encoded into an audio
   waveform
 - adcontrol - processes metadata produced by whp198dec to 'fade' another audio
   track (so as to make the description audio be heard clearly)

%prep
%setup

%build
%configure
make


%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install
rm $RPM_BUILD_ROOT/usr/lib64/gstreamer-1.0/*.la
rm $RPM_BUILD_ROOT/usr/lib64/gstreamer-1.0/*.a

%clean
rm -rf $RPM_BUILD_ROOT

%files
%{_libdir}/gstreamer-1.0/*.so

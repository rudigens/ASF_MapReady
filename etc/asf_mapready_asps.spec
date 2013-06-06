Summary: ASF Tools for processing SAR data, Seasat processing edition
Name: asf_mapready_asps
Version: %{asftoolsversion}
Release: %{asftoolsbuildnumber}
License: BSD
Group: Applications/Scientific
URL: http://www.asf.alaska.edu
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Prefix: /usr/local

%description
The ASF MapReady Remote Sensing Toolkit is a set of tools for
processing SAR data, including importing the raw CEOS data,
geocoding to various map projections, terrain correction,
as well as exporting to various common formats including
JPEG and GeoTIFF.

The ASF MapReady Remote Sensing Toolkit now supports the
processing of ALOS data.

This version is an ASF Internal release with modifications
needed for processing of SEASAT data.
%prep
echo Executing: %%prep
%setup -DT -n asf_tools
 
%build
echo Executing: %%build
cd $RPM_BUILD_DIR/asf_tools
./configure --prefix=$RPM_BUILD_ROOT/usr/local
DEBUG_BUILD=1 make

%install
echo Executing: %%install
rm -rf $RPM_BUILD_ROOT
cd $RPM_BUILD_DIR/asf_tools
make install
rm -rf $RPM_BUILD_ROOT/usr/local/lib

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(666,root,root,777)
%attr(-,root,root) /usr/local/bin
/usr/local/share


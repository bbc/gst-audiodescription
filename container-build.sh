#!/bin/bash

set -eu

CONTAINER=gst-ad-build
NAME="--name $CONTAINER"

lxc-create $NAME --template download -- --dist centos --arch amd64 --release 7
echo "lxc.network.flags = up" >> ~/.local/share/lxc/$CONTAINER/config
lxc-start $NAME

echo -n "Container starting"
until [ "`lxc-attach $NAME -- runlevel`" = "N 5" ]; do echo -n "."; sleep 1; done	
echo " done"

lxc-attach --clear-env $NAME -- bash -c "
set -eux

yum --assumeyes install gstreamer1-devel gstreamer1-plugins-base-devel git autoconf automake libtool make rpm-build
git clone https://github.com/dholroyd/gst-audiodescription.git
cd gst-audiodescription
./autogen.sh
make dist
mkdir -p /rpmbuild/SOURCES/
mv gst-audiodescription-*.tar.gz /rpmbuild/SOURCES/
rpmbuild -bb gst-audiodescription.spec
"

cp $HOME/.local/share/lxc/$CONTAINER/rootfs/rpmbuild/RPMS/x86_64/gst-audiodescription-*.rpm .

lxc-stop $NAME
lxc-destroy $NAME

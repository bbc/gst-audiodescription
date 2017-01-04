FROM debian:jessie
MAINTAINER Simeon van der Steen <simeon@simeonvandersteen.nl>

ENV DEBIAN_FRONTEND noninteractive

# Install dependencies (don't cleanup for now)
RUN apt-get -y update && apt-get install -y --no-install-recommends \
	libtool automake build-essential \
	gstreamer1.0-tools libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev

# Get source
COPY . /usr/local/src/gst-audiodescription
WORKDIR /usr/local/src/gst-audiodescription

RUN ./autogen.sh && make && make install

ENV GST_PLUGIN_PATH /usr/local/lib/gstreamer-1.0

RUN gst-inspect-1.0 audiodescription


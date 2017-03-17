# This container amins to offer a testing environment similar to the one we are
# creating in travis-ci so we can easily reproduce issues detected on our CI
# locally
#
# To use it, simply execute the container like this:
# docker run --rm -ti --privileged -v /path/to/linuxdeployqt:/linuxdeployqt bash
# and then execute tests/tests-ci.sh

FROM ubuntu:trusty

RUN apt-get update && apt-get -y install software-properties-common wget build-essential \
    autoconf git fuse libgl1-mesa-dev psmisc

COPY tests/tests-environment.sh /

RUN /tests-environment.sh

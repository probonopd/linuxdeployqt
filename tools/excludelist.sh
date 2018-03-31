#!/bin/bash

# Download excludelist
blacklisted=($(wget --quiet https://raw.githubusercontent.com/probonopd/AppImages/master/excludelist -O - | sort | uniq | grep -v "^#.*" | grep "[^-\s]"))
# Create array
for item in ${blacklisted[@]:0:${#blacklisted[@]}-1}; do
  echo -ne '\\"'$item'\\" << '
done
echo -ne '\\"'${blacklisted[-1]}'\\"'

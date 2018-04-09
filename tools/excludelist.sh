#!/bin/bash

# Download excludelist
blacklisted=($(wget --quiet https://raw.githubusercontent.com/probonopd/AppImages/master/excludelist -O - | sort | uniq | grep -v "^#.*" | grep "[^-\s]"))
if [ -z $blacklisted ]; then
    # Return nothing if no output from command
    echo ""
    exit
fi

# Create array
for item in ${blacklisted[@]:0:${#blacklisted[@]}-1}; do
  echo -ne '\\"'$item'\\" << '
done
echo -ne '\\"'${blacklisted[-1]}'\\"'

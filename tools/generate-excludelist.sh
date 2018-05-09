#!/bin/bash

set -e

# download excludelist
blacklisted=($(wget --quiet https://raw.githubusercontent.com/probonopd/AppImages/master/excludelist -O - | sort | uniq | grep -v "^#.*" | grep "[^-\s]"))

# sanity check
if [ "$blacklisted" == "" ]; then
    exit 1;
fi

filename=$(readlink -f $(dirname "$0"))/linuxdeployqt/excludelist.h

# overwrite existing source file
cat > "$filename" <<EOF
/*
 * List of libraries to exclude for different reasons.
 *
 * Automatically generated from
 * https://raw.githubusercontent.com/probonopd/AppImages/master/excludelist
 *
 * This file shall be committed by the developers occassionally,
 * otherwise systems without access to the internet won't be able to build
 * fully working versions of linuxdeployqt.
 *
 * See https://github.com/probonopd/linuxdeployqt/issues/274 for more
 * information.
 */

#include <QStringList>

static const QStringList generatedExcludelist = {
EOF

# Create array
for item in ${blacklisted[@]:0:${#blacklisted[@]}-1}; do
    echo -e '    "'"$item"'",' >> "$filename"
done
echo -e '    "'"${blacklisted[$((${#blacklisted[@]}-1))]}"'"' >> "$filename"

echo "};" >> "$filename"

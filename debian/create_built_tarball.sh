#!/bin/sh
# copyright (c) 2011-2014 Gregory Hainaut
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This package is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.



######################################################################
# Global Parameters
######################################################################
help()
{
    cat <<EOF
    Help:
    -rev <rev>     : revision number
    -branch <name> : branch name, take trunk otherwise
EOF

    exit 0
}

# Default value
GIT_SHA1=0;
BRANCH="master"
while [ -n "$1" ]; do
case $1 in
    -help|-h   ) help;shift 1;;
    -rev|-r    ) GIT_SHA1=$2; shift 2;;
    -branch|-b ) BRANCH=$2; shift 2;;
    --) shift;break;;
    -*) echo "ERROR: $1 option does not exists. Use -h for help";exit 1;;
    *)  break;;
esac
done

# Directory
TMP_DIR=/tmp/USBqemu-wheel_git
mkdir -p $TMP_DIR

REMOTE_REPO="https://github.com/jackun/USBqemu-wheel.git"
LOCAL_REPO="$TMP_DIR/USBqemu-wheel"


######################################################################
# Basic functions
######################################################################
date=
version=
release=
get_plugin_version()
{
    local major=`grep -o "PLUGIN_VERSION_MAJOR.*" $LOCAL_REPO/CMakeLists.txt | grep -o "[0-9]*"`
    local mid=`grep -o "PLUGIN_VERSION_MINOR.*" $LOCAL_REPO/CMakeLists.txt | grep -o "[0-9]*"`
    local minor=`grep -o "PLUGIN_VERSION_PATCH.*" $LOCAL_REPO/CMakeLists.txt | grep -o "[0-9]*"`
    release=0
    version="$major.$mid.$minor"
}

get_git_version()
{
    date=`git -C $LOCAL_REPO show -s --format=%ci HEAD | sed -e 's/[\:\-]//g' -e 's/ /./' -e 's/ .*//'`
}

download_orig()
{
    (cd $TMP_DIR && git clone --branch $1 $REMOTE_REPO USBqemu-wheel)
    if [ "$SVN_CO_VERSION" = "1" ] ; then
        (cd $TMP_DIR/USBqemu-wheel && git checkout $GIT_SHA1)
    fi
}

remove_dot_git()
{
    # To save 66% of the package size
    rm -fr  $LOCAL_REPO/.git
}

######################################################################
# Main script
######################################################################
download_orig $BRANCH

get_git_version
get_plugin_version

# must be done after getting the git version
remove_dot_git

# Debian name of package and tarball
if [ $release -eq 1 ]
then
    PKG_NAME="usbqemu-wheel-${version}"
    TAR_NAME="usbqemu-wheel_${version}.orig.tar"
else
    PKG_NAME="usbqemu-wheel.snapshot-${version}~git${date}"
    TAR_NAME="usbqemu-wheel.snapshot_${version}~git${date}.orig.tar"
fi


NEW_DIR=${TMP_DIR}/$PKG_NAME
rm -fr $NEW_DIR
mv $LOCAL_REPO $NEW_DIR

echo "Build the tar.gz file"
tar -C $TMP_DIR -cJf ${TAR_NAME}.xz $PKG_NAME

## Clean
rm -fr $TMP_DIR

exit 0

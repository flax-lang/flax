#!/bin/bash

CAPRI_VERSION=1.17
CAPRI_RELEASE_URL="http://91.206.143.88/capri/releases/"
echo "Getting Capri version $CAPRI_VERSION from $CAPRI_RELEASE_URL"

if [ `uname` = "Darwin" ]; then
	# special case, get 1.18 for OS X
	wget "$CAPRI_RELEASE_URL/1.18/capri-1.18.2-mac-x64.zip"
	mv capri-1.18.2-mac-x64.zip ./capri.zip
	unzip capri.zip
	rm capri.zip

elif [ `uname` = "Linux" ]; then
	wget "$CAPRI_RELEASE_URL/$CAPRI_VERSION/capri-1.17-linux-x64.tar.gz"
	mv capri-1.17-linux-x64.tar.gz ./capri.tar.gz
	tar -xf capri.tar.gz

	rm capri.tar.gz
fi



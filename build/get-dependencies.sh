#!/bin/bash

wget "http://capri.ghostkernel.org/downloads/capri-2.1.0-src.tar.gz"
tar -xf capri-2.1.0-src.tar.gz
cd capri-2.1.0-src/capri-cli
sudo bash build.sh all install

cd ..



#!/bin/bash

wget "http://capri.ghostkernel.org/downloads/capri-2.1.0-src.tar.gz"
tar -xf capri-2.1.0-src.tar.gz
cd capri-2.1.0-src
sudo sh build.sh all install

cd ..



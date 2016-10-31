#!/bin/bash

CLANGCXX=`which clang++`
CLANGC=`which clang`
echo $CLANGCXX
echo $CLANGC
echo `which clang++-3.7`
sudo mv $CLANGCXX /tmp/clang++-old
sudo mv $CLANGC /tmp/clang-old
sudo cp `which clang++-3.7` $CLANGCXX
sudo cp `which clang-3.7` $CLANGC
b2 -j2

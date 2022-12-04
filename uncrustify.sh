#!/bin/bash

set -ex

pushd `dirname $0`

if [ ! -e .uncrustify/build/uncrustify ]; then
	git clone https://github.com/uncrustify/uncrustify.git --branch uncrustify-0.76.0 .uncrustify
	cd .uncrustify
	mkdir build
	cd build
	cmake -DCMAKE_BUILD_TYPE=Release ..
	make
fi

popd

.uncrustify/build/uncrustify -c .uncrustify.cfg -l C --replace *.c *.h

#!/bin/bash

pushd . > '/dev/null';
SCRIPT_PATH="${BASH_SOURCE[0]:-$0}";
while [ -h "$SCRIPT_PATH" ];
do
    cd "$( dirname -- "$SCRIPT_PATH"; )";
    SCRIPT_PATH="$( readlink -f -- "$SCRIPT_PATH"; )";
done

cd "$( dirname -- "$SCRIPT_PATH"; )" > '/dev/null';
SCRIPT_PATH="$( pwd; )";

mkdir solution
cd solution

cmake -G "Unix Makefiles" ..
cmake --build . --config Debug
#cmake --build . --config Release
popd

#!/bin/bash

BASEPATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
export LD_LIBRARY_PATH="$BASEPATH/../../../build/lib:$LD_LIBRARY_PATH"

julia --project=$BASEPATH/.. -e "import Pkg; Pkg.test()"

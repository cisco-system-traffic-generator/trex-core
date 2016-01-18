#!/bin/bash

# if no variable of $PYTHON is define - we try to find it
function find_python {
    # two candidates - machine python and cisco linux python
    MACHINE_PYTHON=python
    CEL_PYTHON=/router/bin/python

    # try the machine python
    PYTHON=$MACHINE_PYTHON
    PCHECK=`$PYTHON -c "import sys; ver = sys.version_info[0] * 10 + sys.version_info[1];sys.exit(ver < 27)"`
    if [ $? -eq 0 ]; then
        return
    fi

    # try the CEL python
    PYTHON=$CEL_PYTHON
    PCHECK=`$PYTHON -c "import sys; ver = sys.version_info[0] * 10 + sys.version_info[1];sys.exit(ver < 27)"`
    if [ $? -eq 0 ]; then
        return
    fi

    echo "*** $PYTHON - python version is too old, 2.7 at least is required"
    exit -1
}

if [ -z "$PYTHON" ]; then
    find_python
fi


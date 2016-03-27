#!/bin/bash

# function finds python2
function find_python2 {
    # two candidates - machine python and cisco linux python
    MACHINE_PYTHON=python
    CEL_PYTHON=/router/bin/python-2.7.4

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

# function finds python3
function find_python3 {
    MACHINE_PYTHON=python3
    ITAY_PYTHON=/auto/proj-pcube-b/apps/PL-b/tools/python3.4/bin/python3
    PYTHON=$MACHINE_PYTHON
    PCHECK=`$PYTHON -c "import sys; ver = sys.version_info[0] * 10 + sys.version_info[1];sys.exit(ver != 34)"`
    if [ $? -eq 0 ]; then
        return
    fi
    PYTHON=$ITAY_PYTHON
    PCHECK=`$PYTHON -c "import sys; ver = sys.version_info[0] * 10 + sys.version_info[1];sys.exit(ver != 34)"`
    if [ $? -eq 0 ]; then
        return
    fi
    echo "*** $PYTHON - python version does not match, 3.4 is required"
    exit -1
}

case "$1" in
    "--python2") # we want python2
        find_python2
        ;;
    "--python3") # we want python3
        find_python3
        ;;
    *)
        if [ -z "$PYTHON" ]; then # no python env. var
            case $USER in
                imarom|hhaim|ybrustin|ibarnea) # dev users, 70% python3 30% python2
                    case $(($RANDOM % 10)) in
                        [7-9])
                            find_python2
                            ;;
                        *)
                            find_python3
                            ;;
                    esac
                    ;;
                *) # default is python2
                    find_python2
                    ;;
            esac
        fi
        ;;
esac



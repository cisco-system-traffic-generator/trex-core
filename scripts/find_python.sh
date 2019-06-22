#!/bin/bash +e

PYTHONS2_LOCAL=( python /usr/bin/python )
PYTHONS2=( ${PYTHONS2_LOCAL[*]} /router/bin/python-2.7.4 )

PYTHONS3_LOCAL=( python3 /usr/bin/python3 )
PYTHONS3=( ${PYTHONS3_LOCAL[*]} /auto/proj-pcube-b/apps/PL-b/tools/python3.4/bin/python3 )

# function finds python2
function find_python2 {

    if [ -n "$PYTHON" ]; then #  
        return;
    fi

    # try different Python paths
    PYTHONS="$@"
    for PYTHON in ${PYTHONS[*]}; do
        $PYTHON -c "import sys; ver = sys.version_info[0] * 10 + sys.version_info[1];sys.exit(ver < 27)" &> /dev/null
        if [ $? -eq 0 ]; then
            return
        fi
    done;

    echo "*** Python2 version is too old, 2.7 at least is required"
    exit -1
}

# function finds python3
function find_python3 {

    if [ -n "$PYTHON3" ]; then 
        PYTHON=$PYTHON3
        return;
    fi

    # try different Python3 paths
    PYTHONS="$@"
    for PYTHON in ${PYTHONS[*]}; do
        $PYTHON -c "import sys; ver = sys.version_info[0] * 10 + sys.version_info[1];sys.exit(ver < 33)" &> /dev/null
        if [ $? -eq 0 ]; then
            return
        fi
    done;

    echo "*** Python3 version is too old, 3.3 or later is required"
    exit -1
}

case "$1" in
    "--python2") # we want python2
        find_python2 ${PYTHONS2[*]}
        ;;
    "--python3") # we want python3
        find_python3 ${PYTHONS3[*]}
        ;;
    "--local") # any local, 50/50 try Python2 first
        case $(($RANDOM % 10)) in
            [5-9])
                (find_python2 ${PYTHONS2_LOCAL[*]}) &> /dev/null && find_python2 ${PYTHONS2_LOCAL[*]} && return
                (find_python3 ${PYTHONS3_LOCAL[*]}) &> /dev/null && find_python3 ${PYTHONS3_LOCAL[*]} && return
                echo "Python versions 2.7 or 3.3 at least required"
                exit -1
                ;;
            *)
                (find_python3 ${PYTHONS3_LOCAL[*]}) &> /dev/null && find_python3 ${PYTHONS3_LOCAL[*]} && return
                (find_python2 ${PYTHONS2_LOCAL[*]}) &> /dev/null && find_python2 ${PYTHONS2_LOCAL[*]} && return
                echo "Python versions 2.7 or 3.3 at least required"
                exit -1
                ;;
        esac
        ;;
    *)
        if [ -z "$PYTHON" ]; then # no python env. var
            case $USER in
                imarom|hhaim|ybrustin|ibarnea) # dev users, 70% python3 30% python2
                    case $(($RANDOM % 10)) in
                        [7-9])
                            find_python2 ${PYTHONS2[*]}
                            ;;
                        *)
                            find_python3 ${PYTHONS3[*]}
                            ;;
                    esac
                    ;;
                *) # default is find any
                    (find_python2 ${PYTHONS2[*]}) &> /dev/null && find_python2 ${PYTHONS2[*]} && return
                    (find_python3 ${PYTHONS3[*]}) &> /dev/null && find_python3 ${PYTHONS3[*]} && return
                    echo "Python versions 2.7 or 3.3 at least required"
                    exit -1
                    ;;
            esac
        fi
        ;;
esac



PYTHONS3_LOCAL=( python3 /usr/bin/python3 )
PYTHONS3=( ${PYTHONS3_LOCAL[*]} /auto/proj-pcube-b/apps/PL-b/tools/python3.4/bin/python3 )


# function finds python3
function find_python3 {

    if [ -n "$PYTHON3" ]; then 
        PYTHON=$PYTHON3
        return;
    fi

    # try different Python3 paths
    PYTHONS="$@"
    for PYTHON in ${PYTHONS[*]}; do
        $PYTHON -c "import sys; ver = sys.version_info[0] * 10 + sys.version_info[1];sys.exit(ver < 34)" &> /dev/null
        if [ $? -eq 0 ]; then
            return
        fi
    done;

    echo "*** Python3 version is too old, 3.4 or later is required"
    exit -1
}

find_python3 ${PYTHONS3[*]}
if [ $? -ne 0 ]; then
  echo "Python version 3.4 at least required"
  exit -1
fi

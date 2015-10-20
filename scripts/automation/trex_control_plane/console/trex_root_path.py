#!/router/bin/python

import os
import sys

def add_root_to_path ():
    """adds trex_control_plane root dir to script path, up to `depth` parent dirs"""
    root_dirname = 'trex_control_plane'
    file_path    = os.path.dirname(os.path.realpath(__file__))

    components = file_path.split(os.sep)
    sys.path.append( str.join(os.sep, components[:components.index(root_dirname)+1]) )
    return

add_root_to_path()

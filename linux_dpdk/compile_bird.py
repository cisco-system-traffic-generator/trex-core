# this script will download & compile bird moving exe file to wanted path
# and create a simple configuration file 

import os
import sys
import subprocess
import shlex
import errno
import time
import tempfile
import argparse
import shutil

with open('BIRD_VERSION', 'r') as f:
    BIRD_VER = f.read()

DEFAULT_CONFIG = """
router id 1.1.1.100;
protocol device {
    scan time 1;
}"""

# remove /router from path
def fix_path():
    path_arr = os.environ['PATH'].split(':')
    os.environ['PATH'] = ':'.join([path for path in path_arr if not path.startswith('/router')])


def run_command(command, timeout=60, poll_rate=0.1, cwd=None):

    try:  # Python 2
        stdout_file = tempfile.TemporaryFile(bufsize=0)
    except:  # Python 3
        stdout_file = tempfile.TemporaryFile(buffering=0)

    try:
        proc = subprocess.Popen(shlex.split(command), stdout=stdout_file,
                                stderr=subprocess.STDOUT, cwd=cwd, close_fds=True, universal_newlines=True)

        for _ in range(int(timeout/poll_rate)):
            time.sleep(poll_rate)
            if proc.poll() is not None: # process stopped
                break
        if proc.poll() is None:
            proc.kill()  # timeout
            stdout_file.seek(0)
            return (errno.ETIMEDOUT, '%s\n\n...Timeout of %s second(s) is reached!' % (stdout_file.read(), timeout))
        stdout_file.seek(0)
        return (proc.returncode, stdout_file.read())
    finally:
        stdout_file.close()

def build_bird(dst, is_verbose = False):
    fix_path()
    bird_down_link = "https://gitlab.labs.nic.cz/labs/bird/-/archive/v%s/bird-v%s.tar.gz" % (BIRD_VER, BIRD_VER)
    bird_down_path = "%s/bird-v%s" % (dst, BIRD_VER)

    # download bird
    code, output = run_command("wget -O %s/bird.tar.gz %s" % (dst, bird_down_link))
    if code != 0:
        raise Exception('cannot download bird link from "%s"' % bird_down_link)
    if is_verbose:
        print(output)

    # extract the files
    code, output = run_command("tar -zxf %s/bird.tar.gz -C %s" % (dst, dst))
    if code != 0:
        raise Exception('cannot extract the file: "%s/bird.tar.gz"' % dst)
    if is_verbose:
        print(output)

    # autoreconf
    code, output = run_command('autoreconf', cwd=bird_down_path)
    if code != 0:
        raise Exception('Error in "{0}" command, output: {1}'.format('autoreconf', output))
    if is_verbose:
        print(output)

    # configure
    code, output = run_command('./configure --disable-client', cwd=bird_down_path)
    if code != 0:
        raise Exception('Error in "{0}" command, output: {1}'.format('./configure --disable-client', output))
    if is_verbose:
        print(output)

    # make
    code, output = run_command('make', cwd=bird_down_path, timeout=180)
    if code != 0:
        raise Exception('Error in "{0}" command, output: {1}'.format('make', output))
    if is_verbose:
        print(output)

    # create default bird.conf
    with open('%s/bird.conf' % bird_down_path, 'w') as conf_file:
        conf_file.write(DEFAULT_CONFIG)

    # change executable name
    os.rename('%s/bird' % bird_down_path , '%s/trex_bird' % bird_down_path)

    # change permissions for bird files
    os.system('chmod 666 %s/bird.conf' % bird_down_path)
    os.system('chmod 555 %s/trex_bird' % bird_down_path)

    # rename bird folder to signify end of compilation
    dst_path = '%s/bird' % dst
    if os.path.exists(dst_path) and os.path.isdir(dst_path):
        shutil.rmtree(dst_path)  # cannot rename when there is another bird folder
    os.rename("%s/bird-v%s" % (dst, BIRD_VER), dst_path)
    if is_verbose:
        print('Moving bird to %s' % dst_path)

def compare_link(link_path, dst_path):
    return os.path.abspath(os.readlink(link_path)) == os.path.abspath(dst_path)

def do_create_link (src, name, where):
    '''
        creates a soft link
        'src'        - path to the source file
        'name'       - link name to be used
        'where'      - where to put the symbolic link
    '''
    # verify that source exists
    if os.path.exists(src):

        full_link = os.path.join(where, name)
        rel_path  = os.path.relpath(src, where)

        if os.path.islink(full_link):
            if compare_link(full_link, rel_path):
                return
            os.unlink(full_link)

        if not os.path.lexists(full_link):
            print('{0} --> {1}'.format(name, rel_path))
            os.symlink(rel_path, full_link)

def create_links(src_dir, links_dir):
    if not os.path.exists(links_dir):
        os.makedirs(links_dir)

    do_create_link(src    = src_dir + '/trex_bird',
                    name  = 'trex_bird',
                    where = links_dir)
    do_create_link(src    = src_dir + '/bird.conf',
                    name  = 'bird.conf',
                    where = links_dir)
    do_create_link(src    = src_dir + '/birdcl',
                    name  = 'birdcl',
                    where = links_dir)

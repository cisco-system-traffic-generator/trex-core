import sys, os

cur_dir = os.path.dirname(__file__)


try: # example is being run as "python -m trex.examples.astf.<example>"
    import trex.astf.api
except: # run as standalone script "python <example>"
    trex_path = os.path.join(cur_dir, os.pardir, os.pardir, os.pardir)
    sys.path.insert(0, os.path.abspath(trex_path))


try: # our usual repo
    pars = [os.pardir] * 6
    scripts_path = os.path.abspath(os.path.join(cur_dir, *pars))
    EXT_LIBS_PATH = os.path.join(scripts_path, 'external_libs')
    assert os.path.isdir(EXT_LIBS_PATH)
except: # in client package
    pars = [os.pardir] * 5
    scripts_path = os.path.abspath(os.path.join(cur_dir, *pars))
    EXT_LIBS_PATH = os.path.abspath(os.path.join(cur_dir, os.pardir, os.pardir, os.pardir, os.pardir, 'external_libs'))
    assert os.path.isdir(EXT_LIBS_PATH), 'Could not determine external_libs path'


def get_profiles_path():
    path = os.path.join(scripts_path, 'astf')
    if os.path.isdir(path):
        return path
    raise Exception('Could not determine path to ASTF profiles')


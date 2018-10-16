import sys, os

cur_dir = os.path.dirname(__file__)


try: # example is being run as "python -m trex.examples.astf.<example>"
    import trex.astf.api
except: # run as standalone script "python <example>"
    trex_path = os.path.join(cur_dir, os.pardir, os.pardir, os.pardir)
    sys.path.insert(0, os.path.abspath(trex_path))


try: # our usual repo
    scripts_path = os.path.abspath(os.path.join(cur_dir, os.pardir, os.pardir, os.pardir, os.pardir, os.pardir, os.pardir))
    ASTF_PROFILES_PATH = os.path.join(scripts_path, 'astf')
    EXT_LIBS_PATH = os.path.join(scripts_path, 'external_libs')
    assert os.path.isdir(ASTF_PROFILES_PATH)
    assert os.path.isdir(EXT_LIBS_PATH)
except: # in client package
    ASTF_PROFILES_PATH = os.path.abspath(os.path.join(cur_dir, os.pardir, os.pardir, os.pardir, 'profiles', 'astf'))
    EXT_LIBS_PATH = os.path.join(cur_dir, os.pardir, os.pardir, os.pardir, os.pardir, 'external_libs') # client package path

assert os.path.isdir(ASTF_PROFILES_PATH), 'Could not determine ASTF profiles path'
assert os.path.isdir(EXT_LIBS_PATH), 'Could not determine external_libs path'


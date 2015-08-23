# encoding: utf-8

"""dpkt documentation build configuration file"""

from __future__ import unicode_literals

import os
import re
import sys
import types
import mock


def get_version(filename):
    init_py = open(filename).read()
    metadata = dict(re.findall("__([a-z]+)__ = '([^']+)'", init_py))
    return metadata['version']


# -- Workarounds to have autodoc generate API docs ----------------------------

sys.path.insert(0, os.path.abspath('..'))


# Mock any objects that we might need to
foo = mock.Mock()
foo.__version__ = '0.1.1'
sys.modules['foo'] = foo


# -- General configuration ----------------------------------------------------
needs_sphinx = '1.0'
extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.extlinks',
    'sphinx.ext.intersphinx',
    'sphinx.ext.viewcode',
    'sphinx.ext.autosummary',
    'sphinxcontrib.napoleon'
]

templates_path = ['_templates']
source_suffix = '.rst'
master_doc = 'index'

project = 'dpkt'
copyright = '2009-2015 Dug Song and contributors'

release = get_version('../dpkt/__init__.py')
version = '.'.join(release.split('.')[:2])

exclude_patterns = ['_build']

pygments_style = 'sphinx'

modindex_common_prefix = ['dpkt.']

autodoc_default_flags = ['members', 'undoc-members', 'show-inheritance']
autodoc_member_order = 'bysource'


# -- Options for HTML output --------------------------------------------------
html_theme = 'default'
html_static_path = ['_static']

html_use_modindex = True
html_use_index = True
html_split_index = False
html_show_sourcelink = True

htmlhelp_basename = 'dpkt'

# -- Options for extlink extension --------------------------------------------
extlinks = {
    'issue': ('https://github.com/kbandla/dpkt/issues/%s', '#'),
}

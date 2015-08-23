import os
import sys

try:
    from setuptools import setup, Command
except ImportError:
    from distutils.core import setup, Command

package_name = 'dpkt'
description = 'fast, simple packet creation / parsing, with definitions for the basic TCP/IP protocols'
readme = open('README.rst').read()
requirements = []

# PyPI Readme
long_description = open('README.rst').read()

# Pull in the package
package = __import__(package_name)


class BuildDebPackage(Command):
    """Build a Debian Package out of this Python Package"""
    try:
        import stdeb
    except ImportError as exc:
        print 'To build a Debian Package you must install stdeb (pip install stdeb)'

    description = "Build a Debian Package out of this Python Package"
    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        os.system('python setup.py sdist')
        sdist_file = os.path.join(os.path.join(os.path.dirname(os.path.realpath(__file__)), 'dist'),
                                  package_name + '-' + package.__version__ + '.tar.gz')
        print sdist_file
        os.system('py2dsc-deb ' + sdist_file)


setup(name=package_name,
      version=package.__version__,
      author=package.__author__,
      author_email=package.__author_email__,
      url=package.__url__,
      description=description,
      long_description=long_description,
      packages=['dpkt'],
      install_requires=requirements,
      license='BSD',
      zip_safe=False,
      classifiers=[
          'Development Status :: 4 - Beta',
          'Intended Audience :: Developers',
          'License :: OSI Approved :: BSD License',
          'Natural Language :: English',
          'Programming Language :: Python :: 2.6',
          'Programming Language :: Python :: 2.7',
          'Programming Language :: Python :: Implementation :: CPython',
          'Programming Language :: Python :: Implementation :: PyPy',
      ],
      cmdclass={'debian': BuildDebPackage}
)

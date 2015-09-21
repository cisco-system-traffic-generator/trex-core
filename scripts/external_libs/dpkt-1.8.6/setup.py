import os
import sys

try:
    from setuptools import setup, Command
except ImportError:
    from distutils.core import setup, Command

package_name = 'dpkt'
description = 'fast, simple packet creation / parsing, with definitions for the basic TCP/IP protocols'
readme = open('README.rst').read()
requirements = [ ]

# PyPI Readme
long_description = open('README.rst').read()

# Pull in the package
package = __import__(package_name)

setup(name=package_name,
    version=package.__version__,
    author=package.__author__,
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
    ]
)

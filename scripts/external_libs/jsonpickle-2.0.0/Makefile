#!/usr/bin/env make

# External commands
BLACK ?= black
CTAGS ?= ctags
FIND ?= find
PYTHON ?= python
PYTEST ?= $(PYTHON) -m pytest
RM_R ?= rm -fr
SH ?= sh
TOX ?= tox
# Detect macOS to customize how we query the cpu count.
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo unknown')
ifeq ($(uname_S),Darwin)
    NPROC ?= sysctl -n hw.activecpu
else
    NPROC ?= nproc
endif

# Options
flags ?=
timeout ?= 600

# Default job count -- this is used if "-j" is not present in MAKEFLAGS.
nproc := $(shell sh -c '$(NPROC) 2>/dev/null || echo 4')

# Extract the "-j#" flags in $(MAKEFLAGS) so that we can forward the value to
# other commands.  This can be empty.
JOB_FLAGS := $(shell echo -- $(MAKEFLAGS) | grep -o -e '-j[0-9]\+' | head -n 1)
# Extract just the number from "-j#".
JOB_COUNT := $(shell printf '%s' "$(JOB_FLAGS)" | sed -e 's/-j//')
# We have "-jX" from MAKEFLAGS but tox wants "--parallel X"
DASH_J := $(shell echo -- $(JOB_FLAGS) -j$(nproc) | grep -o -e '-j[0-9]\+' | head -n 1)
NUM_JOBS := $(shell printf %s "$(DASH_J)" | sed -e 's/-j//')

TESTCMD ?= $(PYTEST)
TOXCMD ?= $(TOX)
TOXCMD += --develop --skip-missing-interpreters
ifdef multi
    TOXCMD += --parallel $(NUM_JOBS)
    TOXCMD += -e
    TOXCMD += 'clean,py{27,36,37,38,39},py{27,38,39}-sa{10,11,12,13},py{27,38,39}-libs'
    # Disable coverage when running in parallel
    TOXCMD += -- --no-cov
endif
ifdef V
    TESTCMD += --verbose
    TOXCMD += -v
endif

# Coverage
COVERAGE_ENV ?= py39

# Data
ARTIFACTS := build
ARTIFACTS += dist
ARTIFACTS += __pycache__
ARTIFACTS += tags
ARTIFACTS += *.egg-info

PYTHON_DIRS := tests
PYTHON_DIRS += jsonpickle

# The default target of this makefile is....
all:: help

help:
	@echo "---- Makefile Targets ----"
	@echo "make help            - print this message"
	@echo "make test            - run unit tests"
	@echo "make tox		        - run unit tests using tox"
	@echo "make tox multi=1     - run unit tests on multiple pythons using tox"
	@echo "make clean           - remove cruft"
.PHONY: help

test:
	$(TESTCMD) $(flags)
.PHONY: test

tox:
	$(TOXCMD) $(flags)
.PHONY: tox

tags:
	$(FIND) $(PYTHON_DIRS) -name '*.py' -print0 | xargs -0 $(CTAGS) -f tags

clean:
	$(FIND) $(PYTHON_DIRS) -name '*.py[cod]' -print0 | xargs -0 rm -f
	$(FIND) $(PYTHON_DIRS) -name '__pycache__' -print0 | xargs -0 rm -fr
	$(RM_R) $(ARTIFACTS)
.PHONY: clean

format::
	$(BLACK) --skip-string-normalization --target-version py27 $(PYTHON_DIRS)
.PHONY: format

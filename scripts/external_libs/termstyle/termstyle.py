#!/usr/bin/env python
# Copyright (c) 2009, Tim Cuthbertson 
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of the organisation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
# WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

import sys

class Style(object):
	prefix='\x1b['
	suffix='m'
	enabled = True
	
	def __init__(self, on_codes, off_codes = 0):
		self._on = self.sequence(on_codes)
		self._off = self.sequence(off_codes)
	
	def _get_on(self): return self._on if self.enabled else ''
	def _get_off(self): return self._off if self.enabled else ''
	on = property(_get_on)
	off = property(_get_off)
		
	@classmethod
	def sequence(cls, codes):
		wrap_single = lambda code: "%s%s%s" % (cls.prefix, code, cls.suffix)
		try:
			return ''.join([wrap_single(code) for code in codes])
		except TypeError:
			return wrap_single(codes)
			
	def __str__(self):
		if not self.enabled:
			return ''
		return self.on
	
	def __call__(self, *args):
		contents = ''.join(["%s%s" % (self.on, arg) for arg in args])
		return "%s%s" % (contents, self.off)
		

def auto():
	"""set colouring on if STDOUT is a terminal device, off otherwise"""
	try:
		Style.enabled = False
		Style.enabled = sys.stdout.isatty()
	except (AttributeError, TypeError):
		pass

def enable():
	"""force coloured output"""
	Style.enabled = True

def disable():
	"""disable coloured output"""
	Style.enabled = False
	
default = reset = Style(0)

black = Style(30)
red = Style(31)
green = Style(32)
yellow = Style(33)
blue = Style(34)
magenta = Style(35)
cyan = Style(36)
white = Style(37)

bg_black = Style(40)
bg_red = Style(41)
bg_green = Style(42)
bg_yellow = Style(43)
bg_blue = Style(44)
bg_magenta = Style(45)
bg_cyan = Style(46)
bg_white = Style(47)
bg_default = Style(49)

bold = Style(1)
underscore = Style(4)
inverted = Style(7)
italic = Style(3)

auto()

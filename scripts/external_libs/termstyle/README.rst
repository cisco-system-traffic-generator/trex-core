=========
termstyle
=========

termstyle is a simple python library for adding coloured output to
terminal (console) programs.  The definitions come from ECMA-048_, the
"Control Functions for Coded Character Sets" standard.

Installation:
-------------

I thoroughly recommend using the zero-install feed (see the project homepage) to manage your dependencies if at all possible. zero-install_ provides a much better system than pip or easy_install, and works with absolutely any language and allows decentralised package management that requires no special privileges to install.

Example Usage:
--------------
::

	from termstyle import *
	print "%s:%s" % (red('Hey'), green('how are you?'))
	print blue('How ', bold('you'), ' doin?')

or, you can use a colour just as a string::

	print "%sBlue!%s" % (blue, reset)

Styles:
-------
::

	reset or default (no colour / style)

colour::

	black
	red
	green
	yellow
	blue
	magenta
	cyan
	white

background colour::

	bg_black
	bg_red
	bg_green
	bg_yellow
	bg_blue
	bg_magenta
	bg_cyan
	bg_white
	bg_default

In terminals supporting transparency ``bg_default`` is often used to set
the background to transparent [#]_.

weight::

	bold
	inverted

style::

	italic
	underscore

Controls:
---------
::

	auto() - sets colouring on only if sys.stdout is a terminal
	disabe() - disable colours
	enable() - enable colours

.. [#] Supporting terminals include rxvt-unicode_, and Eterm_.

.. _ECMA-048: http://www.ecma-international.org/publications/files/ECMA-ST/Ecma-048.pdf
.. _rxvt-unicode: http://software.schmorp.de/
.. _Eterm: http://www.eterm.org/
.. _zero-install: http://0install.net/


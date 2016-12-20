# Asciidoc-deck.js

A Deck.js backend for asciidoc. 


## Dependencies

* AsciiDoc
* Deck.js (included in the backend package)

Optional:

* If you want to highlight source code, please install Pygments or source-highlight.


## Installation

Download the [backend package][deckjs] and use asciidoc to install:

~~~~.bash
asciidoc --backend install deckjs-X.Y.Z.zip
~~~~

This will install the backend to `~/.asciidoc/backend/deckjs`.

You can also use this backend without installation, see the next section.


## Usage

* With deckjs backend installed, use following command to generate slides:

~~~~.bash
asciidoc -b deckjs file.asciidoc
~~~~

By default, the `linkcss` option is disabled so all the required js and css file will be embedded into the output slide. Checkout the [template file][example] for how to enable all kinds of options.

* To use without Installation, you need to specify different argument:

~~~~.bash
asciidoc -f deckjs.conf file.asciidoc
~~~~

Make sure your asciidoc can properly find `deckjs.conf`. For asciidoc's configuration file loading strategy, please refer to [this guide][asc-conf-guide].

Note that without installation, you also have to enable `linkcss` option. Then put `deck.js`, `ad-stylesheet` and generated slide into the same directory. 



[deckjs]:https://github.com/houqp/asciidoc-deckjs/downloads
[deckjs-ext]:https://github.com/downloads/houqp/asciidoc-deckjs/deck.js.extended.zip
[asc-conf-guide]:http://www.methods.co.nz/asciidoc/userguide.html#X27
[example]:http://houqp.github.com/asciidoc-deckjs/example-template.asciidoc


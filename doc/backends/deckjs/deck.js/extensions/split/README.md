
deck.split.js
==============

`deck.split.js` is a deckjs extension that helps you split long slide into
multiple small slides.


Installation 
------------

Just drop the whole repo into deck.js's `extensions` directory, then include
the script in your slide.


Usage
-----

To split a long slide, you need to indicate position inside original slide by
adding following line:

```html
<div style="page-break-after:always"></div>
```

Indeed, it could be any tag set with `page-break-after:always` style. Every
time you add this line, a new slide with the same title will be created, so you
can split your slide into pieces as much as you want.



/*!
Deck JS - deck.toc
Copyright (c) 2011 Remi BARRAQUAND
Dual licensed under the MIT license and GPL license.
https://github.com/imakewebthings/deck.js/blob/master/MIT-license.txt
https://github.com/imakewebthings/deck.js/blob/master/GPL-license.txt
*/

/*
This module provides a support for TOC to the deck.
*/

(function($, deck, undefined) {
    var $d = $(document);
    var $toc;
        
    /*
	Extends defaults/options.
	
        options.classes.toc
		This class is added to the deck container when showing the slide
                toc.
	
	options.keys.toc
		The numeric keycode used to toggle between showing and hiding 
                the slide toc.
    
	options.selectors.toc
		The element matching this selector displays the toc.
    
	options.selectors.tocTitle
		The element matching this selector displays the current title
                of the slide i.e the current h1.

        options.selectors.tocSection
		The element matching this selector displays the current section
                of the slide i.e the current h2.
	
        options.selectors.tocSubSection
		The element matching this selector displays the current 
                subsection of the slide i.e the current h3.
    
        options.selectors.tocSubSubSection
		The element matching this selector displays the current 
                subsubsection of the slide i.e the current h4.
	*/
    $.extend(true, $[deck].defaults, {
        classes: {
            toc: 'deck-toc-frame'
        },
		
        keys: {
            toc: 84 // t
        },
                
        selectors: {
            toc: '.deck-toc',
            tocTitle: '.deck-toc-h1',
            tocSection: '.deck-toc-h2',
            tocSubSection: '.deck-toc-h3',
            tocSubSubSection: '.deck-toc-h4',
            tocStatus: '.deck-toc-status'
        }
    });

    /*
	jQuery.deck('showToc')
	
	Shows the slide toc by adding the class specified by the toc class option
	to the deck container.
	*/
    $[deck]('extend', 'showToc', function() {
        $[deck]('getContainer').addClass($[deck]('getOptions').classes.toc);
    });
	
    /*
	jQuery.deck('hideToc')
	
	Hides the slide toc by removing the class specified by the toc class
	option from the deck container.
	*/
    $[deck]('extend', 'hideToc', function() {
        $[deck]('getContainer').removeClass($[deck]('getOptions').classes.toc);
    });

    /*
	jQuery.deck('toggleToc')
	
	Toggles between showing and hiding the TOC.
	*/
    $[deck]('extend', 'toggleToc', function() {
        $[deck]('getContainer').hasClass($[deck]('getOptions').classes.toc) ? 
        $[deck]('hideToc') : $[deck]('showToc');
    });
        
    /*
        jQuery.deck('Init')
        */
    $d.bind('deck.init', function() {
        var opts = $[deck]('getOptions');
        var container = $[deck]('getContainer');
        
        /* Bind key events */
        $d.unbind('keydown.decktoc').bind('keydown.decktoc', function(e) {
            if (e.which === opts.keys.toc || $.inArray(e.which, opts.keys.toc) > -1) {
                $[deck]('toggleToc');
                e.preventDefault();
            }
        });
        
        /* Hide TOC panel when user click on container */
        container.click(function(e){
            $[deck]('hideToc');
        });
                
        /* Init TOC and append it to the document */
        $toc = new TOC();
        $($[deck]('getOptions').selectors.toc).append($toc.root);
                
        /* Go through all slides */
        $.each($[deck]('getSlides'), function(i, $el) {
            var slide = $[deck]('getSlide',i);
            //var tocElementFound = false;
            
            /* If there is a toc item, push it in the TOC */
            for(var level=1; level<6; level++) {
                if( slide.find("h"+level).length > 0) {
                    var tocTitle = "";
                    var $tocElement = slide.find("h"+level+":first");
                    if( $tocElement.attr("title") != undefined && $tocElement.attr("title") != "") {
                        tocTitle = $tocElement.attr("title");
                    } else {
                        tocTitle = $tocElement.text();
                    }
                    $toc.push(level, tocTitle, slide);
                    $toc.tag(slide);
                    //tocElementFound = true;
                }
            }
            
            /* Tag the slide with the current TOC level */
            $toc.tag(slide);
        });
    })
    /* Update current slide number with each change event */
    .bind('deck.change', function(e, from, to) {
        var opts = $[deck]('getOptions');
        var slideTo = $[deck]('getSlide', to);
        var container = $[deck]('getContainer');
		
        if (container.hasClass($[deck]('getOptions').classes.toc)) {
            container.scrollTop(slideTo.offset().top);
        }
            
        /* update toc status */
        if( slideTo.data("toc") ) {
            // reset
            $(opts.selectors.tocTitle).text("");
            $(opts.selectors.tocSection).text("");
            $(opts.selectors.tocSubSection).text("");
            $(opts.selectors.tocSubSubSection).text("");

            if( slideTo.hasClass('hide-toc-status') ) {
                $(opts.selectors.tocStatus).hide();
            } else {
                $(opts.selectors.tocStatus).show();
                // update according to the current context
                var $context = $toc.context(slideTo.data('toc'))            
                for(var level=1; level<=$context.length; level++) {
                    switch(level) {
                        case 1: 
                            $(opts.selectors.tocTitle).text($context[level-1]);
                            break;
                        case 2: 
                            $(opts.selectors.tocSection).text($context[level-1]);
                            break;
                        case 3: 
                            $(opts.selectors.tocSubSection).text($context[level-1]); 
                            break;
                        case 4: 
                            $(opts.selectors.tocSubSubSection).text($context[level-1]); 
                            break;
                    }
                }
            } 
        }
    });
        
    /*
        Simple TOC manager (must be improved)
        */
    var TOC = function() {
        
        this.root = $("<ul/>", {"class":"toc"});
            
        /* 
            Push new item in the TOC 
          
            depth is the level (e.g. 1 for h1, 2 for h2, etc.)
            title is the toc-item title
            slide is the slide that provides the toc-element
            */
        this.push = function(depth,title,slide) {
            inc(depth);
                
            /* Create toc element */
            var $tocElm = $("<li />", {
                id: "toc-"+($c.join('-'))
            }).data({ // keep track of the slide in case...
                slide: slide,
                title: title
            }).append($("<a />", { // create an hyperlink
                href: "#"+$(slide).attr('id'),
                text: title
            })).append($("<ul />"));
                                
            /* insert it at the right place */
            var $target = this.root;
            if( depth > 1) {
                $target = ($target.find("li#toc-"+($c.slice(0,$c.length-1).join('-')))).children("ul");
            }
            $tocElm.appendTo($target);
        };
        
        /*
            Tag the slide with the current TOC level.
        
            slide is the slide to tag
            */
        this.tag = function(slide) {
            slide.data({
                toc: $c.slice(0)
            });
        }
        
        /*
            Get the current TOC context
        
            path is the current path in the TOC
            */
        this.context = function(path) {
            $context = new Array();
            var $target = this.root;
            for(var depth=0; depth<path.length; depth++) {
                var tocElm = $target.find("li#toc-"+(path.slice(0,depth+1).join('-')))
                $context.push(tocElm.data('title'));
                $target = (tocElm).children("ul");
            }
            
            return $context;
        }
            
        /* cursor */
        var $c = [-1];
        function inc(depth) {
            var current_depth = $c.length;
            if(depth>current_depth) {
                for(i=current_depth;i<depth;i++) {
                    $c.push(0);
                }
            } else if( current_depth>depth) {
                for(i=depth;i<current_depth;i++) {
                    $c.pop();
                    $c[depth-1]++
                }
            } else {
                $c[depth-1]++
            }
        }
    }
})(jQuery, 'deck');
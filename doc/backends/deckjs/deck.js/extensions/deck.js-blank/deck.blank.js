/*!
Deck JS - deck.blank - v1.0
Copyright (c) 2012 Mike Kellenberger
*/

/*
This module adds the necessary methods and key bindings to blank/unblank the screen by pressing 'b'.
*/
(function($, deck, undefined) {
	var $d = $(document);

    $[deck]('extend', 'activateBlankScreen', function() {
        $[deck]('getSlide').hide();
    });
    
    $[deck]('extend', 'deactivateBlankScreen', function() {
        $[deck]('getSlide').show();
    });

	$[deck]('extend', 'blankScreen', function() {
        $[deck]('getSlide').is(":visible") ? $[deck]('activateBlankScreen') : $[deck]('deactivateBlankScreen');
    });

	$d.bind('deck.init', function() {
		// Bind key events
		$d.unbind('keydown.blank').bind('keydown.blank', function(e) {
			if (e.which==66) {
				$[deck]('blankScreen');
				e.preventDefault();
			}
		});
	});
})(jQuery, 'deck');
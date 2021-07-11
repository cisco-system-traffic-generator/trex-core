define( function () { 
    // each can define -- flatten; restore; post_restore. Note that if the object has sub objects
    //   in restore, then they MUST be restored just to keep the object count valid. No skipping...
    var handlers = {
      'fractions.Fraction': {
          restore: function(obj) {
              return obj._numerator / obj._denominator;
          }
      },   
    };
    
    if (typeof jsonpickle != 'undefined') {
        jsonpickle.handlers = handlers;
    }
    return handlers;
});
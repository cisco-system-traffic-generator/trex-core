define(function () {
    var tags = {
            ID: 'py/id',
            OBJECT: 'py/object',
            TYPE: 'py/type',
            REPR: 'py/repr',
            REF: 'py/ref',
            TUPLE: 'py/tuple',
            SET: 'py/set',
            SEQ: 'py/seq',
            STATE: 'py/state',
            JSON_KEY: 'json://',            
    };
    tags.RESERVED = [tags.ID, tags.OBJECT, 
                          tags.TYPE, tags.REPR, 
                          tags.REF, tags.TUPLE, 
                          tags.SET, tags.SEQ, 
                          tags.STATE, tags.JSON_KEY];
    tags.PY_CLASS = '_py_class';
    
    if (typeof jsonpickle != 'undefined') {
        jsonpickle.tags = tags;
    }
    return tags;
});
/**
 * jsonpickleJS -- interpretation of python jsonpickle in Javascript
 * main.js -- main loader -- this should be the only file that most users care about.
 * 
 * Copyright (c) 2014 Michael Scott Cuthbert and cuthbertLab
 */

if (typeof jsonpickle == 'undefined') {
    jsonpickle = {}; // global for now...
}

define(['./unpickler', './pickler', './util', './tags', './handlers'], 
        function(unpickler, pickler, util, tags, handlers) { 
    jsonpickle.pickler = pickler;
    jsonpickle.unpickler = unpickler;
    jsonpickle.util = util;
    jsonpickle.tags = tags;
    jsonpickle.handlers = handlers;

    jsonpickle.encode = function (value, unpicklable, make_refs,
            keys, max_depth, backend) {
        if (unpicklable === undefined) {
            unpicklable = true;
        }
        if (make_refs === undefined) {
            make_refs = true;
        }
        if (keys === undefined) {
            keys = false;
        }
        var options = {
            unpicklable: unpicklable,
            make_refs: make_refs,
            keys: keys,
            max_depth: max_depth,
            backend: backend,
        };
        return pickler.encode(value, options);
    };
    
    
    jsonpickle.decode = function (string, backend, keys) {
        if (keys === undefined) {
            keys = false;
        }
        return unpickler.decode(string, backend, keys);
    };
    return jsonpickle;
});
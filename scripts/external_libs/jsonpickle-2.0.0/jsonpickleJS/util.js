define(['./tags'], function(tags) { 
    var util = {};
    util.merge = function(destination, source) {
        if (source !== undefined) {
            for (var property in source) {
                destination[property] = source[property];
            }            
        }
        return destination;
    };
    
    util.PRIMITIVES = ['string', 'number', 'boolean'];

    /* from jsonpickle.util */
    // to_do...
    util.is_type = function (obj) { return false; };

    // same as dictionary in JS...
    util.is_object = function (obj) { return util.is_dictionary(obj); };
    
    util.is_primitive = function (obj) {
        if (obj === undefined || obj == null) {
            return true;
        }
        if (util.PRIMITIVES.indexOf(typeof obj) != -1) {
            return true;
        }
        return false;
    };

    
    util.is_dictionary = function (obj) {
        return ((typeof obj == 'object') && (obj !== null));
    };

    util.is_sequence = function (obj) {
        return (util.is_list(obj) || util.is_set(obj) || util.is_tuple(obj));
    };
    
    util.is_list = function (obj) {
        return (obj instanceof Array);
    };
    
    // to_do...
    util.is_set = function (obj) { return false; };
    util.is_tuple = function (obj) { return false; };
    util.is_dictionary_subclass = function (obj) { return false; };
    util.is_sequence_subclass = function (obj) { return false; };
    util.is_noncomplex = function (obj) { return false; };
    
    util.is_function = function (obj) {
        return (typeof obj == 'function');
    };
    util.is_module = function (obj) { return false; };

    util.is_picklable = function(name, value) {
        if (tags.RESERVED.indexOf(name) != -1) {
            return false;
        }
        if (util.is_function(value)) {
            return false;
        } else {
            return true;
        }
    };
    util.is_installed = function (module) {
        return true; // ???
    };
    
    util.is_list_like = function (obj) {
        return util.is_list(obj);
    };
    
    if (typeof jsonpickle != "undefined") {
        jsonpickle.util = util;
    }
    return util; 
});
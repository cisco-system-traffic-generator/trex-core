/**
 * jsonPickle/javascript/unpickler -- Conversion from music21p jsonpickle streams
 *
 * Copyright (c) 2013-14, Michael Scott Cuthbert and cuthbertLab
 * Based on music21 (=music21p), Copyright (c) 2006–14, Michael Scott Cuthbert and cuthbertLab
 * 
 * usage:
 * 
 * js_obj = unpickler.decode(json_string);
 * 
 * 
 */

define(['./util', './handlers', './tags'], function(util, handlers, tags) {    
    var unpickler = {};
    
    unpickler.decode = function (string, user_handlers, options) {
        var params = {
          keys: false,
          safe: false,
          reset: true,
          backend: JSON,          
        };
                
        util.merge(params, options);                

        var use_handlers = {};
        util.merge(use_handlers, handlers); // don't screw up default handlers...
        util.merge(use_handlers, user_handlers);
        
        // backend does not do anything -- it is there for
        // compat with py-JSON-Pickle        
        if (params.context === undefined) {
            var unpickler_options = {
                keys: params.keys,
                backend: params.backend,
                safe: params.safe,
            };
            context = new unpickler.Unpickler(unpickler_options, use_handlers);
        }
        var jsonObj = params.backend.parse(string);
        return context.restore(jsonObj, params.reset);
    };

    unpickler.Unpickler = function (options, handlers) {
        var params = {
            keys: false,
            safe: false,
        };
        util.merge(params, options);
        this.keys = params.keys;
        this.safe = params.safe;
        
        this.handlers = handlers;
        //obsolete...
        //this._namedict = {};
        
        // The namestack grows whenever we recurse into a child object
        this._namestack = [];
        
        // Maps objects to their index in the _objs list
        this._obj_to_idx = {};
        this._objs = [];
    };
    
    unpickler.Unpickler.prototype.reset = function () {
        //this._namedict = {};
        this._namestack = [];
        this._obj_to_idx = {};
        this._objs = [];
    };
    
    /**
     * Restores a flattened object to a JavaScript representation
     * as close to the original python object as possible.
     * 
     * Requires that javascript 
     */
    unpickler.Unpickler.prototype.restore = function (obj, reset) {
        if (reset) {
            this.reset();
        }
        return this._restore(obj);
    };
    
    unpickler.Unpickler.prototype._restore = function (obj) {
        var has_tag = unpickler.has_tag;
        var restore = undefined;
        
        if (has_tag(obj, tags.ID)) {
            restore = this._restore_id.bind(this);
        } else if (has_tag(obj, tags.REF)) {
            // backwards compat. not supported
        } else if (has_tag(obj, tags.TYPE)) {
            restore = this._restore_type.bind(this);
        } else if (has_tag(obj, tags.REPR)) {
            // backwards compat. not supported
        } else if (has_tag(obj, tags.OBJECT)) {
            restore = this._restore_object.bind(this);
        } else if (has_tag(obj, tags.TUPLE)) {
            restore = this._restore_tuple.bind(this);            
        } else if (has_tag(obj, tags.SET)) {
            restore = this._restore_set.bind(this);
        } else if (util.is_list(obj)) {
            restore = this._restore_list.bind(this);
        } else if (util.is_dictionary(obj)) {
            restore = this._restore_dict.bind(this);
        } else {
            restore = function (obj) { return obj; };
        }
        return restore(obj);
    };
    
    unpickler.Unpickler.prototype._restore_id = function (obj) {
        return this._objs[obj[tags.ID]];
    };

    unpickler.Unpickler.prototype._restore_type = function (obj) {
        var typeref = unpickler.loadclass(obj[tags.TYPE]);
        if (typeref === undefined) {
            return obj;
        } else {
            return typeref;
        }
    };
    
    unpickler.Unpickler.prototype._restore_object = function (obj) {
        var class_name = obj[tags.OBJECT];
        var handler = this.handlers[class_name];
        if (handler !== undefined && handler.restore !== undefined) {
            var instance = handler.restore(obj);
            instance[tags.PY_CLASS] = class_name;
            return this._mkref(instance);
        } else {
            var cls = unpickler.loadclass(class_name);
            if (cls === undefined) {
                obj[tags.PY_CLASS] = class_name;
                return this._mkref(obj);
            }
            var instance = this._restore_object_instance(obj, cls);
            instance[tags.PY_CLASS] = class_name;
            if (handler !== undefined && handler.post_restore !== undefined) {
                return handler.post_restore(instance);
            } else {                
                return instance;
            }
        }
    };
    unpickler.Unpickler.prototype._loadfactory = function (obj) {
        var default_factory = obj['default_factory'];
        if (default_factory === undefined) {
            return undefined;
        } else {
            obj['default_factory'] = undefined;
            return this._restore(default_factory);
        }
    };
    
    
    unpickler.Unpickler.prototype._restore_object_instance = function (obj, cls) {
        //var factory = this._loadfactory(obj);
        var args = unpickler.getargs(obj);
        if (args.length > 0) {
            args = this._restore(args);
        }
        // not using factory... does not seem to apply to JS
        var instance = unpickler.construct(cls, args);
        this._mkref(instance);
        return this._restore_object_instance_variables(obj, instance);
    };
    
    unpickler.Unpickler.prototype._restore_object_instance_variables = function (obj, instance) {
        var has_tag = unpickler.has_tag;
        var restore_key = this._restore_key_fn();
        var keys = [];
        for (var k in obj) {
            if (obj.hasOwnProperty(k)) {
                keys.push(k);
            }
        }
        keys.sort();
        for (var i = 0; i < keys.length; i++) {
            var k = keys[i];
            if (tags.RESERVED.indexOf(k) != -1) {
                continue;
            }
            var v = obj[k];
            this._namestack.push(k);
            k = restore_key(k);
            var value = undefined;
            if (v !== undefined && v !== null) {
                value = this._restore(v);                
            }
            // no setattr checks...
            instance[k] = value;
            this._namestack.pop();
        }
        if (has_tag(obj, tags.SEQ)) {
            if (instance.push !== undefined) {
                for (var v in obj[tags.SEQ]) {
                    instance.push(this._restore(v));
                }
            } // no .add ...            
        }
        
        if (has_tag(obj, tags.STATE)) {
            instance = this._restore_state(obj, instance);
        }
        return instance;
    };
    
    
    unpickler.Unpickler.prototype._restore_state = function (obj, instance) {
        // only if the JS object implements __setstate__
        if (instance.__setstate__ !== undefined) {
            var state = this._restore(obj[tags.STATE]);
            instance.__setstate__(state);
        } else {
            instance = this._restore(obj[tags.STATE]);
        }
        return instance;
    };

    unpickler.Unpickler.prototype._restore_list = function (obj) {
        var parent = [];
        this._mkref(parent);
        var children = [];
        for (var i = 0; i < obj.length; i++) {
            var v = obj[i];
            var rest = this._restore(v);
            children.push(rest);
        }
        parent.push.apply(parent, children);
        return parent;
    };
    unpickler.Unpickler.prototype._restore_tuple = function (obj) {
        // JS having no difference between list, tuple, set -- returns Array
        var children = [];
        var tupleContents = obj[tags.TUPLE]; 
        for (var i = 0; i < tupleContents.length; i++) {
            children.push(this._restore(tupleContents[i]));
        }
        return children;
    };
    unpickler.Unpickler.prototype._restore_set = function (obj) {
        // JS having no difference between list, tuple, set -- returns Array
        var children = [];
        var setContents = obj[tags.SET]; 
        for (var i = 0; i < setContents.length; i++) {
            children.push(this._restore(setContents[i]));
        }
        return children;
    };
    unpickler.Unpickler.prototype._restore_dict = function (obj) {
        var data = {};
        //var restore_key = this._restore_key_fn();
        var keys = [];
        for (var k in obj) {
            if (obj.hasOwnProperty(k)) {
                keys.push(k);
            }
        }
        keys.sort();
        for (var i = 0; i < keys.length; i++) {
            var k = keys[i];
            var v = obj[k];
            
            this._namestack.push(k);
            data[k] = this._restore(v);
            // no setattr checks...
            this._namestack.pop();
        }
        return data;
    };
    
    unpickler.Unpickler.prototype._restore_key_fn = function () {
        if (this.keys) {
            return function (key) {
                if (key.indexOf(tags.JSON_KEY) == 0) {
                    key = unpickler.decode(key.slice(tags.JSON_KEY.length),
                            this.handlers,
                            {context: this, keys: this.keys, reset: false}
                    );
                    return key;
                }
            };
        } else {
            return function (key) { return key; };
        }
    };
    
    // _refname not needed...
    
    unpickler.Unpickler.prototype._mkref = function (obj) {
        // does not use id(obj) in javascript
        this._objs.push(obj);
        return obj;
    };
    
    
    unpickler.getargs = function (obj) {
        var seq_list = obj[tags.SEQ];
        var obj_dict = obj[tags.OBJECT];
        if (seq_list === undefined || obj_dict === undefined) {
            return [];
        }
        var typeref = unpickler.loadclass(obj_dict);
        if (typeref === undefined) {
            return [];
        }
        if (typeref['_fields'] !== undefined) {
            if (typeref['_fields'].length == seq_list.length) {
                return seq_list;
            }
        }
        return [];
    };
    
    unpickler.loadclass = function (module_and_name) {
        var main_check = '__main__.';
        if (module_and_name.indexOf(main_check) == 0) {
            module_and_name = module_and_name.slice(main_check.length);
        }
        var parent = window;
        var module_class_split = module_and_name.split('.');
        for (var i = 0; i < module_class_split.length; i++) {
            var this_module_or_class = module_class_split[i];
            parent = parent[this_module_or_class];
            if (parent === undefined) {
                return parent;
            }
        }
        return parent;
    };
    
    unpickler.has_tag = function (obj, tag) {
        if ((typeof obj == 'object') &&
                (obj[tag] !== undefined)) {
            return true;
        } else {
            return false;
        }
    };
    
    // http://stackoverflow.com/questions/1606797/use-of-apply-with-new-operator-is-this-possible
    unpickler.construct = function (constructor, args) {
        function F() {
            return constructor.apply(this, args);
        }
        F.prototype = constructor.prototype;
        return new F();
    };
    
    if (jsonpickle !== undefined) {
        jsonpickle.unpickler = unpickler;
    }    
    return unpickler;    
});
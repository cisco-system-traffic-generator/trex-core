define(['./util', './handlers', './tags'], function(util, handlers, tags) {    
    var pickler = {};
    
    pickler.encode = function(value, options) {
        var params = {
                unpicklable: false,
                make_refs: true,
                keys: false,
                max_depth: undefined,
                reset: true,
                backend: undefined, // does nothing; python compat
                context: undefined,
        };
                
        util.merge(params, options);
        if (params.context === undefined) {
            outparams = {
                    unpicklable: params.unpicklable,
                    make_refs: params.make_refs,
                    keys: params.keys,
                    backend: params.backend,
                    max_depth: params.max_depth,
            };            
            params.context = new pickler.Pickler(outparams);
            var fixed_obj = params.context.flatten(value, params.reset);
            return JSON.stringify(fixed_obj);  
        }
    };
    
    pickler.Pickler = function (options) {
        var params = {
                unpicklable: true,
                make_refs: true,
                max_depth: undefined,
                backend: undefined,
                keys: false,
        };
        util.merge(params, options);
        this.unpicklable = params.unpicklable;
        this.make_refs = params.make_refs;
        this.backend = params.backend;
        this.keys = params.keys;
        this._depth = -1;
        this._max_depth = params.max_depth;
        this._objs = [];
        this._seen = [];        
    };
    pickler.Pickler.prototype.reset = function () {
        this._objs = [];
        this._depth = -1;
        this._seen = [];
    };
    
    pickler.Pickler.prototype._push = function () {
        this._depth += 1;
    };

    pickler.Pickler.prototype._pop = function (value) {
        this._depth -= 1;
        if (this._depth == -1) {
            this.reset();
        }
        return value;
    };

    pickler.Pickler.prototype._mkref = function (obj) {
        var found_id = this._get_id_in_objs(obj);
        //console.log(found_id);
        if (found_id != -1) {
            return false;
//            if (this.unpicklable == false || this.make_refs == false) {
//                return true;
//            } else {
//                return false;
//            }            
        }
        this._objs.push(obj);
        return true;
    };
    pickler.Pickler.prototype._get_id_in_objs = function (obj) {
        var objLength = this._objs.length;
//        console.log('sought obj', obj);
//        console.log('stored objs: ', this._objs);
        for (var i = 0; i < objLength; i++) {
            if (obj === this._objs[i]) {                
                return i;
            }
        }
        return -1;
    };
    
    pickler.Pickler.prototype._getref = function (obj) {
        var wrap_obj =  {};
        wrap_obj[tags.ID] = this._get_id_in_objs(obj);
        return wrap_obj;
    };

    pickler.Pickler.prototype.flatten = function (obj, reset) {
        if (reset === undefined) {
            reset = true;
        }
        if (reset == true) {
            this.reset();
        }
        var flatOut = this._flatten(obj);
        console.log(this._objs);
        return flatOut;
    };
    
    pickler.Pickler.prototype._flatten = function (obj) {
        this._push();
        return this._pop(this._flatten_obj(obj));
    };

    pickler.Pickler.prototype._flatten_obj = function (obj) {
        this._seen.push(obj);
        max_reached = (this._depth == this._max_depth) ? true : false;
        if (max_reached || (this.make_refs == false && this._get_id_in_objs(obj) != -1)) {
            // no repr equivalent, use str;
            return toString(obj);
        } else {
            var flattener = this._get_flattener(obj);
            //console.log(flattener);
            return flattener.call(this, obj);
        }
    };

    pickler.Pickler.prototype._list_recurse = function (obj) {
        var l = [];
        for (var i = 0; i < obj.length; i ++) {
            l.push(this._flatten(obj[i]));
        }
        return l;
        
    };

    pickler.Pickler.prototype._get_flattener = function (obj) {
        if (util.is_primitive(obj)) {
            return function (obj) { return obj; };
        }
        if (util.is_list(obj)) {
            if (this._mkref(obj)) {
                return this._list_recurse;
            } else {
                this._push();
                return this._getref;
            }
        }
        if (util.is_tuple(obj)) {
            if (this.unpicklable == false) {
                return this._list_recurse;
            } else {
                return function (obj) {
                    var obj_wrap = {};
                    obj_wrap[tags.TUPLE] = this._list_recurse(obj);                    
                };
            }
        }
        if (util.is_set(obj)) {
            if (this.unpicklable == false) {
                return this._list_recurse;
            } else {
                return function (obj) {
                    var obj_wrap = {};
                    obj_wrap[tags.SET] = this._list_recurse(obj);                    
                };                
            }
        }
        // better -- translate as object...
        //if (util.is_dictionary(obj)) {
        //    return this._flatten_dict_obj;
        //}
        //if (util.is_type(obj)) {
        //    return _mktyperef;
        //}
        if (util.is_object(obj)) {
            return this._ref_obj_instance;
        }
        console.log('no flattener for ', obj, ' of type ', typeof obj);
        return undefined;
    };

    pickler.Pickler.prototype._ref_obj_instance = function (obj) {
        if (this._mkref(obj)) {
            return this._flatten_obj_instance(obj);
        }
        return this._getref(obj);
    };

    pickler.Pickler.prototype._flatten_obj_instance = function (obj) {
        var data = {};
        has_class = (obj[tags.PY_CLASS] !== undefined); // ? or ...
        has_dict = true;
        has_slots = false;
        has_getstate = (obj.__getstate__ !== undefined);
        
        if (has_class && util.is_module(obj) == false) {
            var fullModuleName = pickler._getclassdetail(obj);
            if (this.unpicklable) {
                data[tags.OBJECT] = fullModuleName;
                console.log(data);
            }
            handler = handlers[fullModuleName];
            if (handler !== undefined) {
                handler.flatten(obj, data);
            }
        }
        
        if (util.is_module(obj)) {
            // todo if possible to have this happen...
        }
        
        if (util.is_dictionary_subclass(obj)) {
            // todo if possible to have this happen...
        }
        if (has_dict) {
            // where every object currently ends up
            if (util.is_sequence_subclass(obj)) {
                // todo if possible to be...
            }
            if (has_getstate) {
                return this._getstate(obj, data);
            }
            return this._flatten_dict_obj(obj, data);
        }
        
        // todo: is_sequence_subclass
        // todo: is_noncomplex
        // todo: has_slots... 
    };

    pickler.Pickler.prototype._flatten_dict_obj = function (obj, data) {
        if (data === undefined) {
            data = new obj.prototype.constructor();
        }
        var key_index = [];
        for (var key in obj) {
            if (obj.hasOwnProperty(key)) {
                key_index.push(key);
            }
        }
        for (var i = 0; i < key_index.length; i++) {
            var key = key_index[i];
            var value = obj[key];
            if (key === tags.PY_CLASS) {
                continue;
            }
            this._flatten_key_value_pair(key, value, data);
        }
        // default_factory...
        return data;
    };
    
    
    // _flatten_newstyle_with_slots
    
    pickler.Pickler.prototype._flatten_key_value_pair = function (k, v, data) {
        if (util.is_picklable(k, v) == false) {
            return data;
        }
        // assume all keys are strings -- Javascript;
        data[k] = this._flatten(v);
        return data;        
    };

    //pickler.Pickler.prototype._flatten_sequence_obj = function () {};

    pickler.Pickler.prototype._getstate = function (obj, data) {
        var state = this._flatten_obj(obj.__getstate__());
        if (this.unpicklable) {
            data[tags.STATE] = state;
        } else {
            data = state;
        }
        return data;
    };
    
    //pickler._mktyperef = function (obj) {};

    pickler._getclassdetail = function(obj) {
       // just returns the Python class name
        return obj[tags.PY_CLASS];        
    };
        
    if (jsonpickle !== undefined) {
        jsonpickle.pickler = pickler;
    }    
    return pickler;    
});
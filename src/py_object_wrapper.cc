#include <node.h>
#include "py_object_wrapper.h"
#include "utils.h"
#include "datetime.h"

Persistent<FunctionTemplate> PyObjectWrapper::py_function_template;

void PyObjectWrapper::Initialize() {
    HandleScope scope;

    PyDateTime_IMPORT;

    Local<FunctionTemplate> fn_tpl = FunctionTemplate::New();
    Local<ObjectTemplate> proto = fn_tpl->PrototypeTemplate();
    Local<ObjectTemplate> obj_tpl = fn_tpl->InstanceTemplate();

    obj_tpl->SetInternalFieldCount(1);

    // this has first priority. see if the properties already exist on the python object
    obj_tpl->SetNamedPropertyHandler(Get, Set);

    // If we're calling `toString`, delegate to our version of ToString
    proto->SetAccessor(String::NewSymbol("toString"), ToStringAccessor); 

    // likewise for valueOf
    obj_tpl->SetAccessor(String::NewSymbol("valueOf"), ValueOfAccessor); 

    // Python objects can be called as functions.
    obj_tpl->SetCallAsFunctionHandler(Call, Handle<Value>());

    py_function_template = Persistent<FunctionTemplate>::New(fn_tpl);
}

Handle<Value> PyObjectWrapper::New(PyObject* obj) {
    HandleScope scope;
    Local<Value> jsVal;

    // undefined
    if(obj == Py_None) {
        jsVal = Local<Value>::New(Undefined());
    }
    else if(PyDict_Check(obj)) {
        Local<Object> dict = v8::Object::New();
        PyObject *key, *value;
        Py_ssize_t pos = 0;
        while (PyDict_Next(obj, &pos, &key, &value)) {
            Handle<Value> jsKey = PyObjectWrapper::New(key);
            Handle<Value> jsValue = PyObjectWrapper::New(value);
            dict->Set(jsKey, jsValue);
        }
        jsVal = dict;
    }
    else if(PyList_CheckExact(obj)) {
        int size = PyList_Size(obj);
        Local<Array> array = v8::Array::New(size);
        PyObject* value;
        for(int i = 0; i < size; i++ ){
            value = PyList_GetItem(obj, i);
            Handle<Value> jsValue = PyObjectWrapper::New(value);
            array->Set(i, jsValue);
        }
        jsVal = array;
    }
    // double
    else if(PyFloat_CheckExact(obj)) {
        double d = PyFloat_AsDouble(obj);
        jsVal = Local<Value>::New(Number::New(d));
    }
    // integer (can be 64b)
    else if(PyInt_CheckExact(obj)) {
        long i = PyInt_AsLong(obj);
        jsVal = Local<Value>::New(Number::New((double) i));
    }
    // string
    else if(PyString_CheckExact(obj)) {
        // ref to internal representation: no need to dealloc
        char *str = PyString_AsString(obj);
        if(str) {
            jsVal = Local<Value>::New(String::New(str));
        }
    }
    else if(PyBool_Check(obj)) {
        int b = PyObject_IsTrue(obj);
        if(b != -1) {
            jsVal = Local<Value>::New(Boolean::New(b));
        }
    }

    if(PyErr_Occurred()) {
        Py_XDECREF(obj);
        return ThrowPythonException();
    }
    
    if(jsVal.IsEmpty()) {
        Local<Object> jsObj = py_function_template->GetFunction()->NewInstance();
        PyObjectWrapper* wrapper = new PyObjectWrapper(obj);
        wrapper->Wrap(jsObj);
        jsVal = Local<Value>::New(jsObj);
    }
    else {
        Py_XDECREF(obj);
    }
    return scope.Close(jsVal);
}

Handle<Value> PyObjectWrapper::Get(Local<String> key, const AccessorInfo& info) {
    // returning an empty Handle<Value> object signals V8 that we didn't
    // find the property here, and we should check the "NamedAccessor" functions
    HandleScope scope;
    PyObjectWrapper* wrapper = ObjectWrap::Unwrap<PyObjectWrapper>(info.Holder());
    String::Utf8Value utf8_key(key);
    string value(*utf8_key);
    PyObject* result = wrapper->InstanceGet(value);
    if(result) {
        return PyObjectWrapper::New(result);
    }
    return Handle<Value>();
}

Handle<Value> PyObjectWrapper::Set(Local<String> key, Local<Value> value, const AccessorInfo& info) {
    // we don't know what to do.
    return Undefined();
}

Handle<Value> PyObjectWrapper::CallAccessor(Local<String> property, const AccessorInfo& info) {
    HandleScope scope;
    Local<FunctionTemplate> func = FunctionTemplate::New(Call);
    return scope.Close(func->GetFunction());
}

Handle<Value> PyObjectWrapper::ToStringAccessor(Local<String> property, const AccessorInfo& info) {
    HandleScope scope;
    Local<FunctionTemplate> func = FunctionTemplate::New(ToString);
    return scope.Close(func->GetFunction());
}

Handle<Value> PyObjectWrapper::ValueOfAccessor(Local<String> property, const AccessorInfo& info) {
    HandleScope scope;
    Local<FunctionTemplate> func = FunctionTemplate::New(ValueOf);
    return scope.Close(func->GetFunction());
}

Handle<Value> PyObjectWrapper::Call(const Arguments& args) {
    HandleScope scope;
    PyObjectWrapper* pyobjwrap = ObjectWrap::Unwrap<PyObjectWrapper>(args.This());
    Handle<Value> result = pyobjwrap->InstanceCall(args);
    return scope.Close(result);
}

Handle<Value> PyObjectWrapper::ToString(const Arguments& args) {
    HandleScope scope;
    PyObjectWrapper* pyobjwrap = ObjectWrap::Unwrap<PyObjectWrapper>(args.This());
    Local<String> result = String::New(pyobjwrap->InstanceToString(args).c_str());
    return scope.Close(result);
}

Handle<Value> PyObjectWrapper::ValueOf(const Arguments& args) {
    HandleScope scope;
    PyObjectWrapper* pyobjwrap = ObjectWrap::Unwrap<PyObjectWrapper>(args.This());
    PyObject* py_obj = pyobjwrap->InstanceGetPyObject();
    if(PyCallable_Check(py_obj)) {
        Local<FunctionTemplate> call = FunctionTemplate::New(Call);
        return scope.Close(call->GetFunction());
    } else if (PyNumber_Check(py_obj)) {
        long long_result = PyLong_AsLong(py_obj);
        return scope.Close(Integer::New(long_result));
    } else if (PySequence_Check(py_obj)) {
        int len = PySequence_Length(py_obj);
        Local<Array> array = Array::New(len);
        for(int i = 0; i < len; ++i) {
            Handle<Object> jsobj = PyObjectWrapper::py_function_template->GetFunction()->NewInstance();
            PyObject* py_obj_out = PySequence_GetItem(py_obj, i);
            PyObjectWrapper* obj_out = new PyObjectWrapper(py_obj_out);
            obj_out->Wrap(jsobj);
            array->Set(i, jsobj);
        }
        return scope.Close(array);
    } else if (PyMapping_Check(py_obj)) {
        int len = PyMapping_Length(py_obj);
        Local<Object> object = Object::New();
        PyObject* keys = PyMapping_Keys(py_obj);
        PyObject* values = PyMapping_Values(py_obj);
        for(int i = 0; i < len; ++i) {
            PyObject *key = PySequence_GetItem(keys, i),
                *value = PySequence_GetItem(values, i),
                *key_as_string = PyObject_Str(key);
            char* cstr = PyString_AsString(key_as_string);

            Local<Object> jsobj = PyObjectWrapper::py_function_template->GetFunction()->NewInstance();
            PyObjectWrapper* obj_out = new PyObjectWrapper(value);
            obj_out->Wrap(jsobj);
            Py_XDECREF(key);
            Py_XDECREF(key_as_string);
        }
        Py_XDECREF(keys);
        Py_XDECREF(values);
        return scope.Close(object);
    }
    return Undefined();
}

PyObject* PyObjectWrapper::ConvertToPython(const Handle<Value>& value) {
    int len;
    HandleScope scope;

    if(value->IsString()) {
        return PyString_FromString(*String::Utf8Value(value->ToString()));
    } else if (value->IsBoolean()) {
    	if (value->ToBoolean()->IsTrue()) {
    		return Py_True;
    	} else {
    		return Py_False;
    	}
    } else if (value->IsNull() || value->IsUndefined()) {
    	return Py_None;
	} else if(value->IsNumber()) {
        return PyFloat_FromDouble(value->NumberValue());
    } else if(value->IsDate()) {
    	Handle<Date> date = Handle<Date>::Cast(value);
        PyObject* floatObj = PyFloat_FromDouble(date->NumberValue() / 1000.0 ); // javascript returns milliseconds since epoch. python wants seconds since epoch
    	PyObject* timeTuple = Py_BuildValue("(O)", floatObj);
        Py_DECREF(floatObj);
        PyObject* dateTime = PyDateTime_FromTimestamp(timeTuple);
        Py_DECREF(timeTuple);
        return dateTime;
    } else if(value->IsObject()) {
    	if(value->IsArray()) {
			Local<Array> array = Array::Cast(*value);
			len = array->Length();
			PyObject* py_list = PyList_New(len);
			for(int i = 0; i < len; ++i) {
				Local<Object> obj = array->Get(i)->ToObject();
				if (!obj->FindInstanceInPrototypeChain(PyObjectWrapper::py_function_template).IsEmpty()) {
					PyObjectWrapper* python_object = ObjectWrap::Unwrap<PyObjectWrapper>(obj);
					PyObject* pyobj = python_object->InstanceGetPyObject();
					PyList_SET_ITEM(py_list, i, pyobj);
				} else {
					Local<Value> js_val = array->Get(i);
					PyList_SET_ITEM(py_list, i, ConvertToPython(js_val));
				}
			}
			return py_list;
		} else {
			Local<Object> obj = value->ToObject();
			if(!obj->FindInstanceInPrototypeChain(PyObjectWrapper::py_function_template).IsEmpty()) {
				PyObjectWrapper* python_object = ObjectWrap::Unwrap<PyObjectWrapper>(value->ToObject());
				PyObject* pyobj = python_object->InstanceGetPyObject();
				return pyobj;
			} else {
				Local<Array> property_names = obj->GetPropertyNames();
				len = property_names->Length();
				PyObject* py_dict = PyDict_New();
				for(int i = 0; i < len; ++i) {
					Local<String> str = property_names->Get(i)->ToString();
					Local<Value> js_val = obj->Get(str);
					PyDict_SetItemString(py_dict, *String::Utf8Value(str), ConvertToPython(js_val));
				}
				return py_dict;
			}
		}
        return NULL;
    } else if(value->IsArray()) {
		Local<Array> array = Array::Cast(*value);
		len = array->Length();
		PyObject* py_list = PyList_New(len);
		for(int i = 0; i < len; ++i) {
			Local<Value> js_val = array->Get(i);
			PyList_SET_ITEM(py_list, i, ConvertToPython(js_val));
		}
		return py_list;
    } else if(value->IsUndefined()) {
        Py_RETURN_NONE;
    }
    return NULL;
}

Handle<Value> PyObjectWrapper::InstanceCall(const Arguments& args) {
    // for now, we don't do anything.
    HandleScope scope;
    int len = args.Length();
    PyObject* args_tuple = PyTuple_New(len);

    for(int i = 0; i < len; ++i) {
        PyObject* py_arg = ConvertToPython(args[i]);
        if (PyErr_Occurred()) {
            return ThrowPythonException();
        }
        PyTuple_SET_ITEM(args_tuple, i, py_arg);
    }
    PyObject* result = PyObject_CallObject(mPyObject, args_tuple);
    if (PyErr_Occurred()) {
        return ThrowPythonException();
    }

    Py_XDECREF(args_tuple);

    if(result) {
        return scope.Close(PyObjectWrapper::New(result));
    } else {
        return ThrowPythonException();
    }
}

string PyObjectWrapper::InstanceToString(const Arguments& args) {
    PyObject* as_string = PyObject_Str(mPyObject);
    string native_string(PyString_AsString(as_string));
    Py_XDECREF(as_string);
    return native_string;
}

PyObject* PyObjectWrapper::InstanceGet(const string& key) {
    if(PyObject_HasAttrString(mPyObject, key.c_str())) {
        PyObject* attribute = PyObject_GetAttrString(mPyObject, key.c_str());
        return attribute;
    } 
    return (PyObject*)NULL;
}

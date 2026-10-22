#ifndef _MULTIDICT_PY38_COMPAT_H
#define _MULTIDICT_PY38_COMPAT_H

#include <Python.h>
#include <structmember.h>
#include <string.h>

/* Compatibility shims for Python 3.8/3.9.  Python 3.8 predates PEP 573
   (module state for heap types) and a few C API additions this extension
   relies on; 3.8/3.9 predate both the type flags a static, non-subclassable
   type is marked with and the single-type `bases` argument accepted by
   PyType_FromModuleAndSpec().  Everything here is compiled out on newer
   versions. */

#if PY_VERSION_HEX < 0x03090000

/* PEP 573 associates every type created from a spec with the module that
   created it.  There is no such association before 3.9, so the module is
   remembered in a process-wide pointer instead.  The extension only has a
   single module state per process on 3.8, which means subinterpreters
   share it; single-interpreter use is unaffected. */
static PyObject* _multidict_compat_module = NULL;

static inline PyObject*
_Multidict_Type_FromSpecWithBases(
    PyObject* module, PyType_Spec* spec, PyObject* bases)
{
    _multidict_compat_module = module;
    return PyType_FromSpecWithBases(spec, bases);
}

static inline PyObject*
_Multidict_Type_GetModule(PyTypeObject* tp)
{
    (void)tp;
    if (_multidict_compat_module == NULL) {
        PyErr_SetString(PyExc_TypeError, "PyType_GetModule: no module");
        return NULL;
    }
    return _multidict_compat_module;
}
#define PyType_GetModule _Multidict_Type_GetModule

/* PyType_FromSpec() only learned the __weaklistoffset__ member convention
   in 3.9.  Install the offset on the finished type instead; no instance
   exists yet, so every instance still sees it and PyObject_ClearWeakRefs()
   accepts the type in tp_dealloc. */
static inline void
_Multidict_FixWeaklistOffset(PyTypeObject* tp, PyType_Spec* spec)
{
    const PyType_Slot* slot;
    const PyMemberDef* memb;
    for (slot = spec->slots; slot->slot; slot++) {
        if (slot->slot != Py_tp_members) {
            continue;
        }
        for (memb = (const PyMemberDef*)slot->pfunc; memb->name != NULL;
             memb++) {
            if (strcmp(memb->name, "__weaklistoffset__") == 0) {
                tp->tp_weaklistoffset = memb->offset;
                return;
            }
        }
    }
}

/* Added in 3.9; the older varargs helper has the same contract. */
#define PyObject_CallMethodNoArgs(obj, name) \
    PyObject_CallMethodObjArgs((obj), (name), NULL)
#define PyObject_CallMethodOneArg(obj, name, arg) \
    PyObject_CallMethodObjArgs((obj), (name), (arg), NULL)

/* Added in 3.9, where it also backs types.GenericAlias.  Before 3.9
   there is no generic alias type, so __class_getitem__ returns the class
   itself, as it did in multidict releases that supported 3.8. */
static inline PyObject*
_Multidict_GenericAlias(PyObject* cls, PyObject* args)
{
    (void)args;
    Py_INCREF(cls);
    return cls;
}
#define Py_GenericAlias _Multidict_GenericAlias

#else  /* PY_VERSION_HEX >= 0x03090000 */

/* Deliberately a function, not a macro: the later
   `#define PyType_FromModuleAndSpec` must not rewrite this call into the
   wrapper that calls it. */
static inline PyObject*
_Multidict_Type_FromSpecWithBases(
    PyObject* module, PyType_Spec* spec, PyObject* bases)
{
    return PyType_FromModuleAndSpec(module, spec, bases);
}

#endif  /* PY_VERSION_HEX < 0x03090000 */

#if PY_VERSION_HEX < 0x030A0000

/* Added in 3.10.  Both bits are unused before then, and statically
   allocated extension types are already immutable without the first one. */
#define Py_TPFLAGS_IMMUTABLETYPE (1UL << 8)
#define Py_TPFLAGS_DISALLOW_INSTANTIATION (1UL << 7)

/* Since 3.10, PyType_FromModuleAndSpec() accepts a single type as `bases`
   and wraps it in a tuple itself; 3.8/3.9 insist on a tuple. */
static inline PyObject*
_Multidict_Type_FromModuleAndSpec(
    PyObject* module, PyType_Spec* spec, PyObject* bases);

/* Py_TPFLAGS_DISALLOW_INSTANTIATION is enforced by type_call() from 3.10
   on.  A tp_new that always fails has the same effect on 3.8/3.9; it also
   stops Type.__new__ from building an uninitialised shell. */
static inline PyObject*
_Multidict_DisallowedNew(PyTypeObject* type, PyObject* args, PyObject* kwargs)
{
    (void)args;
    (void)kwargs;
    PyErr_Format(
        PyExc_TypeError, "cannot create '%.100s' instances", type->tp_name);
    return NULL;
}

static inline PyObject*
_Multidict_Type_FromModuleAndSpec(
    PyObject* module, PyType_Spec* spec, PyObject* bases)
{
    PyObject* res;
    if (bases != NULL && !PyTuple_Check(bases)) {
        PyObject* tup = PyTuple_Pack(1, bases);
        if (tup == NULL) {
            return NULL;
        }
        res = _Multidict_Type_FromSpecWithBases(module, spec, tup);
        Py_DECREF(tup);
    }
    else {
        res = _Multidict_Type_FromSpecWithBases(module, spec, bases);
    }
    if (res != NULL) {
        PyTypeObject* tp = (PyTypeObject*)res;
        if (spec->flags & Py_TPFLAGS_DISALLOW_INSTANTIATION) {
            tp->tp_new = _Multidict_DisallowedNew;
        }
#if PY_VERSION_HEX < 0x03090000
        _Multidict_FixWeaklistOffset(tp, spec);
#endif
    }
    return res;
}
#define PyType_FromModuleAndSpec _Multidict_Type_FromModuleAndSpec

#endif  /* PY_VERSION_HEX < 0x030A0000 */

#endif  /* _MULTIDICT_PY38_COMPAT_H */

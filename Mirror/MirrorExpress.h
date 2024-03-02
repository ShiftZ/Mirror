#pragma once

#include "MirrorClass.h"
#include "MirrorMethod.h"
#include "MirrorEnums.h"

// default property and method class injection
#define CLASS(_Type_, ...) MIRROR_CLASS(_Type_, Mirror::Property, Mirror::Method,, __VA_ARGS__)
#define STRUCT(_Type_, ...) MIRROR_STRUCT(_Type_, Mirror::Property, Mirror::Method,, __VA_ARGS__)
#define ENUM(_Type_, ...) MIRROR_ENUM(_Type_,, __VA_ARGS__)
#define XENUM(_Type_, ...) MIRROR_ENUM_EXTERNAL(_Type_,, __VA_ARGS__)

#define MULTIBASE(...) MIRROR_MULTIBASE(__VA_ARGS__)

#define PROPERTY(name, ...) MIRROR_PROPERTY(name,, __VA_ARGS__)
#define XPROPERTY(name, ...) decltype(name) MIRROR_PROPERTY_DATA(name,, __VA_ARGS__)
#define VPROPERTY(name, ...) MIRROR_VIRTUAL_PROPERTY(name,, __VA_ARGS__)
#define METHOD(name) MIRROR_METHOD(name,)
#define VMETHOD(name) MIRROR_VIRTUAL_METHOD(name,)
#define IMETHOD(name) MIRROR_IMAGINARY_METHOD(name,)

#define GETTER(getter) MIRROR_GETTER(getter)
#define SETTER(setter) MIRROR_SETTER(setter)
#define MOVER(mover) MIRROR_MOVER(mover)
#define METATXT(txt) MIRROR_TEXT(txt)
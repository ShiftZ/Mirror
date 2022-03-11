#pragma once

#include "MirrorClass.h"
#include "MirrorMethod.h"
#include "MirrorEnums.h"

// default property and method class injection
#define CLASS(name, ...) MIRROR_CLASS(Mirror::Property, Mirror::Method, name, __VA_ARGS__)
#define STRUCT(name, ...) MIRROR_STRUCT(Mirror::Property, Mirror::Method, name, __VA_ARGS__)
#define ENUM(type, ...) MIRROR_ENUM(type, __VA_ARGS__)
#define XENUM(type, ...) MIRROR_ENUM_EXTERNAL(type, __VA_ARGS__)

#define MULTIBASE(...) MIRROR_MULTIBASE(__VA_ARGS__)

#define PROPERTY(name, ...) MIRROR_PROPERTY(name, __VA_ARGS__)
#define XPROPERTY(name, ...) decltype(name) MIRROR_PROPERTY_DATA(name, __VA_ARGS__)
#define VPROPERTY(name, ...) MIRROR_VIRTUAL_PROPERTY(name, __VA_ARGS__)
#define METHOD(name) MIRROR_METHOD(name)
#define VMETHOD(name) MIRROR_VIRTUAL_METHOD(name)
#define IMETHOD(name) MIRROR_IMAGINARY_METHOD(name)

#define GETTER(getter) MIRROR_GETTER(getter)
#define SETTER(setter) MIRROR_SETTER(setter)
#define MOVER(mover) MIRROR_MOVER(mover)
#define METATXT(txt) MIRROR_TEXT(txt)
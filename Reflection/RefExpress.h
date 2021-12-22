#pragma once

#include "RefClass.h"
#include "RefMethod.h"
#include "RefEnums.h"

#define CLASS(name, ...) REFLECTION_CLASS(Reflection::Property, Reflection::Method, name, __VA_ARGS__)
#define STRUCT(name, ...) REFLECTION_STRUCT(Reflection::Property, Reflection::Method, name, __VA_ARGS__)
#define ENUM(type, ...) REFLECTION_ENUM(type, __VA_ARGS__)
#define XENUM(type, ...) REFLECTION_ENUM_EXTERNAL(type, __VA_ARGS__)

#define MULTIBASE(...) REFLECTION_MULTIBASE(__VA_ARGS__)

#define PROPERTY(name, ...) REFLECTION_PROPERTY(name, __VA_ARGS__)
#define XPROPERTY(name, ...) decltype(name) REFLECTION_PROPERTY_DATA(name, __VA_ARGS__)
#define VPROPERTY(name, ...) REFLECTION_VIRTUAL_PROPERTY(name, __VA_ARGS__)
#define METHOD(name) REFLECTION_METHOD(name)
#define VMETHOD(name) REFLECTION_VIRTUAL_METHOD(name)
#define IMETHOD(name) REFLECTION_IMAGINARY_METHOD(name)

#define GETTER(getter) REFLECTION_GETTER(getter)
#define SETTER(setter) REFLECTION_SETTER(setter)
#define MOVER(mover) REFLECTION_MOVER(mover)
#define METATXT(txt) REFLECTION_TEXT(txt)
Reflection can be a very handy tool in development, especially in game development. Unfortunately C++ language doesn't provide any reasonable level of reflection and it will not provide it in the foreseeable future. Moreover, it lacks vital tools for a convenient proprietary implementation. For a very long time this library was at my domestic disposal waiting for being deprecated and abandoned as C++ extensively evolved. But it is neither deprecated nor has a replacement been found because there is no library that can meet all my design goals.

# Design goals
This library was designed with these goals in mind:
- header only and lightweight
- no external preprocessing
- no manual class\properties registration
- everything that can be done automatically must be done that way
- reflection shall not interrupt native C++ language features if possible:
    - multiple and virtual inheritance support
    - in-class member initialization
- no type limits, all C++ types and containers can be used as properties
- option to have a custom property getter and setter
- option to implement a serializer\deserializer for templated classes (like stl containers) in a general way with the type as a template parameter (explained later)
- based on the latest C++ features

Basically, the reflection must be as close to native as possible. The old fashioned reflection libraries typically require out of class definition scope property registrations. If C++ had a reflection, it would have no such external registration requirements.

Some implementations lack custom property getters and setters. This is an important thing to have when serializing\deserializing complex classes.
In OOP getters and setters are used to read and modify the state of an object and for a good reason. In many cases you just can not modify a data field of an object without invalidating the object's state. Deserialization is no different, it must use setters and getters, otherwise it breaks the fundamental OOP principle.

And last but not least, serialization\deserialization of templated types. It would be nice to write a serialization\deserialization of std::vector as a template, instead of writing an individual code for each instance of std::vector. And even more, it would be nice to have an option to write a general serializer\deserializer for all containers in a single templated function. There is a json sample showing how you can do it.

The library heavily relies on C++20 features. Concepts instead of SFINAE tricks and ranges helped to shrink the library size and reduced unnecessary template instantiations to minimum.

# Lua bind
Lua bind is a good example of how reflection can be used to integrate lua scripting into your application. It allows to read\write data fields and call methods of native c++ objects with lua code. It's not a part of the library itself, it's built on top of it. You can find it in the 'Addons' folder and a sample of how to use it in the root directory.

# Basic usage
"MirrorExpress.h" is the header that contains everything you need to start. It contains predefined macros for class\struct and property declarations.
Now you can declare a struct and its properties:

```
struct Cat
{
	STRUCT(Cat)
	std::string PROPERTY(name);
	int PROPERTY(lifes) = 9;
	void METHOD(SayMeow)() { std::cout << "Meow"; }
};
```

There are two ways to get the reflected information for the struct:
```
Class* cat_class = Cat::Class::GetClass();
```
or
```
Cat cat;
Class* cat_class = cat.GetClass();
```
cat_class now can be used to obtain information about class\struct properties and methods, class\struct name, bases, heirs and more.
Properties can be used to get and set values:
```
const Property* name = cat_class->GetProperty("name");
name->SetValue(alice, string("Alice"));

const Property* lifes = cat_class->GetProperty("lifes");
int lifes = lifes->GetValue<int>(cat);
```
or using shortcuts:
```
Mirror::SetValue(cat, "name", string("Alice"));
int lifes = Mirror::GetValue<int>(cat, "lifes");
```

For deeper diving please refer to the provided samples.

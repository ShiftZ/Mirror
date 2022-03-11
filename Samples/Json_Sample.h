#pragma once

#include "MirrorExpress.h"
#include "JsonBox.h"

#define JSON_CLASS(Name) MIRROR_CLASS(JsonMetaData, Method, Name)
#define JSON_STRUCT(Name) MIRROR_STRUCT(JsonMetaData, Method, Name)

using namespace std; // using namespace in a header file is not the best idea
using namespace Mirror; // but it makes it easier because c++ doesn't support nested namespace usings

template<typename>
struct JsonReadWrite; // it's convenient to keep Read and Write implementations in a single templated struct

struct JsonMetaData : virtual Property // virtual inheritance to make it possible to combine different metas into a single property class
{
	void (*write)(void*, const Property*, JsonBox::Value&);
	void (*read)(void*, const Property*, const JsonBox::Value&);

	template<typename PropertyMeta>
	JsonMetaData(PropertyMeta* meta) : Property(meta)
	{
		write = &JsonMetaData::Write<typename PropertyMeta::Type>;
		read = &JsonMetaData::Read<typename PropertyMeta::Type>;
	}

	template<typename PropertyType>
	static void Write(void* obj, const Property* property, JsonBox::Value& jval)
	{
		JsonReadWrite<PropertyType>::Write(property->GetValue<PropertyType>(obj), jval);
	}

	template<typename PropertyType>
	static void Read(void* obj, const Property* property, const JsonBox::Value& jval)
	{
		PropertyType value;
		JsonReadWrite<PropertyType>::Read(value, jval);
		property->SetValue(obj, move(value));		
	}
};

template<Mirrored Type>
struct JsonReadWrite<Type>
{
	static void Write(const Type& obj, JsonBox::Value& jval)
	{
		JsonBox::Object jobj;

		for (const JsonMetaData* property : Type::Class::Properties())
		{
			JsonBox::Value property_jval;
			property->write(obj.GetThis(), property, property_jval);
			jobj.insert({string(property->name), property_jval});
		}

		jval.setObject(jobj);
	}

	static void Read(Type& obj, const JsonBox::Value& jval)
	{
		if (jval.isNull()) return;
		
		if (!jval.isObject()) 
			throw logic_error("JsonValue is not an object type.");

		const JsonBox::Object& jobj = jval.getObject();

		for (const JsonMetaData* property : Type::Class::Properties())
		{
			auto it = jobj.find(string(property->name));
			if (it != jobj.end())
				property->read(obj.GetThis(), property, it->second);
		}
	}
};

template<integral Type>
struct JsonReadWrite<Type>
{
	static void Write(const Type& value, JsonBox::Value& jval)
	{
		jval.setInt(int(value));
	}

	static void Read(Type& value, const JsonBox::Value& jval)
	{
		if (!jval.isNull())
		{
			if (jval.isInteger())
				value = Type(jval.getInt());
			else if (jval.isString())
				value = stoi(jval.getString());
			else
				throw logic_error("JsonValue is neither integer nor string.");
		}
		else
			value = Type(0);
	}
};

template<typename Type> requires is_enum_v<Type>
struct JsonReadWrite<Type>
{
	static void Write(const Type& value, JsonBox::Value& jval)
	{
		jval.setString((string)Enum<Type>::ToString(value));
	}

	static void Read(Type& value, const JsonBox::Value& jval)
	{
		if (!jval.isString())
			throw logic_error("JsonValue for enum is not a string");
		value = Enum<Type>::ToValue(jval.getString());
	}
};

template<typename Type> requires is_floating_point_v<Type>
struct JsonReadWrite<Type>
{
	static void Write(const Type& fvalue, JsonBox::Value& jval)
	{
		jval.setDouble(double(fvalue));
	}

	static void Read(Type& fvalue, const JsonBox::Value& jval)
	{
		if (!jval.isDouble() && !jval.isInteger())
			throw logic_error("JsonValue is not a number.");
		fvalue = (Type)jval.getDouble();
	}
};

template<>
struct JsonReadWrite<bool>
{
	static void Write(const bool& value, JsonBox::Value& jval)
	{
		jval.setBoolean(value);
	}

	static void Read(bool& value, const JsonBox::Value& jval)
	{
		if (!jval.isBoolean())
			throw logic_error("JsonValue is not a boolean.");
		value = jval.getBoolean();
	}
};

template<>
struct JsonReadWrite<string>
{
	static void Write(const string& str, JsonBox::Value& jval)
	{
		jval.setString(str);
	}

	static void Read(string& str, const JsonBox::Value& jval)
	{
		if (!jval.isString())
			throw logic_error("JsonValue is not a string.");
		str = jval.getString();
	}
};

template<ranges::range Type> // a very general range reader\writer
struct JsonReadWrite<Type>
{
	using ValueType = decay_t<decltype(*begin(declval<Type>()))>;

	static void Write(const Type& range, JsonBox::Value& jval)
	{
		int i = 0;
		JsonBox::Array jary(size(range));
		for (auto it = begin(range); it != end(range); ++it, ++i)
			JsonReadWrite<ValueType>::Write(*it, jary[i]);
		jval.setArray(jary);
	}

	static void Read(Type& range, const JsonBox::Value& jval)
	{
		if (!jval.isArray())
			throw logic_error("JsonValue is not an array.");

		const JsonBox::Array& jary = jval.getArray();
		vector<ValueType> values(jary.size());
		for (unsigned i = 0; i < jary.size(); i++)
			JsonReadWrite<ValueType>::Read(values[i], jary[i]);
		range = Type(make_move_iterator(values.begin()), make_move_iterator(values.end()));
	}
};
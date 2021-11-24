#pragma once

#include <any>
#include <memory>
#include <vector>
#include <string>

#include "RefClass.h"
#include "RefProperty.h"
#include "RefMethod.h"

#include "lua.h"
#include "lauxlib.h"

struct LuaDestroyer { void operator()( lua_State* state ) { if (state) lua_close(state); } };

template< typename > struct LuaValue;

template<typename Type>
concept LuaPushable = requires(Type v) { LuaValue<Type>::Push(nullptr, v); };

template<typename Type>
concept LuaGettable = requires(Type v) { LuaValue<Type>::Get(nullptr, 0); };

template<typename Type>
void LuaPush(lua_State* lua, Type& value) { LuaValue<Type>::Push(lua, value); }

template<typename Type>
void LuaPush(lua_State* lua, Type&& value) { LuaValue<Type>::Push(lua, move(value)); }

template<typename Type>
Type LuaGet(lua_State* lua, int index) { return LuaValue<Type>::Get(lua, index); }

class LuaProperty : public virtual Reflection::Property
{
	void (LuaProperty::*push_value)(void*, lua_State*) const = nullptr;
	void (LuaProperty::*read_value)(void*, lua_State*, int) const = nullptr;
	std::any (*get_any)(lua_State*, int) = nullptr;
	
public:
	template< typename Type >
	void Pusher( void* obj, lua_State* lua ) const { LuaValue<Type>::Push(lua, GetValue<Type>(obj)); }

	template< typename Type >
	void Reader( void* obj, lua_State* lua, int index ) const { SetValue(obj, LuaValue<Type>::Get(lua, index)); }

	template< typename Type >
	static std::any AnyGetter( lua_State* lua, int index )
	{
		return std::make_any<Type>(Type(LuaValue<Type>::Get(lua, index)));
	}

public:
	template< typename Meta >
	LuaProperty( Meta* meta ) : Property(meta)
	{
		if constexpr (LuaPushable<Meta::Type>)
			push_value = &LuaProperty::Pusher<Meta::Type>;

		if (meta_text.find("LuaReadOnly") == std::string::npos)
		{
			if constexpr (LuaGettable<Meta::Type>)
				read_value = &LuaProperty::Reader<typename Meta::Type>;

			if constexpr (LuaGettable<Meta::Type> && Reflection::IsCopyConstructible<Meta::Type>)
				get_any = &LuaProperty::AnyGetter<Meta::Type>;
		}
	}

	using Reflection::Property::GetAny;

	std::any GetAny( lua_State* lua, int index ) const
	{
		if (!get_any) throw Reflection::Exception("Property '%s' of type '%s' can not be converted to 'any'", name, type->name());
		return get_any(lua, index);
	}

	void PushValue( void* obj, lua_State* lua ) const
	{
		if (!push_value) throw Reflection::Exception("Property '%s' of type '%s' can not be read", name, type->name());
		(this->*push_value)(obj, lua);
	}

	void PushValue( Reflection::Reflected* obj, lua_State* lua ) const
	{
		PushValue(obj->GetThis(), lua);
	}

	void ReadValue( void* obj, lua_State* lua, int index ) const
	{
		if (!read_value) throw Reflection::Exception("Property '%s' of type '%s' can not be written", name, type->name());
		(this->*read_value)(obj, lua, index);
	}

	void ReadValue( Reflection::Reflected* obj, lua_State* lua, int index ) const
	{
		ReadValue(obj->GetThis(), lua, index);
	}

	bool CanPushValue() const { return push_value != nullptr; }
	bool CanReadValue() const { return read_value != nullptr; }
	bool CanGetAny() const { return get_any != nullptr; }
};

struct LuaMethod : virtual Reflection::Method
{
	template< typename Signature, int N, int... Ns >
	struct Caller : Caller<Signature, N - 1, Ns..., N> {};

	template< typename Return, typename... Args, int... Ns >
	struct Caller<Return (Args...), 0, Ns...>
	{
		static Return Call( lua_State* lua, Reflection::Reflected* obj, Method* method )
			{ return method->Invoke<Return (Args...)>(*obj, LuaValue<Args>::Get(lua, -Ns)...); }
	};

	lua_CFunction invoke;

	template< typename MethodMeta >
	LuaMethod( MethodMeta* meta ) : Method(meta)
	{
		Bind(meta->Func());
	}

	template< typename ReturnType, typename Type, typename... Arguments >
	void Bind( ReturnType (Type::*)(Arguments...) )
	{
		invoke = &LuaMethod::Invoke<ReturnType, Type, Arguments...>;
	}

	template< typename ReturnType, typename Type, typename... Arguments >
	static int Invoke( lua_State* lua )
	{
		if (lua_gettop(lua) != sizeof...(Arguments) + 1)
			luaL_error(lua, "Invalid number of arguments (%d/%d)", lua_gettop(lua), sizeof...(Arguments) + 1);

		if (!lua_isuserdata(lua, 1))
			luaL_argerror(lua, 1, "must be an object.");

		Method* method = (LuaMethod*)lua_touserdata(lua, lua_upvalueindex(1));
		Reflected* obj = *(Reflected**)lua_touserdata(lua, 1);

		try
		{
			if constexpr (!std::is_same_v<ReturnType, void>)
				LuaValue<ReturnType>::Push(lua, Caller<ReturnType (Arguments...), sizeof...(Arguments)>::Call(lua, obj, method));
			else
				Caller<ReturnType (Arguments...), sizeof...(Arguments)>::Call(lua, obj, method);
		}
		catch ( std::exception& ex )
		{
			luaL_error(lua, "Error occurred running '%s': '%s'", method->name, ex.what());
		}

		return lua_gettop(lua) - sizeof...(Arguments) - 1;
	}
};

inline int LuaGetter( lua_State* lua )
{
	std::string name = lua_tostring(lua, -1);
	Reflected* obj = *(Reflected**)lua_touserdata(lua, -2);
	if (!obj) luaL_error(lua, "Referring to null object.");
	Reflection::Class* cls = obj->GetClass();

	LuaProperty* property = nullptr;
	LuaMethod* method = nullptr;

	if (name.front() > 'Z')
	{
		property = cls->GetProperty<LuaProperty>(name);
		if (!property) method = cls->GetMethod<LuaMethod>(name);
	}
	else
	{
		method = cls->GetMethod<LuaMethod>(name);
		if (!method) property = cls->GetProperty<LuaProperty>(name);
	}

	if (property)
	{
		if (!property->CanPushValue())
			luaL_error(lua, "field '%s' can not be read from lua", name.c_str());
		property->PushValue(obj, lua);
		return 1;
	}

	if (method)
	{
		lua_pushlightuserdata(lua, method);
		lua_pushcclosure(lua, method->invoke, 1);
		return 1;
	}

	return luaL_error(lua, "field '%s' not found in '%s'", name.c_str(), cls->RawName());
}

inline int LuaSetter( lua_State* lua )
{
	std::string name = lua_tostring(lua, -2);
	Reflected* obj = *(Reflected**)lua_touserdata(lua, -3);
	if (!obj) luaL_error(lua, "Assigning to null object.");
	Reflection::Class* cls = obj->GetClass();

	if (LuaProperty* property = cls->GetProperty<LuaProperty>(name))
	{
		if (!property->CanReadValue()) 
			luaL_error(lua, "Property '%s' of type '%s' can not be written from lua", property->name, property->type->name());
		property->ReadValue(obj, lua, -1);
	}
	else
		luaL_error(lua, "Property '%s' not found in '%s'", name.c_str(), cls->RawName());

	return 0;
}

inline int LuaDestructor( lua_State* lua )
{
	using Data = std::pair<Reflected*, bool>;
	auto& [ptr, own] = *(Data*)lua_touserdata(lua, 1);
	if (own) delete ptr;
	return 0;
}

inline int LuaEqual( lua_State* lua )
{
	Reflected* a = *(Reflected**)luaL_checkudata(lua, 1, "Reflected");
	Reflected* b = *(Reflected**)luaL_checkudata(lua, 2, "Reflected");
	lua_settop(lua, 0);
	lua_pushboolean(lua, a == b);
	return 1;
}

inline void LuaBindGetter( lua_State* lua, lua_CFunction getter )
{
	luaL_getmetatable(lua, "Reflected");
	if (lua_isnil(lua, -1))
	{
		lua_pop(lua, 1);
		luaL_newmetatable(lua, "Reflected");
	}
	lua_pushcfunction(lua, getter);
	lua_setfield(lua, -2, "__index");
	lua_pop(lua, 1);
}

inline void LuaBindSetter( lua_State* lua, lua_CFunction setter )
{
	luaL_getmetatable(lua, "Reflected");
	if (lua_isnil(lua, -1))
	{
		lua_pop(lua, 1);
		luaL_newmetatable(lua, "Reflected");
	}
	lua_pushcfunction(lua, setter);
	lua_setfield(lua, -2, "__newindex");
	lua_pop(lua, 1);
}

inline void LuaBindEqual( lua_State* lua, lua_CFunction eq )
{
	luaL_getmetatable(lua, "Reflected");
	if (lua_isnil(lua, -1))
	{
		lua_pop(lua, 1);
		luaL_newmetatable(lua, "Reflected");
	}
	lua_pushcfunction(lua, eq);
	lua_setfield(lua, -2,  "__eq");
	lua_pop(lua, 1);
}

inline void LuaBindDestructor( lua_State* lua, lua_CFunction destructor )
{
	luaL_getmetatable(lua, "Reflected");
	if (lua_isnil(lua, -1))
	{
		lua_pop(lua, 1);
		luaL_newmetatable(lua, "Reflected");
	}
	lua_pushcfunction(lua, destructor);
	lua_setfield(lua, -2, "__gc");
	lua_pop(lua, 1);
}


template<>
struct LuaValue<std::string>
{
	static void Push( lua_State* lua, const std::string& str )
		{ lua_pushlstring(lua, str.c_str(), str.size()); }

	static std::string Get( lua_State* lua, int index )
		{ return !lua_isnil(lua, index) ? luaL_checkstring(lua, index) : std::string(); }
};

template<>
struct LuaValue<const char*>
{
	static void Push( lua_State* lua, const char* str ) { lua_pushstring(lua, str); }
	static const char* Get( lua_State* lua, int index ) { return luaL_checkstring(lua, index); }
};

template<>
struct LuaValue<bool>
{
	static void Push( lua_State* lua, bool value ) { lua_pushboolean(lua, value); }
	static bool Get( lua_State* lua, int index )
	{
		luaL_checktype(lua, index, LUA_TBOOLEAN);
		return lua_toboolean(lua, index);
	}
};

template< typename Type > requires std::is_integral_v<Type> && !std::is_same_v<bool, Type>
struct LuaValue<Type>
{
	static void Push( lua_State* lua, Type value ) { lua_pushinteger(lua, value); }
	static Type Get( lua_State* lua, int index ) { return (Type)luaL_checknumber(lua, index); }
};

template< typename Type > requires std::is_floating_point_v<Type>
struct LuaValue<Type>
{
	static void Push( lua_State* lua, Type value ) { lua_pushnumber(lua, value); }
	static Type Get( lua_State* lua, int index ) { return (Type)luaL_checknumber(lua, index); }
};

template< typename Type > requires std::is_enum_v<Type>
struct LuaValue<Type>
{
	static void Push( lua_State* lua, Type value ) { lua_pushinteger(lua, int(value)); }
	static Type Get( lua_State* lua, int index ) { return (Type)luaL_checkinteger(lua, index); }
};

template< typename Type > requires std::is_convertible_v<Type*, Reflected*>
struct LuaValue<Type*>
{
	static void Push( lua_State* lua, const Reflected* iref, bool possess = false )
	{
		if (!iref) return lua_pushnil(lua);
		using Data = std::pair<const Reflected*, bool>;
		Data* data = (Data*)lua_newuserdata(lua, sizeof(Data));
		*data = {iref, possess};
		luaL_getmetatable(lua, "Reflected");
		lua_setmetatable(lua, -2);
	}

	static Type* Get( lua_State* lua, int index )
	{
		if (!lua_isuserdata(lua, index) && !lua_isnil(lua, index))
			luaL_argerror(lua, index, "not an object");

		if (lua_isnil(lua, index)) return nullptr;
		
		Reflected* obj = *(Reflected**)lua_touserdata(lua, index);
		return dynamic_cast<Type*>(obj);
	}
};

template< typename Type > requires Reflection::Reflected<Type> && !std::is_convertible_v<Type*, Reflected*>
struct LuaValue<Type*>
{
	static void Push( lua_State* lua, Type* value ) { LuaValue<Type>::Push(lua, *value); }
};

template< typename Type >
struct LuaValue<std::unique_ptr<Type>>
{
	static void Push( lua_State* lua, const std::unique_ptr<Type>& ptr ) requires LuaPushable<Type*>
	{
		LuaValue<Type*>::Push(lua, ptr.get());
	}
	
	static void Push( lua_State* lua, std::unique_ptr<Type>&& ptr ) requires LuaPushable<Type*>
	{
		LuaValue<Reflected*>::Push(lua, ptr.release(), true);
	}

	static std::unique_ptr<Type> Get( lua_State* lua, int index ) requires std::is_convertible_v<Type*, Reflected*>
	{
		if (!lua_isuserdata(lua, index) && !lua_isnil(lua, index))
			luaL_argerror(lua, index, "not an object");

		if (lua_isnil(lua, index)) return nullptr;

		using Data = std::pair<Reflected*, bool>;
		auto& [ptr, own] = *(Data*)lua_touserdata(lua, index);
		if (!own) luaL_argerror(lua, index, "the object must be owned by Lua");
		own = false;

		return std::unique_ptr<Type>((Type*)ptr);
	}
};

template< typename Type > requires Reflection::Reflected<Type>
struct LuaValue<Type>
{
	//static void Push( lua_State* lua, const Reflected& iref ) { LuaValue<Reflected*>::Push(lua, &iref); }

	static void Push( lua_State* lua, const Type& obj )
	{
		lua_newtable(lua);
		Reflection::Class* cls = obj.GetClass();
		for (LuaProperty* property :  cls->Properties<LuaProperty>())
		{
			if (property->CanPushValue())
			{
				property->PushValue(obj.GetThis(), lua);
				lua_setfield(lua, -2, property->name);
			}
		}
	}

	static Type Get( lua_State* lua, int index ) requires std::is_copy_constructible_v<Type>
	{
		if (!lua_isuserdata(lua, index) && !lua_istable(lua, index))
		luaL_argerror(lua, index, "not an object. user data or table expected.");
	
		if (lua_isuserdata(lua, index))
		{
			Reflected* iref = *(Reflected**)lua_touserdata(lua, index);
			Type* src = dynamic_cast<Type*>(iref);
			if (!src)
			{
				const char* msg = lua_pushfstring(lua, "can't convert '%s' to '%s'", typeid(*iref).name(), typeid(Type).name());
				luaL_argerror(lua, index, msg);
			}

			return Type(*src);
		}
		else
		{
			Type object;
			Reflection::Class* cls = object.GetClass();
			lua_pushnil(lua);
			while (lua_next(lua, index < 0 ? index - 1 : index))
			{
				if (!lua_isstring(lua, -2))
					luaL_argerror(lua, index, "object table key is not a string");

				const char* name = lua_tostring(lua, -2);
				LuaProperty* property = cls->GetProperty<LuaProperty>(name);
				if (!property)
				{
					const char* msg = lua_pushfstring(lua, "can't find lua property '%s'", name);
					luaL_argerror(lua, index, msg);
				}

				if (!property->CanReadValue())
				{
					const char* msg = lua_pushfstring(lua, "property '%s' can not be written", name);
					luaL_argerror(lua, index, msg);
				}

				property->ReadValue(&object, lua, -1);
				
				lua_pop(lua, 1);
			}
			return object;
		}
	}
};

template< typename Type, template< typename... > class Array, typename... Others >
requires Reflection::IsSTLContainer<Array<Type, Others...>> && !std::is_same_v<Array<Type, Others...>, std::string> &&
		 !Reflection::IsSTLMap<Array<Type, Others...>>
struct LuaValue<Array<Type, Others...>>
{
	static void Push( lua_State* lua, const Array<Type, Others...>& ar ) requires LuaPushable<Type>
	{
		lua_newtable(lua);
		int i = 0;
		for (const Type& value : ar)
		{
			lua_pushinteger(lua, ++i);
			LuaValue<Type>::Push(lua, value);
			lua_settable(lua, -3);
		}
	}

	static void Push( lua_State* lua, Array<Type>&& ar ) requires LuaPushable<Type>
	{
		lua_newtable(lua);
		int i = 0;
		for (const Type& value : ar)
		{
			lua_pushinteger(lua, ++i);
			LuaValue<Type>::Push(lua, std::move(value));
			lua_settable(lua, -3);
		}
	}

	static Array<Type, Others...> Get( lua_State* lua, int index ) requires LuaGettable<Type>
	{
		if (!lua_istable(lua, index))
			luaL_argerror(lua, index, "not a table");

		std::vector<Type> values;
		lua_pushnil(lua);
		while (lua_next(lua, index < 0 ? index - 1 : index))
		{
			values.push_back(LuaValue<Type>::Get(lua, -1));
			lua_pop(lua, 1);
		}

		return Array<Type, Others...>(std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
	}
};

template< typename Key, typename Value, template< typename... > class Map, typename... Others >
requires Reflection::IsSTLMap<Map<Key, Value, Others...>>
struct LuaValue<Map<Key, Value, Others...>>
{
	static void Push( lua_State* lua, const Map<Key, Value, Others...>& map ) requires LuaPushable<Key> && LuaPushable<Value>
	{
		lua_newtable(lua);
		for (auto& [key, value] : map)
		{
			LuaValue<Key>::Push(lua, key);
			LuaValue<Value>::Push(lua, value);
			lua_settable(lua, -3);
		}

		if constexpr (std::is_convertible_v<Key, Reflected*>)
		{
			if (luaL_newmetatable(lua, "unordered_map<Reflected*, Value>"))
			{
				auto lookup = []( lua_State* lua )
				{
					lua_pushnil(lua);
					while (lua_next(lua, -3))
					{
						Reflected* a = *(Reflected**)lua_touserdata(lua, -2);
						Reflected* b = *(Reflected**)lua_touserdata(lua, -3);
						if (a == b) return 1;
						lua_pop(lua, 1);
					}
					lua_pushnil(lua);
					return 1;
				};

				lua_pushcfunction(lua, lookup);
				lua_setfield(lua, -2, "__index");
			}
			lua_setmetatable(lua, -2);
		}
	}

	static Map<Key, Value, Others...> Get( lua_State* lua, int index ) requires LuaGettable<Key> && LuaGettable<Value>
	{
		if (!lua_istable(lua, index))
			luaL_argerror(lua, index, "not a table");

		std::vector<std::pair<Key, Value>> values;
		lua_pushnil(lua);
		while (lua_next(lua, index < 0 ? index - 1 : index))
		{
			values.emplace_back(LuaValue<Key>::Get(lua, -2), LuaValue<Value>::Get(lua, -1));
			lua_pop(lua, 1);
		}

		return Map<Key, Value>(std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
	}
};

inline const char* LuaLoadTable( lua_State*, const char* );
inline void LuaWriteTable( lua_State* lua, std::vector<char>& buf, int index = -1 );

inline bool LuaWrite( lua_State* lua, std::vector<char>& buf, int index = -1 )
{
	char type = lua_type(lua, index);
	switch (type)
	{
		case LUA_TNIL: buf.push_back(type); return true;

		case LUA_TBOOLEAN:
		{
			buf.push_back(type);
			buf.push_back(lua_toboolean(lua, index));
			return true;
		}

		case LUA_TNUMBER:
		{
			buf.push_back(type);
			buf.push_back(lua_isinteger(lua, index));
			if (lua_isinteger(lua, index))
			{
				lua_Integer value = lua_tointeger(lua, index);
				buf.insert(buf.end(), (char*)&value, (char*)(&value + 1));
			}
			else
			{
				lua_Number value = lua_tonumber(lua, index);
				buf.insert(buf.end(), (char*)&value, (char*)(&value + 1));
			}
			return true;
		}
			
		case LUA_TSTRING:
		{
			buf.push_back(type);
			size_t len;
			const char* str = lua_tolstring(lua, index, &len);
			buf.insert(buf.end(), (char*)&len, (char*)(&len + 1));
			buf.insert(buf.end(), str, str + len);
			return true;
		}

		case LUA_TTABLE:
		{
			buf.push_back(type);
			LuaWriteTable(lua, buf, index);
			return true;
		}

		case LUA_TFUNCTION:
		{
			buf.push_back(type);
			buf.resize(buf.size() + sizeof(int));
			size_t mark = buf.size();
			
			auto writer = []( lua_State* lua, const void* data, size_t size, void* userdata )
			{
				std::vector<char>* buffer = (std::vector<char>*)userdata;
				buffer->insert(buffer->end(), (char*)data, (char*)data + size);
				return 0;
			};

			lua_pushvalue(lua, index);
			lua_dump(lua, writer, &buf, false);
			lua_pop(lua, 1);

			size_t size = buf.size() - mark;
			char* size_place = buf.data() + mark - sizeof(int);
			*(int*)size_place = int(size);
			return true;
		}

		case LUA_TUSERDATA:
		{
			buf.push_back(type);
			
			lua_getmetatable(lua, index);
			if (!lua_istable(lua, -1))
			{
				lua_pop(lua, 1);
				throw Reflection::Exception("can not write user data without metatable");
			}

			lua_getfield(lua, -1, "__name");
			const char* name = lua_tostring(lua, -1);
			lua_pop(lua, 1);

			lua_getfield(lua, -1, "write");
			if (!lua_isfunction(lua, -1))
			{
				lua_pop(lua, 2);
				return false;
			}

			buf.insert(buf.end(), name, name + strlen(name) + 1);

			lua_pushvalue(lua, index < 0 ? index - 2 : index);
			lua_pushlightuserdata(lua, &buf);
			lua_call(lua, 2, 0);

			lua_pop(lua, 1); // pop metatable
			
			return true;
		}
	}

	return false;
}

inline const char* LuaLoad( lua_State* lua, const char* ptr )
{
	switch (*(ptr++))
	{
		case LUA_TNIL: lua_pushnil(lua); break;
		case LUA_TBOOLEAN: lua_pushboolean(lua, *(ptr++)); break;
		
		case LUA_TNUMBER:
		{
			if (*(ptr++))
			{
				lua_pushinteger(lua, *(lua_Integer*)ptr);
				ptr += sizeof(lua_Integer);
			}
			else
			{
				lua_pushnumber(lua, *(lua_Number*)ptr);
				ptr += sizeof(lua_Number);
			}
			break;
		}

		case LUA_TSTRING:
		{
			size_t* len = (size_t*)ptr;
			ptr += sizeof(size_t);
			lua_pushlstring(lua, (char*)ptr, *len);
			ptr += *len;
			break;
		}

		case LUA_TTABLE:
		{
			lua_newtable(lua);
			ptr = LuaLoadTable(lua, ptr);
			break;
		}

		case LUA_TFUNCTION:
		{
			int size = *(int*)ptr;
			ptr += sizeof(int);
			luaL_loadbufferx(lua, (char*)ptr, size, "", "b");
			ptr += size;
			break;
		}

		case LUA_TUSERDATA:
		{
			luaL_getmetatable(lua, ptr);
			if (!lua_istable(lua, -1))
				throw Reflection::Exception("'%s' metatable not found", ptr);
			
			lua_getfield(lua, -1, "read");
			if (!lua_isfunction(lua, -1))
				throw Reflection::Exception("no reader in '%s' metafield", ptr);

			lua_remove(lua, -2); // remove metatable

			ptr += strlen(ptr) + 1;
			lua_pushlightuserdata(lua, (void*)ptr);
			lua_call(lua, 1, 2);

			lua_Integer size = lua_tointeger(lua, -1);
			ptr += size;
			lua_pop(lua, 1);

			break;
		}
	}

	return ptr;
}

inline void LuaWriteTable( lua_State* lua, std::vector<char>& buf, int index )
{
	assert(lua_istable(lua, index));
	lua_pushnil(lua);
	
	while (lua_next(lua, index < 0 ? index - 1 : index))
	{
		size_t size = buf.size();
		if (!LuaWrite(lua, buf, -2) || !LuaWrite(lua, buf, -1))
			buf.resize(size);
		lua_pop(lua, 1);
	}
	buf.push_back(LUA_TNONE);
}

inline const char* LuaLoadTable( lua_State* lua, const char* ptr )
{
	while (*ptr != LUA_TNONE)
	{
		ptr = LuaLoad(lua, ptr);
		ptr = LuaLoad(lua, ptr);
		lua_settable(lua, -3);
	}
	return ptr + 1;
}


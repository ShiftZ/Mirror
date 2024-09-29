#pragma once

#include <type_traits>

namespace Mirror
{
	using namespace std;

	template<typename Type>
	struct StaticInstance
	{
		static Type instance;
	};

	template<typename Type>
	Type StaticInstance<Type>::instance;

	template<void* (*Func)()>
	struct Executor
	{
		Executor() { Func(); }
	};

	struct Constructor
	{
		Constructor(void (*func)()) { func(); }
	};

	template<typename Type>
	concept Mirrored = requires { typename Type::Meta; };

	template<typename Type>
	concept StlContainer = requires (Type& c)
	{
		typename Type::iterator;
		typename Type::value_type;
		c.begin(), c.end();
	};

	template<typename Type>
	constexpr bool StlSet = StlContainer<Type> && is_same_v<typename Type::key_type, typename Type::value_type>;

	template<typename Type>
	concept StlMap = StlContainer<Type> && requires { Type::mapped_type; };

	template<typename Type>
	concept CopyAssignable = 
		(!StlContainer<Type> && is_copy_assignable_v<Type>) ||
		(StlMap<Type> && is_copy_assignable_v<typename Type::mapped_type> && is_copy_assignable_v<typename Type::key_type>) ||
		is_copy_assignable_v<typename Type::value_type>;
	
	template<typename Type>
	concept CopyConstructible = 
		(!StlContainer<Type> && is_copy_constructible_v<Type>) ||
		(StlMap<Type> && is_copy_constructible_v<typename Type::mapped_type> && is_copy_constructible_v<typename Type::key_type>) ||
		is_copy_constructible_v<typename Type::value_type>;

	template<typename Type>
	concept DefaultConstructible = requires { new Type; };

	template<typename...> struct TypeList;

	class Temporal
	{
		void* ptr;
		void (*destructor)(void*);

	public:
		template<typename Type>
		static void Destroy(void* ptr) { delete (Type*)ptr; }

		template<typename Type>
		static void* Make(const Type& original)
		{
			static thread_local Temporal copy;
			if (copy.ptr) copy.destructor(copy.ptr);
			copy.ptr = new Type(original);
			copy.destructor = Destroy<Type>;
			return copy.ptr;
		}

		~Temporal() { if (ptr) destructor(ptr); }
	};

	template<typename ReturnType, typename Type, typename... Arguments>
	ReturnType GetReturnType(ReturnType (Type::*)(Arguments ...));

	template<typename ReturnType, typename... Arguments>
	ReturnType GetReturnType(ReturnType (*)(Arguments ...));
}
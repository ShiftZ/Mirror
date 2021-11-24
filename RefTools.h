#pragma once

#include <exception>
#include <type_traits>
#include <string>

namespace Reflection
{
	using namespace std;

	class Exception : public std::exception
	{
		std::string msg;

	public:
		template<typename... Types>
		Exception(const char* format, Types ... args)
		{
			int len = snprintf(nullptr, 0, format, args...);
			msg.resize(len + 1);
			snprintf(&msg.front(), len + 1, format, args...);
			msg.pop_back();
		}

		const char* what() const noexcept override { return msg.c_str(); }
	};

	template<typename Type>
	struct StaticInstance
	{
		static Type instance;
	};

	template<typename Type>
	Type StaticInstance<Type>::instance;

	template<void (*Func)()>
	struct Executor
	{
		Executor() { Func(); }
	};

	template<typename Type>
	struct ReflectedCheck
	{
		template<typename type, typename = typename type::Class>
		static constexpr bool test(type*) { return true; }
		static constexpr bool test(...) { return false; }
		static const bool value = test((Type*)0);
	};

	template<>
	struct ReflectedCheck<class Reflected> : std::integral_constant<bool, true> {};

	template<typename Type>
	constexpr bool is_reflected = ReflectedCheck<Type>::value;

	template<typename Type>
	struct STLContainer
	{
		template<typename type, typename = typename type::iterator, typename = typename type::value_type>
		static constexpr bool test(type* p, decltype(p->begin())* = nullptr, decltype(p->end())* = nullptr) { return true; }
		static constexpr bool test(...) { return false; }
		static constexpr bool value = test((Type*)0);
	};

	template<typename Type>
	constexpr bool IsSTLContainer = STLContainer<Type>::value;

	template<typename Type>
	struct STLSet
	{
		template<typename type, typename key_type = typename type::key_type, typename value_type = typename type::value_type>
		static constexpr bool test(type*) { return std::is_same_v<key_type, value_type> && STLContainer<type>::value; }
		static constexpr bool test(...) { return false; }
		static constexpr bool value = test((Type*)0);
	};

	template<typename Type>
	constexpr bool IsSTLSet = STLSet<Type>::value;

	template<typename Type>
	struct STLMap
	{
		template<typename type, typename = typename type::mapped_type>
		static constexpr bool test(type*) { return STLContainer<type>::value; }
		static constexpr bool test(...) { return false; }
		static constexpr bool value = test((Type*)0);
	};

	template<typename Type>
	constexpr bool IsSTLMap = STLMap<Type>::value;

	template<typename Type>
	struct CopyAssignable
	{
		template<typename type, typename = enable_if_t<IsSTLContainer<type>>>
		static constexpr bool test(type*)
		{
			if constexpr (IsSTLMap<type>)
				return is_copy_assignable_v<typename type::key_type> && is_copy_assignable_v<typename type::mapped_type>;
			else
				return is_copy_assignable_v<typename type::value_type>;
		}

		static constexpr bool test(...) { return is_copy_assignable_v<Type>; }
		static constexpr bool value = test((Type*)0);
	};

	template<typename Type>
	constexpr bool IsCopyAssignable = CopyAssignable<Type>::value;

	template<typename Type>
	struct CopyConstructible
	{
		template<typename type, typename = enable_if_t<IsSTLContainer<type>>>
		static constexpr bool test(type*)
		{
			if constexpr (IsSTLMap<type>)
				return is_copy_constructible_v<typename type::key_type> && is_copy_constructible_v<typename type::mapped_type>;
			else
				return is_copy_constructible_v<typename type::value_type>;
		}

		static constexpr bool test(...) { return is_copy_constructible_v<Type>; }
		static constexpr bool value = test((Type*)0);
	};

	template<typename Type>
	constexpr bool IsCopyConstructible = CopyConstructible<Type>::value;

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
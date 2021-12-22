#pragma once

#include <typeindex>
#include <type_traits>
#include <format>

#define REFLECTION_METHOD_META(name, native_name, ...) \
	xproperty_##name##_type(); \
	struct xproperty_##name##_meta \
	{ \
		using Scope = Class::Type; \
		string_view Name() { return #name; } \
		auto Func() { return &Class::Type::native_name; } \
		REFLECTION_FORCEDSPEC static void Register() \
		{ \
			Reflection::StaticInstance<Reflection::Executor<&Register>>::instance; \
			xproperty_##name##_meta meta; \
			Class::MethodType* m = new Class::MethodType(&meta); \
			Reflection::Class::Instance<Class::Type>()->AddMethod(m); \
		} \
	}; \
	friend struct xproperty_##name##_meta; \
	__VA_ARGS__ std::invoke_result_t<decltype(&Class::Type::xproperty_##name##_type), Class::Type> native_name

#define REFLECTION_METHOD(name) REFLECTION_METHOD_META(name, name)
#define REFLECTION_VIRTUAL_METHOD(name) REFLECTION_METHOD_META(name, name, virtual)
#define REFLECTION_IMAGINARY_METHOD(name) REFLECTION_METHOD_META(name, xmethod##name)

namespace Reflection
{
	using namespace std;

	class Method
	{
	protected:
		struct Unknown;
		using GenericFunc = void (Unknown::*)();
		using InvokerFunc = void (*)();

		GenericFunc func;
		InvokerFunc invoker;
		void* (*caster)(void*) = nullptr;
		Method* (*copy)(const Method*);

		friend class Class;

	public:
		string_view name;
		const type_info* type;
		type_index signature;
		int num_args;

		struct
		{
			string_view name;
			const type_info* type;
		}
		scope;

	private:
		template<typename Return, typename Object, typename... Args>
		static Return Invoker(Object* scope, const GenericFunc* func, Args&&... args)
		{
			using MethodPtr = Return (Object::*)(Args ...);
			MethodPtr method = *(const MethodPtr*)func;
			return (scope->*method)(forward<Args>(args)...);
		}

		template<typename Return, typename... Args, typename... Types>
		Return Invoke(Return (*)(Args ...), void* obj, Types&&... args) const
		{
			using InvokerFunc = Return (*)(void*, const GenericFunc*, Args&&...);
			InvokerFunc invoker = *(InvokerFunc*)&this->invoker;
			return invoker(obj, &func, std::forward<Types>(args)...);
		}

	public:
		template<typename MethodMeta>
		Method(MethodMeta* meta) : Method(meta->Func())
		{
			name = meta->Name();
			type = &typeid(decltype(meta->Func()));
			scope.name = MethodMeta::Scope::Class::Name();
			scope.type = &typeid(typename MethodMeta::Scope);
			copy = MethodMeta::Scope::Class::Copy;
		}

		template<typename Return, typename Object, typename... Args>
		Method(Return (Object::*method)(Args ...)) : signature(typeid(Return (Args ...)))
		{
			(decltype(method)&)func = method;
			auto invoker = &Method::Invoker<Return, Object, Args...>;
			this->invoker = *(InvokerFunc*)&invoker;
			num_args = sizeof...(Args);
		}

		template<typename Signature, typename Object, typename... Args>
		auto Invoke(const Object& obj, Args ... args) const
		{
			if (signature != typeid(Signature))
				throw logic_error(format("Signature mismatch calling '{}' in '{}'", name, scope.name));
			return Invoke((Signature*)nullptr, GetDomain(obj), std::forward<Args>(args)...);
		}

		template<typename Return, typename Object, typename... Args>
		Return Invoke(const Object& obj, Args ... args) const
		{
			return Invoke<Return (Args ...)>(obj, std::forward<Args>(args)...);
		}

		template<typename Type>
		void* GetDomain(const Type& obj) const
		{
			void* ptr = obj.GetThis();
			return caster ? caster(ptr) : ptr;
		}

		virtual ~Method() = default;
	};

	template<typename... Types>
	struct CombineMethods : Types...
	{
		template<typename MethodMeta>
		CombineMethods(MethodMeta* meta) : Method(meta), Types(meta)... {}
	};
}

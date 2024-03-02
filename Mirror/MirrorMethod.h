#pragma once

#include <typeindex>
#include <type_traits>
#include <format>

#define MIRROR_METHOD_META(_Name_, _NativeName_, _Storage_, ...) \
	xproperty_##_Name_##_type(); \
	struct xproperty_##_Name_##_meta \
	{ \
		using Scope = Class::Type; \
		static std::string_view Name() { using namespace std::literals; return #_Name_##sv; } \
		static auto Func() { return &Class::Type::_NativeName_; } \
		_Storage_ static inline const Mirror::Constructor constructor = +[] \
			{ Class::Construct()->AddMethod(new Class::MethodType((xproperty_##_Name_##_meta*)nullptr)); }; \
	}; \
	static MIRROR_FORCEDSPEC auto xmirror_##_NativeName_##_constructor() { return &xproperty_##_Name_##_meta::constructor; } \
	friend struct xproperty_##_Name_##_meta; \
	__VA_ARGS__ std::invoke_result_t<decltype(&Class::Type::xproperty_##_Name_##_type), Class::Type> _NativeName_

#define MIRROR_METHOD(_Name_, _Storage_) MIRROR_METHOD_META(_Name_, _Name_, _Storage_)
#define MIRROR_VIRTUAL_METHOD(_Name_, _Storage_) MIRROR_METHOD_META(_Name_, _Name_, _Storage_, virtual)
#define MIRROR_IMAGINARY_METHOD(_Name_, _Storage_) MIRROR_METHOD_META(_Name_, xmethod##name, _Storage_)

namespace Mirror
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
			auto _invoker = &Method::Invoker<Return, Object, Args...>;
			invoker = *(InvokerFunc*)&_invoker;
			num_args = sizeof...(Args);
		}

		template<typename Signature, typename Object, typename... Args>
		auto Invoke(const Object& obj, Args ... args) const
		{
			if (signature != typeid(Signature))
				throw logic_error(format("Signature mismatch calling '{}' in '{}'", name, scope.name));
			return Invoke((Signature*)nullptr, GetScope(obj), std::forward<Args>(args)...);
		}

		template<typename Return, typename Object, typename... Args>
		Return Invoke(const Object& obj, Args ... args) const
		{
			return Invoke<Return (Args ...)>(obj, std::forward<Args>(args)...);
		}

		template<typename Type>
		void* GetScope(const Type& obj) const
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

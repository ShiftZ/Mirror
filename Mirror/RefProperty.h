#pragma once

#include <typeinfo>
#include <typeindex>
#include <cassert>
#include <any>

#include "RefTools.h"

#define MIRROR_PROPERTY(property, ...) \
	MIRROR_PROPERTY_DATA(property, __VA_ARGS__); \
	decltype(Mirror::GetReturnType(&Class::Type::xproperty_##property##_type)) property

#define MIRROR_PROPERTY_DATA(property, ...) \
	xproperty_##property##_type(); \
	struct xproperty_##property##_meta \
	{ \
		using Type = decltype(Mirror::GetReturnType(&Class::Type::xproperty_##property##_type)); \
		using Scope = Class::Type; \
		template<typename Type> \
		struct BasicAccess \
		{ \
			static void* Get(void* ptr) { return &((Class::Type*)ptr)->property; } \
			static void Set(void* domain, void* value) { ((Class::Type*)domain)->property = *(Type*)value; } \
			static void Move(void* domain, void* value) { ((Class::Type*)domain)->property = std::move(*(Type*)value); } \
			static string_view Text() { return ""; } \
		}; \
		struct Access : public BasicAccess<Type> { __VA_ARGS__ }; \
		string_view Name() { return #property; } \
		MIRROR_FORCEDSPEC static void Register() \
		{ \
			Mirror::StaticInstance<Mirror::Executor<&Register>>::instance; \
			xproperty_##property##_meta meta; \
			Class::PropertyType* p = new Class::PropertyType(&meta); \
			Mirror::Class::Instance<Class::Type>()->AddProperty(p); \
		} \
	}; \
	friend struct xproperty_##property##_meta

#define MIRROR_VIRTUAL_PROPERTY(property, ...) \
	xproperty_##property##_type(); \
	struct xproperty_##property##_meta \
	{ \
		using Type = std::invoke_result_t<decltype(&Class::Type::xproperty_##property##_type), Class::Type>; \
		using Scope = Class::Type; \
		struct InvalidAccess \
		{ \
			static void Get(); \
			static void Set(); \
			static void Move(); \
			static string_view Text() { return ""; } \
		}; \
		struct Access : public InvalidAccess { __VA_ARGS__ }; \
		string_view Name() { return #property; } \
		MIRROR_FORCEDSPEC static void Register() \
		{ \
			Mirror::StaticInstance<Mirror::Executor<&Register>>::instance; \
			xproperty_##property##_meta meta; \
			Mirror::Class::Instance<Class::Type>()->AddProperty(new Class::PropertyType(&meta)); \
		} \
	}

#define MIRROR_GETTER(getter) \
	static void* Get(void* ptr) { return Gett((Class::Type*)ptr, &Class::Type::getter); } \
	template<typename Type> \
	static void* Gett(Type* ptr, ...) { return (void*)&ptr->getter(); } \
	static void* Gett(Class::Type* ptr, Type (Class::Type::*)() const) { return Mirror::Temporal::Make<Type>(ptr->getter()); } \
	static void* Gett(Class::Type* ptr, Type (Class::Type::*)()) { return Mirror::Temporal::Make<Type>(ptr->getter()); }

#define MIRROR_SETTER(setter) \
	static void Set(void* domain, void* value) { ((Class::Type*)domain)->setter(*(Type*)value); } \
	static void Move(void* domain, void* value) { ((Class::Type*)domain)->setter(*(Type*)value); }

#define MIRROR_MOVER(mover) \
	static void Move(void* domain, void* value) { ((Class::Type*)domain)->mover(std::move(*(Type*)value)); }

#define MIRROR_TEXT(txt) \
	static string_view Text() { return txt; }

namespace Mirror
{
	using namespace std;

	using Getter = void* (*)(void*);
	using Setter = void (*)(void*, void*);
	using Mover = void (*)(void*, void*);

	class Property
	{
	protected:
		void* (*caster)(void*) = nullptr;
		void* (*castany)(const any&);
		Property* (*copy)(const Property*);

		friend class Class;

	public:
		Getter getter = nullptr;
		Setter setter = nullptr;
		Mover mover = nullptr;
		any (*getany)( void* ) = nullptr;

		string_view name;
		const type_info* type;
		Class* ref_class = nullptr;
		type_index type_id;
		bool copy_constructible, copy_assignable;
		string_view meta_text;

		struct
		{
			string_view name;
			const type_info* type;
			type_index type_id;
		}
		scope;

	public:
		template<typename PropertyMeta>
		Property(PropertyMeta* meta) :
			type_id(typeid(typename PropertyMeta::Type)),
			scope{PropertyMeta::Scope::Class::RawName(), &typeid(typename PropertyMeta::Scope), typeid(typename PropertyMeta::Scope)}
		{
			using Type = typename PropertyMeta::Type;

			name = meta->Name();
			type = &typeid(Type);

			if constexpr (IsReflected<Type>)
				ref_class = Type::Class::GetClass();

			if constexpr (is_convertible_v<decltype(&PropertyMeta::Access::Get), Getter>)
				getter = &PropertyMeta::Access::Get;

			if constexpr (CopyAssignable<Type> && is_convertible_v<decltype(&PropertyMeta::Access::Set), Setter>)
				setter = &PropertyMeta::Access::Set;

			if constexpr (is_move_assignable_v<Type> && is_convertible_v<decltype(&PropertyMeta::Access::Move), Mover>)
				mover = &PropertyMeta::Access::Move;

			if constexpr (CopyConstructible<Type>)
				getany = [](void* ptr){ return make_any<Type>(*(Type*)ptr); };

			castany = [](const any& value) -> void* { return (void*)&any_cast<const Type&>(value); };
			copy = PropertyMeta::Scope::Class::Copy;

			copy_constructible = CopyConstructible<Type>;
			copy_assignable = CopyAssignable<Type>;

			meta_text = PropertyMeta::Access::Text();
		}

		template<typename ValueType, typename Type>
		ValueType& GetValue(const Type& obj) const
		{
			return GetValue<ValueType>(obj.GetThis());
		}

		template<typename ValueType>
		ValueType& GetValue(void* ptr) const
		{
			assert(type_index(*type) == typeid(ValueType));
			if (caster) ptr = caster(ptr);
			return *(ValueType*)getter(ptr);
		}

		template<typename Type>
		void* GetPointer(const Type& obj) const
		{
			return GetPointer(obj.GetThis());
		}

		void* GetPointer(void* ptr) const
		{
			if (caster) ptr = caster(ptr);
			return getter(ptr);
		}

		template<typename ValueType, typename Type>
		void SetValue(Type& obj, ValueType&& value) const
		{
			SetValue<ValueType>(obj.GetThis(), forward<ValueType>(value));
		}

		template<typename ValueType>
		void SetValue(void* ptr, ValueType&& value) const
		{
			assert(type_index(*type) == typeid(ValueType));
			if (caster) ptr = caster(ptr);
			if constexpr (is_rvalue_reference_v<ValueType&&>)
				mover(ptr, &value);
			else
				setter(ptr, &value);
		}

		template<typename Type>
		any GetAny(const Type& obj) const { return getany(GetPointer(obj.GetThis())); }

		template<typename Type>
		void SetAny(Type& obj, const any& value) { setter(GetScope(obj), castany(value)); }

		template<typename Type>
		void SetAny(Type& obj, any&& value) { mover(GetScope(obj), castany(value)); }

		any GetAny(void* ptr) const { return getany(GetPointer(ptr)); }

		void SetAny(void* ptr, const any& value)
		{
			if (caster) ptr = caster(ptr);
			setter(ptr, castany(value));
		}

		void SetAny(void* obj, any&& value)
		{
			if (caster) obj = caster(obj);
			mover(obj, castany(value));
		}

		template<typename Type>
		void* GetScope(const Type& obj) const
		{
			void* ptr = obj.GetThis();
			return caster ? caster(ptr) : ptr;
		}

		virtual ~Property() = default;
	};

	template< typename... Types >
	struct CombineMeta : Types...
	{
		template< typename PropertyMeta >
		CombineMeta(PropertyMeta* meta) : Property(meta), Types(meta)... {}
	};

	template<typename Type>
	concept PropertyRange = requires (Type r)
	{
		r.begin(), r.end();
		{*r.begin()} -> convertible_to<const Property*>;
	};
}
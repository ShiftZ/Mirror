#pragma once

#include <typeinfo>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <span>
#include <ranges>

#include "MirrorProperty.h"
#include "MirrorMethod.h"

#ifdef _MSC_VER
#	define MIRROR_FORCEDSPEC __declspec(noinline)
#else
#	define MIRROR_FORCEDSPEC [[noinline]]
#endif

#define MIRROR_CLASS(property_type, method_type, type, ...) MIRROR_STRUCT(property_type, method_type, type, __VA_ARGS__) private:
#define MIRROR_STRUCT(property_type, method_type, type, ...) \
	public: \
	using BaseList = Mirror::GetBaseList<type __VA_ARGS__>; \
	struct Class \
	{ \
		using Type = type; \
		using PropertyType = property_type; \
		using MethodType = method_type; \
		static const char* RawName() { return #type; } \
		static string_view Name() { return Mirror::Class::Instance<type>()->Name(); } \
		static int PropertiesNum() { return Mirror::Class::Instance<type>()->PropertiesNum(); } \
		static auto Properties() { return Mirror::Class::Instance<type>()->Properties<PropertyType>(); } \
		static auto Methods() { return Mirror::Class::Instance<type>()->Methods<MethodType>(); } \
		static const PropertyType* GetProperty(const std::string& name) { return Mirror::Class::Instance<type>()->GetProperty<PropertyType>(name); } \
		static const PropertyType* GetProperty(std::type_index t) { return Mirror::Class::Instance<type>()->GetProperty<PropertyType>(t); } \
		static const PropertyType* GetProperty(int index) { return Mirror::Class::Instance<type>()->GetProperty<PropertyType>(index); } \
		static std::vector<const Mirror::Property*> Resolve( const std::string& path ) { return Mirror::Class::Instance<type>()->Resolve(path); } \
		template<typename PropertyType> \
		static std::vector<const PropertyType*> Resolve(const std::string& path) { return Mirror::Class::Instance<type>()->Resolve<PropertyType>(path); } \
		static const MethodType* GetMethod(const std::string& name) { return Mirror::Class::Instance<type>()->GetMethod<const MethodType>(name); } \
		static auto GetHeirs() { return Mirror::Class::Instance<type>()->GetHeirs(); } \
		static auto GetBases() { return Mirror::Class::Instance<type>()->GetBases(); } \
		static Mirror::Class* GetClass() { return Mirror::Class::Instance<type>(); } \
		static Mirror::Property* Copy(const Mirror::Property* p) { return new PropertyType(*dynamic_cast<const PropertyType*>(p)); } \
		static Mirror::Method* Copy(const Mirror::Method* m) { return new MethodType(*dynamic_cast<const MethodType*>(m)); } \
	}; \
	Mirror::Class* GetClass() const { return Mirror::Class::Instance<type>(); } \
	void* GetThis() const { return (void*)this; } \
	static MIRROR_FORCEDSPEC void xmirror_class() \
	{ \
		Mirror::StaticInstance<Mirror::Executor<&xmirror_class>>::instance; \
		Mirror::Class::Instance<type>(); \
	}

#define MIRROR_MULTIBASE(...) , Mirror::TypeList<__VA_ARGS__>

namespace Mirror
{
	using namespace std;

	template<typename Type>
	concept HasClass = requires { typename Type::Class; };
	
	template<typename Type>
	struct GetReflectedBaseList
	{
		using BaseList = TypeList<>;
	};

	template<typename Type> requires HasClass<Type>
	struct GetReflectedBaseList<Type>
	{
		using BaseList = TypeList<typename Type::Class::Type>;
	};

	template<typename Type, typename BaseList = void>
	using GetBaseList = conditional_t<is_void_v<BaseList>, typename GetReflectedBaseList<Type>::BaseList, BaseList>;

	template<typename From, typename To>
	void* StaticCaster(void* ptr){ return (To*)(From*)ptr; }

	// Interface for abstract usage
	class IMirror
	{
	public:
		virtual Class* GetClass() const = 0;
		virtual void* GetThis() const = 0;
		virtual ~IMirror() = default;
	};

	class Class
	{
		const type_info* type;
		vector<const Property*> properties;
		vector<const Method*> methods;
		unordered_multimap<string_view, const Property*> prop_map;
		unordered_multimap<type_index, const Property*> type_map;
		unordered_multimap<string_view, const Method*> meth_map;

		vector<Class*> bases, heirs;
		unordered_map<const Class*, void*(*)(void*)> casters;
		
		string_view name;
		
		void* (*make_default)() = nullptr;
		IMirror* (*make_reflected)() = nullptr;

	private:
		template<typename Type, typename Base, typename... Others>
		void Construct(TypeList<Base, Others...>*, int level = 0)
		{
			Construct<Type>((typename Base::BaseList*)nullptr, level - 1);

			Class* base = Class::Instance<Base>();
			if (ranges::find(base->heirs, this) == base->heirs.end())
			{
				void* (*caster)(void*) = nullptr;
				if constexpr (std::is_convertible_v<Type*, Base*>)
					caster = &StaticCaster<Type, Base>;
				
				base->heirs.push_back(this);
				if (caster) base->casters.emplace(this, caster);

				for (const Property* property : base->Properties())
				{
					if (!property->caster)
					{
						Property* copy = property->copy(property);
						copy->caster = caster;
						prop_map.emplace(property->name, copy);
						type_map.emplace(property->type_id, copy);
						properties.push_back(copy);
					}
				}

				for (const Method* method : base->Methods())
				{
					if (!method->caster)
					{
						Method* copy = method->copy(method);
						copy->caster = caster;
						meth_map.emplace(method->name, copy);
						methods.push_back(copy);
					}
				}
			}

			if (level == 0)
				bases.push_back(base);

			Construct<Type>((TypeList<Others...>*)nullptr, level);
		}

		template<typename>
		void Construct(TypeList<>*, int = 0) {}

	public:
		template<typename Type>
		static Class* Instance()
		{
			static Class* instance = nullptr;
			if (!instance) instance = new Class((Type*)nullptr);
			return instance;
		}

	public:
		template<typename Type>
		Class(Type* = nullptr)
		{
			type = &typeid(Type);
			name = Type::Class::RawName();
			
			if constexpr (DefaultConstructible<Type>)
			{
				make_default = []()->void* { return new Type(); };

				if constexpr (is_base_of_v<IMirror, Type>)
					make_reflected = []()->IMirror* { return new Type(); };
			}

			Construct<Type>((typename Type::BaseList*)nullptr);
		}

		// Type name
		string_view Name() const { return name; }

		// const char* name
		const char* RawName() const { return name.data(); }

		// Number of properties
		int PropertiesNum() const { return (int)properties.size(); }

		// Number of methods
		int MethodsNum() const { return (int)methods.size(); }

		// Get range of all properties
		auto Properties() const { return span(properties.begin(), properties.end()); }

		// Get range of all methods
		auto Methods() const { return span(methods.begin(), methods.end()); }

		// Get property by name
		const Property* GetProperty(string_view property_name) const
		{
			auto property = prop_map.find(property_name);
			return (property != prop_map.end()) ? property->second : nullptr;
		}

		// Get property by name. Property pointer is cast to PropertyType.
		template<typename PropertyType>
		const PropertyType* GetProperty(string_view name) const
		{
			return dynamic_cast<const PropertyType*>(GetProperty(name));
		}

		// Get property by index
		const Property* GetProperty(int index) const
		{
			return properties[index];
		}

		// Get property by index. Property pointer is cast to PropertyType.
		template<typename PropertyType>
		const PropertyType* GetProperty(int index) const
		{
			return dynamic_cast<const PropertyType*>(properties[index]);
		}

		// Get the first property found of the specified type
		const Property* GetProperty(type_index property_type)
		{
			auto property = type_map.find(property_type);
			return (property != type_map.end()) ? property->second : nullptr;
		}

		// Get the first property found of the specified type
		template<typename PropertyType>
		const PropertyType* GetProperty(type_index type)
		{
			return dynamic_cast<const PropertyType*>(GetProperty(type));
		}

		// Get range of properties of the specified type
		auto GetProperties(type_index property_type)
		{
			auto range = type_map.equal_range(property_type);
			return ranges::subrange(range.first, range.second) | views::values;
		}

		// Get range of properties of the specified type
		template<typename PropertyType>
		auto GetProperties(type_index type)
		{
			return GetProperties(type) | ranges::views::transform([](const Property* p){ return dynamic_cast<PropertyType*>(p); });
		}

		// Resolve path to a property in nested structures
		vector<const Property*> Resolve(string_view path)
		{
			return Resolve<Property>(path);
		}

		// Resolve path to a property in nested structures
		template<typename PropertyType>
		vector<const PropertyType*> Resolve(string_view path)
		{
			vector<const PropertyType*> result;
			size_t size = ranges::count(path, '.');
			result.reserve(size + 1);
			string property_name;
			property_name.reserve(path.size());

			Class* cls = this;
			for (size_t off, pos = 0;; pos = off + 1)
			{
				off = path.find('.', pos);
				property_name.assign(path, pos, off - pos);
				const PropertyType* property = cls->GetProperty<PropertyType>(property_name);
				if (!property) return {};
				result.push_back(property);
				if (off == string::npos) return result;
				if (!property->ref_class) return {};
				cls = property->ref_class;
			}
		}

		// Get method by name
		template<typename MethodType>
		const MethodType* GetMethod(string_view name) const
		{
			return dynamic_cast<const MethodType*>(GetMethod(name));
		}

		// Get method by name
		const Method* GetMethod(string_view property_name) const
		{
			auto method = meth_map.find(property_name);
			return (method != meth_map.end()) ? method->second : nullptr;
		}

		// Get a range of properties converted to PropertyType
		template<typename PropertyType>
		auto Properties() const
		{
			auto transform = views::transform([](const Property* p){ return dynamic_cast<const PropertyType*>(p); });
			auto not_null = views::filter([](const PropertyType* p){ return p != nullptr; });
			return properties | transform | not_null;
		}

		// Get a range of methods converted to MethodType
		template<typename MethodType>
		auto Methods() const
		{
			auto transform = views::transform([](const Method* m){ return dynamic_cast<const MethodType*>(m); });
			auto not_null = views::filter([](const MethodType* m){ return m != nullptr; });
			return methods | transform | not_null;
		}

		template<typename PropertyType>
		void AddProperty(PropertyType* property)
		{
			for (auto heir = heirs.begin(); heir != heirs.end(); ++heir)
			{
				PropertyType* copy = new PropertyType(*property);
				copy->caster = casters[*heir];

				auto& properties = (*heir)->properties;
				auto place = ranges::find_if(properties.rbegin(), properties.rend(), [&](auto& p){ return p->scope.type == type; }).base();
				if (place == properties.begin())
				{
					place = properties.end();
					for (auto it = heirs.begin(); place == properties.end() && it != heir + 1; ++it)
						place = ranges::find_if(properties, [&](auto& p){ return p->scope.type == (*it)->type; });
				}

				(*heir)->prop_map.emplace(copy->name, copy);
				(*heir)->type_map.emplace(copy->type_id, copy);
				(*heir)->properties.insert(place, copy);
			}

			prop_map.emplace(property->name, property);
			type_map.emplace(property->type_id, property);
			properties.push_back(property);
		}

		template<typename MethodType>
		void AddMethod(MethodType* method)
		{
			for (auto heir = heirs.begin(); heir != heirs.end(); ++heir)
			{
				MethodType* copy = new MethodType(*method);
				copy->caster = casters[*heir];

				auto& methods = (*heir)->methods;
				auto place = ranges::find_if(methods.rbegin(), methods.rend(), [&](auto& p){ return p->scope.type == type; }).base();
				if (place == methods.begin())
				{
					place = methods.end();
					for (auto it = heirs.begin(); place == methods.end() && it != heir + 1; ++it)
						place = find_if(methods.begin(), methods.end(), [&](auto& p){ return p->scope.type == (*it)->type; });
				}

				(*heir)->meth_map.emplace(copy->name, copy);
				(*heir)->methods.insert(place, copy);
			}

			meth_map.emplace(method->name, method);
			methods.push_back(method);
		}

		// Get range of derived classes
		auto GetHeirs() const { return span(heirs.begin(), heirs.end()); }

		// Get range of base classes
		auto GetBases() const { return span(bases.begin(), bases.end()); }

		// Get base or null if no base or multiple
		const Class* GetBase() const { return bases.size() == 1 ? bases.back() : nullptr; }

		// Get native type info
		const type_info* GetType() const { return type; }

		// Create an instance of the class if possible
		template<typename Type>
		Type* MakeNew() const { return make_default ? (Type*)make_default() : nullptr; }

		// Create an instance of the class if possible and return a pointer on interface
		IMirror* MakeReflected() const { return make_reflected ? make_reflected() : nullptr; }

		void* Cast(Class* heir, void* ptr) const
		{
			auto it = casters.find(heir);
			return it != casters.end() ? it->second(ptr) : nullptr;
		}

		~Class()
		{
			for (const Property* p : properties) delete p;
			for (const Method* m : methods) delete m;
		}
	};

	// Copy object's properties one by one using getter and setter
	template<typename Type, typename DstType>
	void Copy(DstType& dest, const Type& src)
	{
		Type& dst = dest;

		for (const Property* psrc : src.GetClass()->Properties())
		{
			const Property* pdst = dst.GetClass()->GetProperty(psrc->name);
			assert(pdst->setter && pdst->scope.type_id == psrc->scope.type_id);

			void* value = psrc->getter(psrc->GetScope(src));
			pdst->setter(pdst->GetScope(dst), value);
		}
	}

	// Set value by name
	template<typename Type, typename ValueType>
	void SetValue(Type& obj, string_view name, ValueType&& value)
	{
		const Property* property = obj.GetClass()->GetProperty(name);
		property->SetValue(obj, forward<ValueType>(value));
	}

	// Set value for nested structures using range of properties as path
	template<typename Type, PropertyRange Path, typename ValueType>
	void SetValue(Type& obj, const Path& path, ValueType&& value)
	{
		SetValue(obj.GetThis(), path, forward<ValueType>(value));
	}

	// Set value for nested structures using range of properties as path
	template<PropertyRange Path, typename ValueType>
	void SetValue(void* ptr, const Path& path, ValueType&& value)
	{
		assert(!path.empty());
		auto p = path.begin();
		for (size_t i = 0; i < path.size() - 1; ++i, ++p)
			ptr = (*p)->GetPointer(ptr);
		(*p)->SetValue(ptr, forward<ValueType>(value));
	}

	// Set value for nested structures using string path
	template<typename Type, typename ValueType>
	void SetNestedValue(Type& obj, string_view path, ValueType&& value)
	{
		SetValue(obj.GetThis(), obj.GetClass()->Resolve(path), forward<ValueType>(value));
	}

	// Get value by name
	template<typename ValueType, typename Type>
	ValueType& GetValue(Type& obj, string_view name)
	{
		const Property* property = obj.GetClass()->GetProperty(name);
		return property->GetValue<ValueType>(obj);
	}

	template<typename ValueType, typename Type, PropertyRange Path>
	ValueType& GetValue(Type& obj, const Path& path)
	{
		return GetValue(obj.GetThis(), path);
	}

	template<typename ValueType, PropertyRange Path>
	ValueType& GetValue(void* ptr, const Path& path)
	{
		assert(!path.empty());
		auto p = path.begin();
		for (size_t i = 0; i < path.size() - 1; ++i, ++p)
			ptr = (*p)->GetPointer(ptr);
		return (*p)->template GetValue<ValueType>(ptr);
	}

	template<typename ValueType, typename Type>
	ValueType& GetNestedValue(Type& obj, string_view path)
	{
		return GetValue<ValueType>(obj.GetThis(), obj.GetClass()->Resolve(path));
	}

	// Find pointer to a nested structure following the specified properties
	template<typename PropertyIter>
	auto Resolve(void* ptr, PropertyIter begin, PropertyIter end) -> pair<void*, decltype(*begin)>
	{
		PropertyIter p = begin;
		while (++begin != end)
		{
			ptr = (*p)->GetPointer(ptr);
			p = begin;
		}
		return {ptr, *p};
	}

	// Find pointer to a nested structure following the specified properties
	template<typename PropertyIter>
	auto Resolve(IMirror* obj, PropertyIter begin, PropertyIter end) -> pair<void*, decltype(*begin)>
	{
		return Resolve(obj->GetThis(), begin, end);
	}

	// Find pointer to a nested structure following the specified path
	template<typename PropertyType>
	pair<void*, const PropertyType*> Resolve(void* ptr, Class* cls, string_view path)
	{
		vector<const PropertyType*> ps = cls->Resolve(path);
		if (ps.empty()) return {nullptr, nullptr};
		for (size_t i = 0; i < ps.size() - 1; i++)
			ptr = ps[i]->GetPointer(ptr);
		return {ptr, ps.back()};
	}

	inline pair<void*, const Property*> Resolve(void* ptr, Class* cls, string_view path)
	{
		return Resolve<Property>(ptr, cls, path);
	}

	// Find pointer to a nested structure following the specified path
	template<typename PropertyType>
	pair<void*, PropertyType*> Resolve(IMirror* obj, string_view path)
	{
		return Resolve<PropertyType>(obj->GetThis(), obj->GetClass(), path);
	}

	// Find pointer to a nested structure following the specified path
	inline pair<void*, const Property*> Resolve(IMirror* obj, string_view path)
	{
		return Resolve(obj->GetThis(), obj->GetClass(), path);
	}
}
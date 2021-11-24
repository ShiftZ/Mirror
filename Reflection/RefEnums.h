#pragma once

#include <vector>
#include <unordered_map>
#include <optional>
#include <cassert>

#include "RefTools.h"

#define REFLECTION_ENUM(type, ...) __VA_ARGS__ }; \
	REFLECTION_FORCEDSPEC inline void xreflection_##type##() \
	{ \
		void* execute = &Reflection::StaticInstance<Reflection::Executor<&xreflection_##type>>::instance; \
		struct Values { int64_t __VA_ARGS__; }; \
		Reflection::Enum<type>::Instance().Construct<Values>(#type, #__VA_ARGS__);

#define REFLECTION_ENUM_EXTERNAL(type, ...) \
	REFLECTION_FORCEDSPEC inline void xreflection_##type##() \
	{ \
		void* execute = &Reflection::StaticInstance<Reflection::Executor<&xreflection_##type>>::instance; \
		struct Values { int64_t __VA_ARGS__; }; \
		Reflection::Enum<type>::Instance().Construct<Values>(#type, #__VA_ARGS__); \
	}

namespace Reflection
{
	using namespace std;

	template<typename Type>
	struct Enum
	{
		string_view name;
		vector<string_view> names;
		vector<Type> values;
		unordered_map<string_view, Type> to_enum;
		unordered_map<Type, string_view> to_string;

		static Enum& Instance()
		{
			static Enum<Type> instance;
			return instance;
		}

		template<typename ValuesType>
		void Construct(string_view name, string_view text)
		{
			this->name = name;

			constexpr int num = sizeof(ValuesType) / sizeof(int64_t);

			int64_t vals[num], inversed[num];
			new (vals) ValuesType;

			memcpy(inversed, vals, sizeof(inversed));
			for (int64_t& value : inversed) value = ~value;
			new (inversed) ValuesType;

			for (int64_t value = 0, i = 0; i < num; i++)
			{
				if (vals[i] != inversed[i]) vals[i] = value;
				value = vals[i] + 1;
			}

			for (size_t off = 0, next; off != string::npos;)
			{
				off = text.find_first_not_of(", ", off);
				if (off == string::npos) break;
				next = text.find_first_of(" =,", off);
				string_view name = text.substr(off, next != string::npos ? next - off : string::npos);
				off = text.find(',', next);

				names.push_back(name);
			}

			assert(names.size() == num);

			for (int i = 0; i < num; i++)
			{
				Type value = Type(vals[i]);
				values.push_back(value);
				to_enum.emplace(names[i], value);
				to_string.emplace(value, names[i]);
			}
		}

		// Convert value to string. Throws if value is invalid.
		static string_view ToString(Type value)
		{
			Enum& meta = Instance();
			auto str = meta.to_string.find(value);
			if (str == meta.to_string.end())
				throw Exception("%s: Invalid ToString conversion of (%d)", meta.name.data(), int(value));
			return str->second;
		}

		// Conver value to string. Doesn't throw.
		static optional<string_view> GetString(Type value)
		{
			Enum& meta = Instance();
			auto str = meta.to_string.find(value);
			return (str != meta.to_string.end()) ? optional(str->second) : std::nullopt;
		}

		// Get value by name. Throws if name is invalid
		static Type ToValue(string_view name)
		{
			Enum& meta = Instance();
			auto val = meta.to_enum.find(name);
			if (val == meta.to_enum.end())
				throw Exception("%s: Invalid ToValue conversion of (%s)", meta.name.data(), name.data());
			return val->second;
		}

		// Get value by name. Doesn't throw
		static optional<Type> GetValue(string_view name)
		{
			Enum& meta = Instance();
			auto val = meta.to_enum.find(name);
			return (val != meta.to_enum.end()) ? optional(val->second) : std::nullopt;
		}

		// Check if name is valid
		static bool IsValue(string_view name)
		{
			Enum& meta = Instance();
			return meta.to_enum.find(name) != meta.to_enum.end();
		}

		// Get name of the enum
		static string_view GetName() { return Instance().name; }

		// Get span of enum value names
		static auto GetNames()
		{
			Enum& meta = Instance();
			return span(meta.names.begin(), meta.names.end());
		}

		// Get span of enum values
		static auto GetValues()
		{
			Enum& meta = Instance();
			return span(meta.values.begin(), meta.values.end());
		}

		// Get number of enum values
		static int Num() { return Instance().values.size(); }
	};
}
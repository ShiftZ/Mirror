#pragma once

#include <vector>
#include <unordered_map>
#include <optional>
#include <format>

#define MIRROR_ENUM(_Type_, _Storage_, ...) __VA_ARGS__ }; \
	struct xmirror_##_Type_##_meta \
	{ \
		struct Values { int64_t __VA_ARGS__; }; \
		static inline _Storage_ Mirror::TEnum<_Type_, Values> instance = Mirror::TEnum<_Type_, Values>(#_Type_, #__VA_ARGS__); \
	}; \
	MIRROR_FORCEDSPEC static void* xmirror_##_Type_##_constructor() \
	{ \
		Mirror::Enum<_Type_>::instance = &xmirror_##_Type_##_meta::instance; \
		return &Mirror::StaticInstance<Mirror::Executor<&xmirror_##_Type_##_constructor>>::instance;

#define MIRROR_ENUM_EXTERNAL(_Type_, _Storage_, ...) \
	struct xmirror_##_Type_##_meta \
	{ \
		struct Values { int64_t __VA_ARGS__; }; \
		static inline _Storage_ Mirror::TEnum<_Type_, Values> instance = Mirror::TEnum<_Type_, Values>(#_Type_, #__VA_ARGS__); \
	}; \
	MIRROR_FORCEDSPEC static void* xmirror_##_Type_##_constructor() \
	{ \
		Mirror::Enum<_Type_>::instance = &xmirror_##_Type_##_meta::instance; \
		return &Mirror::StaticInstance<Mirror::Executor<&xmirror_##_Type_##_constructor>>::instance; \
	}

namespace Mirror
{
	using namespace std;

	template<typename Type> requires is_enum_v<Type>
	struct Enum
	{
		string_view name;
		vector<string> names;
		vector<Type> values;
		unordered_map<string_view, Type> to_enum;
		unordered_map<Type, string_view> to_string;

		static inline Enum* instance;

		// Convert value to string. Throws if value is invalid.
		static string_view ToString(Type value)
		{
			auto str = instance->to_string.find(value);
			if (str == instance->to_string.end())
				throw logic_error(format("{} enum: invalid ToString conversion of ({})", instance->name, int(value)));
			return str->second;
		}

		// Conver value to string. Doesn't throw.
		static optional<string_view> GetString(Type value)
		{
			auto str = instance->to_string.find(value);
			return (str != instance->to_string.end()) ? optional(str->second) : nullopt;
		}

		// Get value by name. Throws if name is invalid
		static Type ToValue(string_view name)
		{
			auto val = instance->to_enum.find(name);
			if (val == instance->to_enum.end())
				throw logic_error(format("{} enum: invalid ToValue conversion of '{}'", instance->name, name));
			return val->second;
		}

		// Get value by name. Doesn't throw
		static optional<Type> GetValue(string_view name)
		{
			auto val = instance->to_enum.find(name);
			return (val != instance->to_enum.end()) ? optional(val->second) : nullopt;
		}

		// Check if name is valid
		static bool IsValue(string_view name)
		{
			return instance->to_enum.contains(name);
		}

		// Get name of the enum
		static string_view GetName() { return instance->name; }

		// Get span of enum value names
		static auto GetNames()
		{
			return span(instance->names.begin(), instance->names.end());
		}

		// Get span of enum values
		static auto GetValues()
		{
			return span(instance->values.begin(), instance->values.end());
		}

		// Get pairs of {name, value}
		static auto GetPairs()
		{
			return views::all(instance->to_enum);
		}

		// Get number of enum values
		static int Num() { return instance->values.size(); }
	};

	template<typename Type, typename ValuesType>
	struct TEnum : Enum<Type>
	{
		TEnum(string_view name, string_view text)
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
				string_view ename(text.substr(off, next != string::npos ? next - off : string::npos));
				off = text.find(',', next);

				this->names.emplace_back(ename);
			}

			for (int i = 0; i < num; i++)
			{
				Type value = Type(vals[i]);
				this->values.push_back(value);
				this->to_enum.emplace(this->names[i], value);
				this->to_string.emplace(value, this->names[i]);
			}

			this->instance = this;
		}
	};
}
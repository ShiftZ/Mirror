#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>

#include "MirrorExpress.h"
#include "LuaBind.hpp"

using namespace std;
using namespace Mirror;

enum class Color { ENUM(Color, Black, White, Ginger) }; // built-in enum mirror

enum class Expression { Insolent, Shameless, Begging, Fearsome };
XENUM(Expression, Insolent, Shameless, Begging, Fearsome); // external enum mirror

class Animal : public IMirror
{
	LUA_CLASS(Animal)

public:
	int PROPERTY(lifes) = 1;
};

class Furious : public virtual Animal
{
	LUA_CLASS(Furious)

public:
	bool PROPERTY(bites) = true;
	bool PROPERTY(scratches) = true;
	bool VPROPERTY(furious, GETTER(GetFurious) SETTER(SetFurious) METATXT("Virtual properties do not physically exist"));

public:
	void SetFurious(bool furious) { bites = scratches = furious; }
	bool GetFurious() { return bites && scratches; }
	void VMETHOD(Hiss)() { cout << "Hssss\n"; }
};

class Pet : public virtual Animal
{
	LUA_CLASS(Pet)

public:
	string PROPERTY(name);
	bool PROPERTY(good_pet, SETTER(SetGoodPet) GETTER(IsGoodPet)) = false;

public:
	virtual void SetGoodPet(bool value) { good_pet = value; }
	virtual bool IsGoodPet() { return good_pet; }

	void METHOD(Purr)() { cout << "Purrr-purrr\n"; }
};

class Cat : public Furious, public Pet
{
	LUA_CLASS(Cat, MULTIBASE(Furious, Pet))

public:
	struct Snout : IMirror
	{
		LUA_STRUCT(Snout)
		Expression PROPERTY(expression);
	};

	struct Tail : IMirror
	{
		LUA_STRUCT(Tail)
		Color PROPERTY(color);
	};

public:
	Snout PROPERTY(snout);
	Color PROPERTY(color);
	Tail PROPERTY(tail);

public:
	Cat() { lifes = 9; }

	string METHOD(Meow)(int length)
	{
		return string("Me").append(length, 'o') + 'w';
	}

	void Hiss() override
	{
		Furious::Hiss();
		snout.expression = Expression::Fearsome;
	}

	void SetGoodPet(bool good) override
	{
		Pet::SetGoodPet(good);
		good ? Purr() : Hiss();
		bites = scratches = !good;
	}

	bool IsGoodPet() override { return true; } // there is no bad cats
};

Cat* cat = nullptr;

int main()
{
	lua_State* lua = luaL_newstate();

	LuaBindSetter(lua, LuaSetter);
	LuaBindGetter(lua, LuaGetter);
	LuaBindEqual(lua, LuaEqual);
	LuaBindDestructor(lua, LuaDestructor);
	AddEnum<Color, Expression>(lua);

	luaL_requiref(lua, "_G", luaopen_base, 1);
	lua_settop(lua, 0);

	auto get_cat = [](lua_State* lua)
	{
		unique_ptr cat = make_unique<Cat>();
		::cat = cat.get();
		LuaPush(lua, move(cat));
		return 1;
	};

	lua_pushcfunction(lua, lua_CFunction(get_cat));
	lua_setglobal(lua, "GetCat");

	if (luaL_loadfile(lua, "Luabind_Sample.lua") != 0)
	{
		cout << "'luaL_loadfile' failed\n";
		return 0;
	}

	lua_call(lua, 0, 0);

	lua_close(lua);

	return 0;
}

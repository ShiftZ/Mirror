#include <iostream>
#include <string>

#include "RefExpress.h"

using namespace std;
using namespace Reflection;

enum class Color { ENUM(Color, Black, White, Ginger) }; // built-in enum reflection
enum class Expression { Insolent, Shameless, Begging, Fearsome };
XENUM(Expression, Insolent, Shameless, Begging, Fearsome); // external enum reflection

class Animal : public Reflected // this inheritance is not obligatory, it just makes GetClass() method virtual
{
	CLASS(Animal)

public:
    int PROPERTY(lifes) = 1;
};

class Furious : public virtual Animal
{
	CLASS(Furious)

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
	CLASS(Pet)

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
	CLASS(Cat, MULTIBASE(Furious, Pet))

public:
    struct Snout
    {
	    STRUCT(Snout)
        Expression PROPERTY(expression);
    };

	struct Tail
	{
		STRUCT(Tail)
		Color PROPERTY(color);
	};

public:
    Snout PROPERTY(snout);
    Color PROPERTY(color);
    Tail PROPERTY(tail);

public:
    Cat() { lifes = 9; }

	string METHOD(Meow)(int length) { return string("Me").append(length, 'o') + 'w'; }

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

int main()
{
    Class* cat_class = Cat::Class::GetClass();

    Cat alice;
    Animal* animal = &alice;
    cat_class = animal->GetClass(); // refers to the same class as before

    const Property* name = cat_class->GetProperty("name");
    name->SetValue(alice, string("Alice")); // set value using property

	SetValue(alice, "color", Color::Black); // a shortcut for above two lines

    Expression expression = Enum<Expression>::ToValue("Begging"); // convert string to enum value
    SetNestedValue(alice, "snout.expression", expression); // shortcut for setting a nested value

    string_view class_name = cat_class->Name();

	cout << '\n' << class_name << " properties:\n";
    for (const Property* property : cat_class->Properties()) // iterate over class properties
	    cout << property->name << " (" << property->type->name() << ")\n" ;

    cout << '\n' << class_name << " properties of integer type:\n";
    for (const Property* property : cat_class->GetProperties(typeid(int))) // iterate over class properties of int type
	    cout << property->name << ": " << property->GetValue<int>(alice);

    cout << "\n\n" << class_name << " can:\n";
    for (const Method* method : cat_class->Methods()) // iterate over class methods
        cout << method->name << "\n";

    const Method* method = cat_class->GetMethod("Meow");
    string meow = method->Invoke<string (int)>(alice, 3); // invokes a native function
    cout << '\n' << alice.name << " says " << meow;

    cout << "\nGood girl!\n";
    const Property* good_pet = cat_class->GetProperty("good_pet");
    good_pet->SetValue(alice, true); // the virtual setter of the property will be used

    cout << "\nBad bad girl!\n";
    good_pet->SetValue(alice, false); 

    bool good_girl = good_pet->GetValue<bool>(alice); // property has a virtual getter overriden in Cat class
    cout << alice.name << " is a " << (good_girl ? "good" : "bad") << " girl\n";

    const Property* furious = cat_class->GetProperty("furious");
    if (furious->GetValue<bool>(alice)) // getting value of a virtual property
		cout << alice.name << " is furious\n";

    expression = GetNestedValue<Expression>(alice, "snout.expression"); // shortcut for getting a nested value
    cout << alice.name << " expresses " << Enum<Expression>::ToString(expression) << "ness\n"; // convert enum value to a string

    cout << "\nFun fact:\n" << furious->meta_text;

    return 0;
}

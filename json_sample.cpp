// This is an example of json serialization\deserialization implementation with the mirror library
// It uses JsonBox library, but it gives a general idea how to implement any kind of serializer\deserializer

#include <fstream>
#include <filesystem>

#include "json_sample.h"

enum class StarType { ENUM(StarType, YellowDwarf, RedDwarf, RedGiant, RedSuperGiant, BlueGiant, WhiteDwarf, BrownDwarf) };

struct Planet
{
	JSON_STRUCT(Planet)
	string PROPERTY(name);
	int PROPERTY(diameter);
	float PROPERTY(earth_masses);
	bool PROPERTY(habitable) = false;
};

struct Star
{
	JSON_STRUCT(Star)

	StarType PROPERTY(type);
	string PROPERTY(name);
	vector<Planet> PROPERTY(planets);
};

void JsonWrite(const filesystem::path& path)
{
	Star sun;
	sun.name = "Sun";
	sun.type = StarType::YellowDwarf;

	sun.planets = {
		{"Mercury", 4879, 0.055f},
		{"Venus", 12104, 0.815f},
		{"Earth", 12714, 1.0f, true},
		{"Mars", 6755, 0.107f},
		{"Jupiter", 133709, 317.83f},
		{"Saturn", 120536, 95.16f},
		{"Uranus", 49946, 14.536f},
		{"Neptune", 48682, 17.15f}
	};

	JsonBox::Value jval;
	JsonReadWrite<Star>::Write(sun, jval);

	fstream file(path, ios::out);
	jval.writeToStream(file);
}

Star JsonRead(const filesystem::path& path)
{
	fstream file("solar_system.json", ios::in);

	JsonBox::Value jval;
	jval.loadFromStream(file);

	Star star;
	JsonReadWrite<Star>::Read(star, jval);
	return star;
}

int main()
{
	filesystem::path path = "solar_system.json";
	JsonWrite(path);
	Star sun = JsonRead(path);

	return 0;
}
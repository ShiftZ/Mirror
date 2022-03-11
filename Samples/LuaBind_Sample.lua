cat = GetCat() -- lua possess the cat object and will destroy it on lua_close

cat.name = "Alice"
cat.color = Color.Black -- Color is a table and Black is a number in the table. Created with AddEnum.
cat.snout.expression = Expression.Begging

print(cat.name .. " says " .. cat:Meow(3)) -- invokes a native function

print("Good girl!")
cat.good_pet = true -- the virtual setter of the property will be used

print("Bad bad girl!")
cat.good_pet = false -- the virtual setter of the property will be used

if cat.good_pet then -- property has a virtual getter overriden in Cat class
	print(cat.name .. " is good girl")
else
	print(cat.name .. " is bad girl")
end

if cat.furious then -- getting value of a virtual property
	print(cat.name .. " is furious")
end


local function print_collection(collection)
    local ids = db:read_element_ids(collection);

    print("Printing elements in collection '" .. collection .. "' (" .. #ids .. " elements):\n");
    for i, id in ipairs(ids) do
        local element = db:read_element_by_id(collection, id);

        print(collection .. " element with Id " .. id .. ":");
        for key, value in pairs(element) do
            if type(value) == "table" then
                print("\t" .. key .. ": [" .. table.concat(value, ", ") .. "]");
            else
                print("\t" .. key .. ": " .. tostring(value));
            end
        end
        print();
    end
end

-- create
local config = db:create_element("Configuration", { label = "Configuration" });
local item1 = db:create_element("Collection",
    { label = "Item 1", some_integer = 42, some_float = 3.14, value_int = { 1, 2, 3 } });
local item2 = db:create_element("Collection",
    { label = "Item 2", some_integer = 37, some_float = 2.71, value_int = { 2, 3, 4 } });

-- read
print_collection("Collection");

-- update
print("Updating...");
db:update_element("Collection", item1, { some_integer = 999 })
print_collection("Collection");

-- delete
print("Deleting...");
db:delete_element("Collection", item2);
print_collection("Collection");

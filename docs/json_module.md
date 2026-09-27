# JSON Module

Encoding and decoding of JSON data

Methods:

| Name   | Description                                 |
| ------ | ------------------------------------------- |
| decode | Decodes a JSON string                       |
| encode | Encodes the passed value into a JSON string |


### decode()

Decodes the passed JSON string and returns a Lua value or nil in case of an error

Arguments:

- JSON module object [userdata]
- JSON string to decode

Results:

- Result [table|boolean|string|number|nil]
- Error message [string|nil]

Example:

```lua
local app = ...
print(app.json:encode({ id = 1, name = "thoth" }))
```

```sh
$ ./thoth script.lua
{"name": "thoth", "id": 1}
```

### encode()

Encodes the passed value and returns a JSON string or nil in case of an error

Arguments:

- JSON module object [userdata]
- Value to encode [table|boolean|string|number]

Results:

- JSON string [string|nil]
- Error message [string|nil]

Example:

```lua
local app = ...
local result = app.json:decode('{"name": "thoth", "id": 1}')
print('Type:', type(result));
print('Value:')
for key, value in pairs(result) do
	print(key, value)
end
```

```sh
$ ./thoth script.lua
Type:	table
Value:
name	thoth
id	1
```

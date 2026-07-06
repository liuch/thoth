# Thoth application
Just run the executable file, specifying the path to the Lua script as the first parameter.

The specified Lua script will be run immediately, with a Lua service table as the first parameter. You can pass other parameters to the running script by specifying them after the script name. If no parameters are specified, an attempt will be made to run a script named rc.lua in the current directory.

Example:

myscript.lua:
```lua
local app, arg1, arg2 = ...
print(app, arg1, arg2);
print("-----")
for key, value in pairs(app) do print(key, type(value)) end
```

```sh
$ ./thoth myscript.lua arg1 arg2
table: 0x5650ea44b660	arg1	arg2
-----
json	userdata
http	userdata
html	userdata
utils	userdata
globalProtect	function
readline	userdata
_VERSION	string
```

The first parameter passed to the script is a regular key-value table that contains one function and five modules. The remaining parameters are passed as a string without changes.

| Key           | Type     | Description                              |
| ------------- | --------------------------------------------------- |
| \_VERSION     | string   | Application version                      |
| globalProtect | function | Global environment protection management |
| html          | userdata | HTML parser module                       |
| http          | userdata | HTTP client module                       |
| json          | userdata | JSON module                              |
| readline      | userdata | Readline module                          |
| utils         | userdata | Utils module                             |

## Properties

### \_VERSION

The application version as a string.

## Functions

### globalProtect()

By default, all scripts are run with access to undeclared variables prohibited. This function sets and returns the global environment protection status. If no argument is passed, the current protection status is returned. Otherwise, the passed argument is set as the current value and the old value is returned.

Arguments:

- New protect status value. Optional [boolean|nil]

Results:

- The current or the old protect status value

Example:

```lua
local app = ...
if app.globalProtect() then
	print("Protection is active")
else
	print("Protection is inactive")
end

app.globalProtect(false)
globalVariable1 = "Hello!"
app.globalProtect(true)

print(globalVariable1)
print(globalVariable2) -- Undeclared variable
```

```sh
$ ./thoth myscript.lua
Protection is active
Hello!
Failed to run script: @myscript.lua:13: Attempt to access undeclared variable "globalVariable2"
```

## Modules

### html

This module allows you to parse html documents and extracting data from them. The module uses the libxml2 library.

See the html_module.md file for details.

### http

This module allows you to get resources other the HTTP protocol. The module uses the libcurl library.

See the http_module.md file for details.

### json

This module is for encoding and decoding JSON data. The module uses the libjansson library.

See the json_module.md file for details.

### readline

A simple wrapper for the libreadline library.

See the readline_module.md file for details.

### utils

A set of useful utilites.

See the utils_module.md file for details.

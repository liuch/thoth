# The Thoth Project

The project is designed to quickly run Lua scripts with built-in support for HTTP client (curl), XPath (libxml2), JSON and some other functionality.

Please note: the build and functionality have only been tested on Linux.

## Usage

Just run the executable file by specifying the path to a Lua script as the first parameter.
The specified Lua script will be run immediately, with the Lua service table as the first parameter.
You can pass other parameters to the running script by specifying them after the script name.
If no parameters are specified, an attempt will be made to run a script named rc.lua in the current directory.

## Peculiarities
* Low memory consumption
* Support for http, xpath, json, base64, readline
* Protecting access to undeclared global variables

### Examples

Getting a value from HTML page.

script1.lua:
```lua
local app = ...

local resp = app.http:getSocket():setOpt("useragent", "My useragent"):fetch("https://github.com/octocat")
if not resp.ok then
	print(string.format("Error: The server returned %d", resp.status))
	return 1
end

local xres = resp:html():evalXPath("//h1[contains(@class,'vcard-names')]/span[1]/text()")
if xres:size() == 0 then
	print("Element not found!")
	return 1
end

print(string.format("User name: %s", app.utils.trim(xres:result(1):textContent())))
return 0
```

```sh
$ ./thoth script1.lua
User name: The Octocat
```

-----

Getting value from REST API endpoint with a parameter

script2.lua:
```lua
local app, field_name = ...

if not field_name then
	print("Usage: script_name <field_name>")
	return 1
end

local resp = app.http:getSocket():setOpt("useragent", "My useragent"):fetch("https://api.github.com/users/octocat", {
	headers = {
		["Accept"] = "application/vnd.github+json",
		["X-GitHub-Api-Version"] = "2022-11-28"
	}
})
if not resp.ok then
	print(string.format("Error: The server returned %d", resp.status))
	return 1
end

local value = resp:json()[field_name]
if not value then
	print("Wrong field_name!")
	return 1
end

print(string.format("Value: %s", value))
return 0
```

```sh
$ ./thoth script2.lua name
Value: The Octocat
$ ./thoth script2.lua company
Value: @github
```

### Requirements
* liblua 5.3 or higher
* libcurl4 7.83 or higher
* libjansson 4 or higher
* libreadline 7 or higher
* libxml2
* libb64

## Installation and Configuration

Make sure you have installed all dependencies and corresponding *-dev packages.

With git do this:

```sh
$ git clone https://github.com/liuch/thoth.git /tmp/thoth
$ cd /tmp/thoth
$ make
$ sudo make install
```

The executable file will be installed in the `/usr/local/bin` directory

The following environment variables are available:

- PREFIX - Installation directory prefix. Default value: `/usr/local`
- CC - The compiler to use. Default: `gcc`.
- CFLAGS - Compilation flags. Default: `-Wall -Wextra`

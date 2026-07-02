# HTTP Module

This module is designed to access resources over the HTTP protocol. Main features:

- GET and POST requests
- Getting and setting HTTP headers
- Cookie management
- Proxy settings
- User Agent replacement
- Decoding the response into JSON and HTML objects

Methods:

| Name      | Description                    |
| --------- | ------------------------------ |
| getSocket | Makes a new HTTP socket object |

### getSocket()

Makes and returns a new HTTP socket object.

Arguments:

HTTP module object [userdata]

Results:

- HTTP socket object [userdata]

Example:

```lua
local app = ...
local socket = app.http:getSocket()
print(type(socket), getmetatable(socket).__name)
```

```sh
$ ./thoth script.lua
userdata	thoth.socket
```

## HTTP Socket Object

Type: userdata

Methods:

| Name       | Description                                         |
| ---------- | --------------------------------------------------- |
| close      | Frees resources of the socket object                |
| escape     | Converts the given string to an URL endcoded string |
| fetch      | Performs an HTTP request to the specified resource  |
| setCookies | Sets a new cookie jar for the given socket object   |
| setOpt     | Sets socket options                                 |
| unescape   | Converts the URL encoded string to a "plain string" |

Properties:

| Name       | Description                                         |
| ---------- | --------------------------------------------------- |
| queries    | Request counter                                     |

### close()

Closes the socket and frees the resources associated with it. This method can be useful to close the socket immediately. Otherwise, the resources cleanup and the socket closure will be performed later, just before the object is destroed by the garbage collector.

Arguments:

- HTTP socket object [userdata]

Results:

none

Example:

```lua
local app = ...
local socket = app.http:getSocket()
--[[
some code here
--]]
socket:close()
```

### escape()

Converts the given string to an URL encoded string.

Arguments:

- HTTP socket object [userdata]
- String to convert [string]

Results:

- Converted string [string]

### fetch()

Performs and HTTP request to the specified resource.

Arguments:

- HTTP socket object [userdata]
- URL of the resource [string]
- Request parameters [table]. Optional

The third argument may have the following optional keys:

- headers [table]. Key-value list of the request headers;
- method [string]. HTTP request method. The valid values are: "get" or "post". Default: "get";
- body [table|string]. Key value array or string. The body with the MIME type must be presented as a table;
- bodyType [string]. Body type. The valid values are: "fields" or "mime". Default: "fields";
- response [table]. Additional server data that needs to be stored into the response. For instance: { headers = true }. The valid keys are "headers", "cookies", the valid values are true or false;

Results:

- HTTP response object [userdata]

Example:

```lua
local app = ...
local socket = app.http:getSocket()
local result = socket:fetch("https://www.google.com")
print(type(result))
print(getmetatable(result).__name)
print(result.ok)
```

```sh
$ ./thoth script.lua
userdata
thoth.http.response
true
```

### setCookies()

Sets a cookie jar for the given socket object. If there is no cookie jar with given key, a new cookie jar is created and activated. If the given jar key is a nil, the cookie engine is disabled for this socket and it ignores all cookies sent by the server. You can have multiple cookie jars and switch beetween them without terminating the connection.

Arguments:

- HTTP socket object [userdata]
- Jar key [any]. A value of any type can be used as a key. A nil value disables the cookie engine
- Global [bool]. Set to true if the global context (all sockets) is intended to be used for storing the cookie jar. Set to false if it is intended to be used only with the current socket. The default value is false.

Results:

- The old cookie jar key [any]

Example:

```lua
local app = ...
local socket = app.http:getSocket()
socket:setCookies("user1")
local credentials = { username = "user1", password= "secret" }
local result = socket:fetch("localhost", { method = "post", body = credentials })
if result.ok then
	-- other http requests
	--
	print("Done")
else
	-- Error handling
end
```

### setOpt()

Sets the socket object options.

Arguments:

- HTTP socket object [userdata]
- Option name [string]
- Option value [string|nil]. A nil value disables the option

The following options are currently supported:

- "useragent"     - Useragent string. It replaces the value of the HTTP header User-Agent with the given string
- "proxy"         - Sets the proxy to use for transfers with the socket. The proxy value has the following format: scheme://host:port. Example: socks5://127.0.0.1:9050
- "proxyusername" - Sets the username to use with proxy authentication
- "proxypassword" - Sets the password to use with proxy authentication

Results:

- Self [userdata]

Example:

```lua
local app = ...
local socket = app.http:getSocket():setOpt("useragent", "curl"):setOpt("proxy", "socks5://localhost:9050")
local result = socket:fetch("ifconfig.me")
if (result.ok) then
	print(result:text())
end
```

```sh
$ ./thoth script.lua
2001:67c:e60:c0c:192:42:116:107
```

### unescape()

Converts the given URL encoded string to a "plain string".

Arguments:

- HTTP socket object [userdata]
- String to convert [string]

Results:

- Converted string [string]

### queries

A simple request counter that is incremented by one after each request to a resource using the socket. The initial value is 0. You can set your own value at any time.

Type: number

Access mode: read and write

## HTTP Response Object

Type: userdata

Methods:

| Name    | Description                                                      |
| ------- | ---------------------------------------------------------------- |
| cookies | Returns the server's cookies                                     |
| headers | Returns the server's HTTP headers                                |
| html    | Returns the HTML body of the response as an HTML document object |
| json    | Returns the JSON body of the HTTP response as a Lua value        |
| text    | Returns the response body in plain text                          |

### cookies()

Returns a table with server cookies. Note that you need activate the cookie engine and set the cookies field to true for the response table in the request parameters. See the example bellow.

Arguments:

- HTTP response object [userdata]

Results:

- Table of strings or nil if cookies are not available [table|nil]

Example:

```lua
local app = ...
local socket = app.http:getSocket()
socket:setCookies("myCookieJar") -- Activate the cookie engine
local result = socket:fetch("https://www.google.com", { response = { cookies = true } })
if (result.ok) then
	print(type(result:cookies()))
else
	print("Error!")
end
```

```sh
$ ./thoth script.lua
table
```

### headers()

Returns HTTP headers from the server response. If the name parameter is specified, the method returns the header value with the specified name. Note that you need set the headers field to true for the response table in the request parameters. See the example bellow.

Arguments:

- HTTP response object [userdata]
- Сase insensitive header name. Optional [string|nil]

Results:

- Header value or key-value table with keys in lower case [table|string|nil]

Example:

```lua
local app = ...
local socket = app.http:getSocket()
local result = socket:fetch("https://www.google.com", { response = { headers = true } })
if (result.ok) then
	print(type(result:headers()))
else
	print("Error!")
end
```

```sh
$ ./thoth script.lua
table
```
### html()

Returns the HTML body of the server response as an HTML document object. Note that all subsequent calls to this method will return nil.

Arguments:

- HTTP response object [userdata]

Results:

- HTML document object [userdata|nil]

Example:

```lua
local app = ...
local socket = app.http:getSocket()
local result = socket:fetch("https://www.google.com")
if (result.ok) then
	local doc = result:html()
	print(type(doc))
	print(getmetatable(doc).__name)
	print(doc:evalXPath("//head/title/text()"):result(1):textContent())
else
	print("Error!")
end
```

```sh
$ ./thoth script.lua
userdata
thoth.html.doc
Google
```

### json()

Parses the server response and returns a JSON value. Note that all subsequent calls to this method will return nil.

Arguments:

- HTTP response object [userdata]

Results:

- JSON value [table|boolean|string|number|nil]

Example:

```lua
local app = ...
local socket = app.http:getSocket()
local result = socket:fetch(
	"https://api.github.com/users/octocat",
	{ ["Accept"] = "application/vnd.github+json", ["X-GitHub-Api-Version"] = "2022-11-28" }
)
if (result.ok) then
	print(result:json().name)
else
	print("Error!")
end
```

```sh
$ ./thoth script.lua
The Octocat
```
### text()

Returns the response body in plain text. Note that all subsequent calls to this method will return nil.

Arguments:

- HTTP response object [userdata]

Results:

- The response body as a string [string]

Example:

```lua
local app = ...
local socket = app.http:getSocket()
local result = socket:fetch("https://www.google.com")
if (result.ok) then
	print(result:text():sub(1, 50) .. "…")
else
	print("Error!")
end
```

```sh
$ !./thoth script.lua
<!doctype html><html itemscope="" itemtype="http:/…
```

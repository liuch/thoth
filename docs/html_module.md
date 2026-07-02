# HTML Module

This module is designed for working with an HTML tree of nodes: searching for nodes, obtaining properties, and so on.
The module is also used in the html() method of the http module.

Methods:

| Name       | Description                                                |
| ---------- | ---------------------------------------------------------- |
| fromString | Generates a tree of HTML nodes from the passed HTML string |


### loadFromString()

Creates an HTML document from the passed HTML string.

Arguments:

- HTML module object [userdata]
- HTML string to make the document from [string]

Results:

- HTML document object [userdata]

Example:

```lua
local app = ...
local doc = app.html:fromString("<div><span>Hello!</span></div>")
print(type(doc), getmetatable(doc).__name)
```

```sh
$ ./thoth script.lua
userdata	thoth.html.doc
```

## HTML Document Object

Type: userdata

Methods:

| Name            | Description                                        |
| --------------- | -------------------------------------------------- |
| documentElement | Returns the root element
| evalXPath       | Performs a XPath query and returns a query result  |

### documentElement()

Returns the root element of the HTML document

Arguments:

- HTML document object [userdata]

Resuts:

- Node object [userdata|nil]

Example:

```lua
local app = ...
local doc = app.html:fromString("<div><span>Hello!</span></div>")
local el = doc:documentElement()
print(type(el), getmetatable(el).__name)
```

```sh
$ ./thoth script.lua
userdata	thoth.html.node
```

### evalXPath()

Performs a XPath query and returns XPath result object as a query result.

Arguments:

- HTML document object [userdata]
- XPath query [string]

Results:

- XPath result object [userdata]

Example:

```lua
local app = ...
local doc = app.html:fromString("<div><span>Hello!</span></div>")
local res = doc:evalXPath("//div/span")
print(type(res), getmetatable(res).__name)
```

```sh
$ ./thoth script.lua
userdata	thoth.html.xpath
```

## Node Object

Type: userdata

Methods:

| Name        | Description                                        |
| ----------- | -------------------------------------------------- |
| attribute   | Returns the attribute value by its name            |
| attributes  | Returns a key-value list with all the attributes   |
| evalXPath   | Performs a XPath query and returns a query result  |
| name        | Returns the node name                              |
| outerHTML   | Retruns the outer content of the node as HTML text |
| textContent | Returns the content of the node as a text string   |
| type        | Returns the node type as a string                  |


### attribute()

The method returns the value of the attribute with the name passed as a parameter.
The method returns nil if the node is not an element or does not contain the specified attribute.

Arguments:

- Node object [userdata]
- Attribute name [string]

Results:

- Attribute value [string|nil]

Example:

```lua
local app = ...
local doc = app.html:fromString('<div attr1="val1" attr2="val2"></div>')
local el = doc:evalXPath("//div"):result(1)
print(el:attribute("attr1"))
```

```sh
$ ./thoth script.lua
val1
```

### attributes()

The method returns a key-value list of the node attributes or nil if the node is not en element.

Arguments:

- Node object [userdata]

Results:

- List of attributes [table]

Example:

```lua
local app = ...
local doc = app.html:fromString('<div attr1="val1" attr2="val2"></div>')
local el = doc:evalXPath("//div"):result(1)
for n, v in pairs(el:attributes()) do print(n, v) end
```

```sh
$ ./thoth script.lua
attr2	val2
attr1	val1
```

### evalXPath()

Performs a XPath query and returns XPath result object as a query result.

Arguments:

- Node object [userdata]
- XPath query [string]

Results:

- XPath result object [userdata]

### name()

Returns the node name.

Arguments:

- Node object [userdata]

Results:

- Node name [string]

Example:

```lua
local app = ...
local doc = app.html:fromString('<div><span></span></div>')
local el = doc:evalXPath("//div/*"):result(1)
print(el:name())
```

```sh
$ ./thoth script.lua
span
```

### outerHTML()

Return the outer content of the node object as HTML text.

Arguments:

- Node object [userdata]

Results:

- Node name [string]

```lua
local app = ...
local doc = app.html:fromString('<div><span>Hello!</span></div>')
print(doc:documentElement():outerHTML())
```

```sh
$ ./thoth script.lua
<html><body><div><span>Hello!</span></div></body></html>
```

### textContent()

Returns the text content of the node for one of these types: "element", "text", "attribute", and "comment". For all other types a nil will be returned. If the type is "element", the method returns its own text content and contents of its nested elements.

Arguments:

- Node object [userdata]

Results:

- Text content [string]

Example:

```lua
local app = ...
local doc = app.html:fromString('<div><span>Hello!</span></div>')
print(doc:documentElement():textContent())
```

```sh
$ ./thoth script.lua
Hello!
```

### type()

Returns the node type. The possible values: "element", "text", "attribute", "cdata", "comment".

Arguments:

- Node object [userdata]

Results:

- Node type [string|nil]. If the type is unknown, it returns nil.

Example:

```lua
local app = ...
local doc = app.html:fromString('<div><span attr1="val1">Hello!</span></div>')
print(doc:evalXPath("//span"):result(1):type())
print(doc:evalXPath("//span/@attr1"):result(1):type())
print(doc:evalXPath("//span/text()"):result(1):type())
```

```sh
$ ./thoth script.lua
element
attribute
text
```

## XPath Result Object

Type: userdata

Methods:

| Name    | Description                                                         |
| ------- | ------------------------------------------------------------------- |
| result  | Returns the specified value or all the values from the XPath result |
| size    | Returns the number of elements int the node set of the XPath result |
| type    | Returns the type of XPath result                                    |

### result()

Returns the specified query result or a table with all the query results.

Arguments:

- XPath result object [userdata]
- Result index [number]. Optional

Results:

- Query results [table|string|number|boolean|userdata|nil]. If the second argumen is nil, a table with all the results is returned. If the result type is not supported, a nil is returned.

Example:

```lua
local app = ...
local doc = app.html:fromString('<div><span attr1="val1">Hello!</span></div>')
print(type(doc:evalXPath("//*"):result(1)))
print(type(doc:evalXPath("//*"):result()))
```

```sh
$ ./thoth script.lua
userdata
table
```

### size()

Returns the number of nodes returned by XPath query. If the query result is not a list of nodes, the method returns 0.

Arguments:

- XPath result object [userdata]

Results:

- Number of nodes [number]

Example:

```lua
local app = ...
local doc = app.html:fromString('<div><span/><span/><span/></div>')
print(doc:evalXPath("//span"):size())
```

```sh
$ ./thoth script.lua
3
```

### type()

Returns the type of XPath result

Arguments:

- XPath result object [userdata]

Results:

- The result type [string]. The possible values: "nodeset", "boolean", "number", "string"

Example:

```lua
local app = ...
local doc = app.html:fromString('<div><span/><span/><span/></div>')
print(doc:evalXPath("//span"):type())
print(doc:evalXPath("count(//span)"):type())
print(doc:evalXPath("string(//span[1])"):type())
print(doc:evalXPath("contains(//span[1],'Hi')"):type())
```

```sh
$ ./thoth script.lua
nodeset
number
string
boolean
```

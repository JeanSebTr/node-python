# node-python binding 

python bridge for nodejs!

Hyper-beta, don't hesitate to try and report bugs!

[![Build Status](https://travis-ci.org/JeanSebTr/node-python.png)](https://travis-ci.org/JeanSebTr/node-python)

## Installation

For the module to compile, you need cpython's development headers.
Example for Ubuntu : run ```sudo apt-get install python-dev```

Then simply:
```npm install node-python```

## Usage

```javascript

// python stuff
var python = require('node-python');
var os = python.import('os');

// nodejs stuff
var path = require('path');

assert(os.path.basename(os.getcwd()) == path.basename(process.cwd()))

```

You should now go have fun with that and make it brokes :)

## Current status

What should work:

* Conversion between None and Undefined
* Conversion between Python's and Node's Boolean
* Conversion between Python's and Node's String
* Calling python functions from node
* Conversion from Python's Array to Node's Array

What may be broken:

* Losing precision from Python's 64 bits Integer to Node's Number
* If you're using node v0.6.x (please upgrade) you'll have to manually compile with node-gyp

What's to be done:

* Conversion from Node's Array to Python's Array
* Pass javascript object to python
* Call javascript function from python

What would be realy awesome:

* Proper object introspection


## History

* **v0.0.4** : 2013-10-09
  - use the bindings module to load the native extension
* **v0.0.3** : 2013-07-06
  - Refactor
  - Better type conversion & error handling
  - Compilation now properly working on both OSX and Linux. Windows compilation _may_ work too
* **v0.0.2** : 2012-12-21
  - Forked from [chrisdickinson/node-python](https://github.com/chrisdickinson/node-python)
  - Compilation with node-gyp

HTTP C Parser
=============

For the Alica Project glory.

Run `python setup.py build` to build shared object library file for python. Stored in `build/` directory.

Handling Request
================

There are some specific handler,
  * HTTP Header Request Parser (request class)
  * HTTP POST Multipart (multipart class)
Currently, the other method still in development.

request class
------------

Argument generated after `http_parser.parser.fetch_data` is called,
  * `method`, HTTP Request method
  * `URI`, request URI
  * `URL`, request URL
  * `Query`, from URI. Remember that `URI` = `/path/something?query`
  * `version`, HTTP version. (eg. `HTTP/1.1`)
  * `header`, all request header stored here.
    eg. `Content-Disposition`, `Cookie`, `Host`


Method,
  * `write( buffer )`, accept data string
  * `set_writeback( writeback )`, set response receiver. (eg. `connection.sendall`)
  * `set_client( client )`, set client info. (eg. `"192.168.1.12:47882"`)
    (NEW)
  * `write_from_socket( con_fd )`, accept data from socket. (`connection.fileno()`)
    (REMOVED)
  * `fetch_data`, Fetching data from stream. (REMOVED)
    Warning: must be called once, no checking for that.
    (REMOVED)
  * `is_error()`, check is request error or not. If error "Bad Request" response must be (normally) send.
    (REMOVED)

Steps,
  * Set writeback, `set_writeback`
  * Set writeback, `set_client` (optional)
  * Receive data through `write`.
  * Get body handler through `routing` (self defined).
    Return Object handler
  * Call object handler `write` method (if containing body part), until all body transfered.
  * At the end, call object handler `write_end` method.

```python
import http_parser

class ClientRequest(http_parser.request):
  def routing(self):
    # first setup here
    # processing HTTP Header
    # return Request Object handler
    print "route"
    if self.URI == '/login':
      return Login()
    if self.URI == '/':
      return Home()
    # no return means no routing => call on_error 404
  def on_error(self, code, msg):
    # any HTTP parsing error would call this method
    self.respond("HTTP/1.1 {code} {msg}\r\n".format(code=code, msg=http.status_name[code]) )
    self.respond( msg )

class Login:
  def set_writeback(self, writeback):
    # Set writeback
    self.respond = writeback
  def write(self, buffer):
    # HTTP body chunk will be here
    print buffer
  def close(self):
    # at the end of message, this method will be called
    self.respond("HTTP/1.1 200 OK\r\n")
    self.respond("Content-Type: text/html\r\n")
    self.respond("\r\n")
    self.respond('<input type="text" name="user">')
    self.respond('<input type="password" name="password">')

class Home:
  def set_writeback(self, writeback):
    # Set writeback
    self.respond = writeback

  # note that write method undefined
  # if some request require write method (eg. POST or PUT), ClientRequest.on_error would be called
  # and close method would never be called

  def close(self):
    # at the end of message, this method will be called
    self.respond("HTTP/1.1 200 OK\r\n")
    self.respond("Content-Type: text/html\r\n")
    self.respond("\r\n")
    self.respond("<h1>Hello!</h1>")

# open socket
import socket
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('',10000))
s.listen(1)

# start
try:
  while True:
    # accept connection
    con,addr = s.accept()
    # handle request
    request = ClientRequest()
    # set "respond" / writeback
    request.set_writeback(con.sendall)
    # read stream
    request.write_from_socket(con.fileno())
    # close connection
    con.close()
except KeyboardInterrupt:
  s.close()
```

multipart class
---------------

Steps,
  * Receive data through `write` method.
  * For each header complete, get var object through `get_var_handler(header_info)` (self defined)
    `header_info` is `tuple(key_name, filename, mime)`
  * At the beginning of data, var object's `write_init` method called IF AVAILABLE.
  * For each chucked data, transfered using var object's `write` method.
  * At the end of data, var object's `write_end` method called IF AVAILABLE.

(UNDER CONSTRUCTION)

This class little bit tricky. It must handle each part of POST request. To use this, you have to define your own POSTHandler, derived from multipart class, and how you store the data.

Here's quick class declaration,
```python
class POSTHandler(http_parser.multipart):
  data_map = {
    username = StringIO,
    password = StringIO
  }
  def get_ready_object(self, header):
    # header (dict) argument contain HTTP header (header inside body)
    # Return data object class to continue
    # Return None means invalid, so no more data is fetched
    return DataClass()
    return None
  def on_end(self):
    # Called in the end of post data
    pass
```
And `data class` example,
```python
class DataClass(object):
  def prepare(self, header):
    # header (dict) argument contain HTTP header (header inside body), just like in POSTHandler
    # called in the beginning
    # Return None or True to continue
    # otherwise is invalid, so no more data is fetched
    pass
  def write(self, buffer):
    # receive buffer
    # Remember that, this is a callback for multipart mechanism
    # Return None to continue
    # otherwise is invalid, so no more data is fetched
  def finalize(self):
    # called in the end of data stream
```

```python
post_handler = DataClass()

post = http_parser.multipart(post_handler, boundary)
post = http_parser.multipart(post_handler, boundary, length) # length from Content-Length
```

And to fetch the data can be done in two ways,
  * socket,
    ```python
    post.fetch_socket(con.fileno)
    ```
  * manually buffer,
    ```python
    post.send(buffer)
    ```
    Note: if length is given and total buffer is excessed, excessed data is ignored.

Argument that multipart class hold,
  * `finished`, `True` if request is finished, `False` otherwise.
Available method,
  * `send(buffer)`
  * `fetch_socket(sock_fd)`
  * `is_error()`, `True` if error occurred, `False` otherwise.

Success example
===============
```python
import socket
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('',10000))
s.listen(1)

con,addr = s.accept()

import http_parser

class mod(http_parser.parser):
    def routing(self):
        print self.method
        print self.URI
        print self.header
        return handler()
    def on_error(self, code, name, desc):
        print "ERROR {code} {name}".format(code=code, name=name)
        print desc
class handler:
    def write(self,buffer):
        print buffer
    def respond(self,write):
        write("HTTP/1.1 200 OK\r\n")
        write("\r\n")
        write("Halo!")

request = mod()
request.set_writeback(con.sendall)
request.write_from_socket(con.fileno())

print "finally"
con.close()

s.close()
```

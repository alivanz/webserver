HTTP C Parser
=============

For the Alica Project glory.

Run `python setup.py build` to build shared object library file for python. Stored in `build/` directory.

Handling Request
================

There are some specific handler,
  * HTTP Header Request Parser (parser class)
  * HTTP POST Multipart (multipart class)
Currently, the other method still in development.

parser class
------------

```python
request = http_parser.parser( con.fileno() )
request.fetch_data()
if request.is_error():
  # error mechanism here
```

Argument generated after `http_parser.parser.fetch_data` is called,
  * `method`, HTTP Request method
  * `URI`
  * `Query`, from URI. Remember that `URI` = `/path/something?query`
  * `header`, all request header stored here.
    eg. `Content-Disposition`, `Cookie`, `Host`
Method,
  * `fetch_data`, Fetching data from stream.
    Warning: must be called once, no checking for that.
  * `is_error`, check is request error or not. If error "Bad Request" response must be (normally) send.

multipart class
---------------

This class little bit tricky. It must handle each part of POST request. To use this, you have to define your own POSTHandler, derived from multipart class, and how you store the data.

Here's quick class declaration,
```python
class POSTHandler(http_parser.multipart):
  def get_ready_object(self, header):
    # header (dict) argument contain HTTP header (header inside body)
    # Return data object class to continue
    # Return None means invalid, so no more data is fetched
    return DataClass()
    return None
  def on_end(self):
    # Called in the end of post data. No matter what!
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
  def finalize(self):
    # called in the end of data stream
```

```python
post = http_parser.multipart(boundary)
post = http_parser.multipart(boundary, length) # length from Content-Length
```

And to fetch the data can be done in two ways,
  * socket, `post.fetch_socket(con.fileno)`
  * manually buffer, `post.send(buffer)`.
    Note: if length is given and total buffer is excessed, excessed data is ignored.

Argument that multipart class hold,
  * `finished`, `True` if request is finished, `False` otherwise.
  * `error`, `True` if error occurred, `False` otherwise.
Available method,
  * `send`
  * `fetch_socket`

HTTP C Parser
=============

For the Alica Project glory.

Run `python setup.py build` to build shared object library file for python. Stored in `build/` directory.

Handling Request
================

There are some specific handler,
  * HTTP Header Request Parser (parser class)
  * HTTP POST Multipart (multipart class)

parser class
------------

` request = http_parser.parser( con.fileno() )
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

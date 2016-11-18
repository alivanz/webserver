from distutils.core import setup, Extension
setup(name='http_request', version='1.0',  \
      ext_modules=[
        Extension('http_request', ['py_http_header_parser.c']),
        Extension('http_multipart',['py_http_multipart.c']),
        Extension('http_parser',['main.c'])
        ])

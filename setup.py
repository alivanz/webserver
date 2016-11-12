from distutils.core import setup, Extension
setup(name='http_parser', version='1.0',  \
      ext_modules=[
        Extension('http_parser', ['py_http_header_parser.c']),
        Extension('post_parser',['py_http_multipart.c'])
        #Extension('http_parser',['main.cpp'])
        ])

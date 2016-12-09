from distutils.core import setup, Extension
setup(name='http_request', version='1.0',  \
    py_modules=[
        'http'
    ],
    ext_modules=[
        #Extension('request', ['request.c']),
        #Extension('response',['response.c']),
        #Extension('multipart',['multipart.c']),
        Extension('alivanz',['alivanz.c'])
        #Extension('http_parser',['main.c'])
    ])

#!/usr/bin/python3

import os
import platform
from setuptools import setup
from setuptools.extension import Extension

source_files = [os.path.join("src", x) for x in os.listdir("src") if x.lower().endswith(".cpp")]

extra_compile_args = []
extra_link_args = []

if platform.system() == "Linux" or platform.system() == "Darwin":
    extra_compile_args = ["-std=c++17"]
elif platform.system() == "Windows":
  extra_compile_args = ["/source-charset:utf-8"]

setup(name="ccscript",
    version="1.501",
    description="ccscript",
    url="http://starmen.net/pkhack/ccscript",
    ext_modules=[
        Extension("ccscript",
                  source_files,
                  language="c++",
                  extra_compile_args=extra_compile_args,
                  extra_link_args=extra_link_args
                  )
    ])
#!/usr/bin/env python
import sys,os
import re
import logging
import argparse

logging.basicConfig(level=logging.INFO)


parser = argparse.ArgumentParser(description='Generate Python Bindings')
parser.add_argument('-I', '--include', dest='includes', action='append')
parser.add_argument('-D', '--define', dest='defines', action='append')
parser.add_argument('-o', '--output_dir', dest='output_dir', action='store')
parser.add_argument('-M', '--module_name', dest='module_name', action='store')
parser.add_argument('sources', nargs='+')
args = parser.parse_args()


from pyplusplus import module_builder
from pyplusplus import pygccxml
from pyplusplus.module_builder import call_policies
from pyplusplus import function_transformers as FT
from pygccxml import declarations
from pygccxml.declarations.matchers import access_type_matcher_t, custom_matcher_t
from pygccxml.declarations import matcher


# Creating an instance of class that will help you to expose your declarations
mb = module_builder.module_builder_t( args.sources,
                                      working_directory = os.path.abspath(os.path.curdir),
                                      include_paths = args.includes,
                                      define_symbols = args.defines,
                                      indexing_suite_version = 2,
                                     )

# expose only public interfaces
mb.variables( access_type_matcher_t( 'private' ) ).exclude()
mb.variables( access_type_matcher_t( 'protected' ) ).exclude()
mb.calldefs( access_type_matcher_t( 'private' ) ).exclude()
mb.calldefs( access_type_matcher_t( 'protected' ) ).exclude()
mb.classes( access_type_matcher_t( 'private' ) ).exclude()
mb.classes( access_type_matcher_t( 'protected' ) ).exclude()

mb.namespace('py_details').exclude() # just for template instantiation

# don't expose _XYZ() functions
py_member_filter_re = re.compile(r"(^_[^_])|(.*Cpp$)|(^impl$)")
py_member_filter = custom_matcher_t(lambda decl: py_member_filter_re.match(decl.name))
mb.global_ns.calldefs(py_member_filter, allow_empty=True).exclude()

## Py++ will generate next code: def( ..., function type( function ref ) => safe for function overloading
mb.calldefs().create_with_signature = True

## Every declaration will be exposed at its own line
mb.classes().always_expose_using_scope = True

# Creating code creator. After this step you should not modify/customize declarations.
mb.build_code_creator( module_name = args.module_name )

#license for each file
mb.code_creator.license = '//greetings earthling'

#I don't want absolute includes within code
for d in args.includes:
    mb.code_creator.user_defined_directories.append( d )
                                      
# Writing code to file.
mb.split_module( args.output_dir ) # into sub-directory mode

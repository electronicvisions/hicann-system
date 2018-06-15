#!/usr/bin/env python
import sys, os
from waflib import Options

APPNAME='HICANN-system'

from waflib.extras.symwaf2ic import SYMWAF2IC_VERSION
hicann_test_dir = '/tmp/test' # Some bogus path

def depends(ctx):
    ctx('lib-rcf', branch='v2'),
    ctx('logger')
    ctx('hicann-system', 'units/config'),
    ctx('hicann-system', 'units/arq'),
    ctx('hicann-system', 'units/jtag_lib'),
    ctx('hicann-system', 'units/stage2_conf'),
    ctx('hicann-system', 'units/stage2_hal'),
    ctx('hicann-system', 'units/hicann_test'),
    ctx('hicann-system', 'units/hostfpga'),
    ctx('hicann-system', 'units/measure'),
    ctx('hicann-system', 'global_src/systemc')
    ctx('hicann-system', 'units/communication')
    ctx('sctrltp')


# defining cmd line options
def options(opt):
    hopts = opt.add_option_group('HICANN system')
    # options for simulation
    hopts.add_option('--simulation',
            dest='simulation', action='store_true', default=False,
            help='Enable HICANN-system build flow for simulation')

    opt.load('compiler_cxx')
    opt.load('boost')


def configure(conf):
    conf.load('boost')
    conf.load('compiler_cxx')

    # save options to environment
    conf.env.simulation = Options.options.simulation

    if (conf.env.simulation):
        # THIS PART IS TO MAKE COMPILATION POSSIBLE ON *VALGOL* WITH THE *CADENCE COMPILER*

        # ugly hacking to make cadence C++ compiler visible to waf compiler_c(xx)
        os.environ['CDSROOT'] = '/cad/products/cds/ius82'
        os.environ['PATH'] += ':/cad/products/cds/ius82/tools/systemc/gcc/4.1-x86_64/bin/'

        # define systemc-relevant paths
        conf.env.INCLUDES_SYSTEMC = [
                os.getenv('CDSROOT')+'/tools/systemc/include_pch',
                os.getenv('CDSROOT')+'/tools/tbsc/include',
                os.getenv('CDSROOT')+'/tools/vic/include',
                os.getenv('CDSROOT')+'/tools/ovm/sc/src',
                os.getenv('CDSROOT')+'/tools/systemc/include/tlm'
            ]
        conf.env.LIBPATH_SYSTEMC = [
                '/cad/products/cds/ius82/tools/systemc/gcc/4.1-x86_64/install/lib64',
                '/cad/products/cds/ius82/tools/lib',
                '/cad/products/cds/ius82/tools/tbsc/lib/64bit/gnu/4.1',
                '/cad/products/cds/ius82/tools/lib/64bit/SuSE',
                '/cad/products/cds/ius82/tools/systemc/lib/64bit/gnu/4.1'
            ]
        conf.env.LIB_SYSTEMC = [
                'stdc++', 'gcc_s',
                'tbsc', 'scv',
                'systemc_sh',
                'ncscCoSim_sh',
                'ncscCoroutines_sh',
                'ncsctlm_sh'
            ] # 'ovm', 
        conf.env.RPATH_SYSTEMC = [
                '/cad/products/cds/ius82/tools/lib/64bit',
                '/cad/products/cds/ius82/tools/tbsc/lib/64bit/gnu/4.1',
                '/cad/products/cds/ius82/tools/systemc/lib/64bit'
            ]

        os.environ['BOOSTINC'] = '/usr/local/boost_1.40/include'
        os.environ['BOOSTLIB'] = '/usr/local/boost_1.40/lib'

        conf.env.LIB_XERCES = ['xerces-c']
        # ECM: This is really ugly! AH, please use a check_cxx + some machine-specific tests + inherit dependencies to your build target!
        conf.env.INCLUDES_XERCES = ['/usr/local/xerces_310/includes']
        conf.env.LIBPATH_XERCES = ['/usr/local/xerces_310/lib']

    #conf.fix_boost_paths() # project hicann-system does not know any fix_boost_paths...
    conf.check_boost(lib='system thread', mandatory=True)


def build(bld):
    if (bld.env.simulation):
        # compilation
        SYSTEMC_SOURCES = []
        for x in bld.env.SYSTEMC_SOURCES:
            SYSTEMC_SOURCES.append(bld.path.make_node(x))

        bld.shlib (
            name = 'libncsc_model.so',
            source = SYSTEMC_SOURCES,
            target = 'ncsc_model',
            cxxflags = bld.env['NCSC_CXXFLAGS'],
            use = ['systemc', 'BOOST', 'CUSTOM', 'XERCES'],
        )

    s2ctrl = bld.install_as('bin/s2ctrl.py', 'tools/s2ctrl.py')
    if s2ctrl:
        s2ctrl.chmod = 0755
    bld.install_as('lib/s2ctrl.py', 'tools/s2ctrl.py')

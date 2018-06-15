#!/usr/bin/env python
import os, sys

from optparse import OptionParser, Option, OptionGroup
parser = OptionParser()

parser.usage = "%s [--option [Argument]]" % os.path.basename(sys.argv[0])

parser.epilog = """
Python implementation on tests2 -- Vision's Stage 2 testbench.
"""

parser.add_option( '--s',      dest='starttime', metavar='<starttime>', help='starttime for testbench (used to sync chips)')
parser.add_option( '--sr',     dest='seed',      metavar='<seed>',      help='seed value for random number generation')
parser.add_option( '--js',     dest='speed',     metavar='<speed>',     help='set speed of JTAG interface in kHz (750, 1500, 300, 6000, 12000, 24000')
parser.add_option( '--c',      dest='filename',  metavar='<filename>',  help='specify xml-file to be loaded (default hicann_conf.xml)', default='hicann_conf.xml')
parser.add_option( '--log',    dest='loglevel',  metavar='<loglevel>',  help='specify integer log level - default is 2: INFO')
parser.add_option( '--l',      action='store_true',                     help='list testmodes')
parser.add_option( '--m',      dest='testmode',  metavar='<name>',      help='uses testmode')
parser.add_option( '--label',  dest='label',     metavar='<label>',     help='label your test')

comm_techniques = OptionGroup(parser, 'Comm. technique for: | FPGA  |  DNC   | HICANN')
comm_techniques.add_options( [
Option( '--bj',     action='store_true',                    help='  |  --   |  --    | JTAG  (e.g. I2C)'),
Option( '--bj2f',   nargs=2, type=int, metavar='<nh> <h>',   help='  | JTAG  | GBit   | GBit  (<nh> HICANNs, using <h>)'),
Option( '--bja',    nargs=2, type=int, metavar='<nh> <h>',   help='  | JTAG  | JTAG   | JTAG'),
Option( '--bjac',   nargs=2, type=int, metavar='<nh> <h>',   help='  | JTAG  |  --    | JTAG'),
Option( '--bt',     action='store_true',                    help='uses tcpip communication model'),
Option( '--btp',    action='store_true',                    help='uses tcpip communication model with playback memory'),
Option( '--bo',     action='store_true',                    help='use direct ocp communication model'),
])
parser.add_option_group(comm_techniques)


(options, args) = parser.parse_args()

if len(args) > 0: raise Exception("Unparsed options: ", " ".join(args))


# tests2.cpp part

from libpyhal_s2 import *

# defined in sim_def...
FPGA_COUNT=1
DNC_COUNT=1
HICANN_COUNT=1
print "TODO: DNC_COUNT, DNC_FPGA, FPGA_COUNT are defined globally via #define!"

num_jtag_hicanns = None
tar_jtag_hicann = None
jtf = None

conf = stage2_conf();
conf.readFile("hicann_cfg.xml");

if options.bj2f:
    num_jtag_hicanns = options.bjac[0]
    tar_jtag_hicann  = options.bjac[1]
    raise Exception("not implemented")
if options.bja: # with DNC
    num_jtag_hicanns = options.bjac[0]
    tar_jtag_hicann  = options.bjac[1]
    jtf = myjtag_full(True, True, num_jtag_hicanns, tar_jtag_hicann)
if options.bjac: # without DNC
    num_jtag_hicanns = options.bjac[0]
    tar_jtag_hicann  = options.bjac[1]
    jtf = myjtag_full(True, False, num_jtag_hicanns, tar_jtag_hicann)


jtag_pff = S2C_JtagPhysFpgaFull(jtf)
comm     = jtag_pff
pl       = jtag_pff
if not jtf.open(jtag_interface_t.JTAG_USB, 0):
        raise Exception("Could not connect via JTAG_USB")
#jtf.jtag_speed(750)
#if not jtf.jtag_init():
#        raise Exception("Could not jtag_init")
#jtf.jtag_bulk_init();


if comm.getState() == Stage2Comm.Commstate.openfailed:
    raise Exception("Open failed")

chips = []
for i in range(FPGA_COUNT):
    chips.append( FPGAControl(comm, i) )
for i in range(DNC_COUNT):
    chips.append( DNCControl(comm, i) )
for i in range(HICANN_COUNT):
    chips.append( HicannCtrl(comm, i) )

for c in range(HICANN_COUNT):
    for i in range(tagid_num): # ECM: global symbol! ARGL
        chip = c+FPGA_COUNT+DNC_COUNT
        print "Binding LinkLayer for tag", i, "on chip", c, "... ",
        #chips[chip].tags[i].Bind(pl,0)
        chips[chip].tags[i].Bind(pl) # ECM: no upper layer...
        print "done."
jtag = jtf

if options.testmode:
    # try to find testmode
    if not os.path.exists(options.testmode):
        raise Exception("Could not find testmode: %s" % options.testmode)
    else:
        execfile(options.testmode)

jtf.jtag_close()
jtf.close()

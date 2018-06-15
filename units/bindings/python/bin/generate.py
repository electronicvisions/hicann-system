#!/usr/bin/env python
import os
execfile(os.path.join(os.path.dirname(os.path.realpath(__file__)), 'header.py'))


# === TODO ===
# - How can we map:
#     xyz function( some_type a ** )


### GLOBAL
# just try to get everything
#mb.enumerations().include()
#mb.variables().include()

### STAGE2_CONF
# units/stage2_conf/source/stage2_conf.h
stage2_conf = mb.class_( '::stage2_conf' )
stage2_conf.include()
stage2_conf.noncopyable = True


### STAGE2_HAL
# units/stage2_hal/source/ctrlmod.h
mb.member_functions( '::facets::CtrlModule::get_data', arg_types = ['::facets::ci_addr_t &', '::facets::ci_data_t &'] ). \
    add_transformation( FT.output( 'addr' ), FT.output( 'data' ), alias = 'get_data_deprecated' )

# units/stage2_hal/source/fpga_control.h
mb.member_function( '::facets::FPGAControl::getFpgaRxData'               ).add_transformation( FT.output( 'rxdata' ) )

# units/stage2_hal/source/fg_control.h
mb.member_function( '::facets::FGControl::get_read_data'                 ).add_transformation( FT.output( 'rowaddr' ), FT.output( 'data' ) )
mb.member_function( '::facets::FGControl::get_read_data_ram'             ).add_transformation( FT.output( 'rowaddr' ), FT.output( 'data' ) )

# units/stage2_hal/source/neuron_control.h
mb.member_function( '::facets::NeuronControl::get_read_data'             ).add_transformation( FT.output( 'rowaddr' ), FT.output( 'data' ) )

# units/stage2_hal/source/neuronbuilder_control.h
mb.member_function( '::facets::NeuronBuilderControl::get_read_data'      ).add_transformation( FT.output( 'rowaddr' ), FT.output( 'data' ) )
mb.member_function( '::facets::NeuronBuilderControl::get_read_data_nmem' ).add_transformation( FT.output( 'rowaddr' ), FT.output( 'data' ) )

# units/stage2_hal/source/linklayer.h
linklayer = mb.class_ ( '::LinkLayer<facets::ci_payload, facets::ARQPacketStage2>' )
linklayer.include()
linklayer.member_function( 'GetSendData' ).exclude() # the function takes **
linklayer.member_function( 'Read', arg_types = ['::facets::ci_payload * *', None]       ).exclude() # the function takes **

# units/stage2_hal/source/s2_types.h
mb.member_function(  '::facets::IData::setPayload',  return_type = '::facets::ptype &'  ).exclude() # we exclude the set-by-ref function
mb.member_function(  '::facets::IData::setAddr',     return_type = '::uint32_t &'       ).exclude() # we exclude the set-by-ref function
mb.member_function(  '::facets::SBData::setPayload', return_type = '::facets::sbtype &' ).exclude() # we exclude the set-by-ref function

# units/stage2_hal/source/*
mb.member_functions( return_type='::facets::L1SwitchControl &'      ).call_policies = call_policies.return_internal_reference()
mb.member_functions( return_type='::facets::SynapseControl &'       ).call_policies = call_policies.return_internal_reference()
mb.member_functions( return_type='::facets::NeuronBuilderControl &' ).call_policies = call_policies.return_internal_reference()
mb.member_functions( return_type='::facets::NeuronControl &'        ).call_policies = call_policies.return_internal_reference()
mb.member_functions( return_type='::facets::FGControl &'            ).call_policies = call_policies.return_internal_reference()
mb.member_functions( return_type='::facets::SPL1Control &'          ).call_policies = call_policies.return_internal_reference()
mb.member_functions( return_type='::facets::DncIfControl &'         ).call_policies = call_policies.return_internal_reference()
mb.member_functions( return_type='::facets::Stage2Comm *'           ).call_policies = call_policies.return_internal_reference()
mb.member_functions( return_type='::facets::l2_packet_t &'          ).call_policies = call_policies.return_internal_reference()
mb.member_functions( return_type='::facets::l2_pulse &'             ).call_policies = call_policies.return_internal_reference()
mb.member_functions( return_type='::facets::ci_packet_t &'          ).call_policies = call_policies.return_internal_reference()
mb.free_functions(   return_type='::std::ostream &'                 ).call_policies = call_policies.return_internal_reference()
# fix functions returning argument references (to python style "return x,y,z")
mb.member_function( name='::facets::L1SwitchControl::get_read_cfg', arg_types=[None, '::uint32_t &'] ). \
    add_transformation( FT.output( 'rowaddr' ), FT.output( 'cfg' ), alias='get_read_cfg' )
mb.member_function( name='::facets::L1SwitchControl::get_read_cfg', arg_types=[None, '::std::vector<bool, std::allocator<bool> > &'] ). \
    add_transformation( FT.output( 'rowaddr' ), FT.output( 'cfg' ), alias='get_read_cfg_boolVector' )
mb.free_operators( '::facets::operator<<' ).exclude() # boost.python doesn't support these atm


### ARQ
# units/arq/source/arq.h
arq = mb.class_( '::ARQ< facets::ci_payload, facets::ARQPacketStage2 >' )
arq.include()
arq.member_function( 'GetSendData' ).exclude() # the function takes **
arq.member_function( 'Read'        ).exclude() # the function takes **
arq.member_functions( 'dbg' ).exclude() # strange logging stuff

# units/arq/source/packetlayer.h
packetlayer = mb.class_( '::PacketLayer< facets::ARQPacketStage2, facets::ARQPacketStage2 >' )
packetlayer.member_function( 'GetSendData' ).exclude() # the function takes **
#packetlayer.member_function( 'GetSendData' ).add_transformation( FT.output( 'p' ) )
packetlayer.member_functions( 'dbg' ).exclude() # strange logging stuff
packetlayer = mb.class_( '::PacketLayer< facets::ci_payload, facets::ARQPacketStage2 >' )
packetlayer.member_function( 'GetSendData' ).exclude() # the function takes **
#packetlayer.member_function( 'GetSendData' ).add_transformation( FT.output( 'p' ) )
packetlayer.member_function( 'Read', arg_types = ['::facets::ci_payload &',   None]        ).add_transformation( FT.output( 'p' ), alias = 'Read_by_ref' )
packetlayer.member_function( 'Read', arg_types = ['::facets::ci_payload * *', None]        ).exclude() # the function takes **
packetlayer.member_function( 'Read', arg_types = ['::facets::ci_payload &',   None, None,  None] ).add_transformation( FT.output( 'p' ), alias = 'Read_by_ref' )
packetlayer.member_function( 'Read', arg_types = ['::facets::ci_payload * *', None, None,  None] ).exclude() # the function takes **
packetlayer.member_functions( 'dbg' ).exclude() # strange logging stuff

# units/stage2_hal/source/s2comm.h
s2comm = mb.class_( 'Stage2Comm' )
s2comm.member_functions( 'Read', arg_types = ['::facets::ARQPacketStage2 &', None, None, None ]       ).add_transformation( FT.output( 'p' ), alias = 'Read' )

# units/arq/source/nullstream.h
nullostream = mb.class_( '::nullostream' )
nullostream.class_( 'nullbuf' ).exclude() # should be private?
nullostream.variable( 'm_sbuf' ).exclude() # should be private, but class is a struct

# exclude non-sense stuff (SystemC?)
mb.class_( '::Timer' ).exclude()


### JTAG
# units/jtag_lib/source/jtag_cmdbase.h
jtag_cmdbase = mb.class_( 'jtag_cmdbase' )
jtag_cmdbase.constructors().exclude() # This class contains pure virtual stuff, no construction possible.
jtag_cmdbase.member_function( 'set_get_jtag_data_chain', arg_types=['::uint64_t']+[None]*3 ).add_transformation( FT.output( 'rdata' ), alias='set_get_jtag_data_chain' )
#jtag_cmdbase.member_function( 'set_get_jtag_data_chainb'       ).add_transformation( FT.output( 'rdata' ) )
jtag_cmdbase.member_function( 'get_jtag_data_chain'            ).add_transformation( FT.output( 'rdata' ) )
#jtag_cmdbase.member_function( 'get_jtag_data_chainb'           ).add_transformation( FT.output( 'rdata' ) )
jtag_cmdbase.member_function( 'DNC_get_rx_data'                ).add_transformation( FT.output( 'rdata' ) )
jtag_cmdbase.member_function( 'DNC_read_status'                ).add_transformation( FT.output( 'rdata' ) )
jtag_cmdbase.member_function( 'DNC_read_channel_sts'           ).add_transformation( FT.output( 'status' ) )
jtag_cmdbase.member_function( 'read_id', arg_types=2*[None]    ).add_transformation( FT.output( 'rdata' ) )
jtag_cmdbase.member_function( 'HICANN_read_status'             ).add_transformation( FT.output( 'rdata' ) )
jtag_cmdbase.member_function( 'HICANN_get_rx_data'             ).add_transformation( FT.output( 'rdata' ) )
jtag_cmdbase.member_function( 'FPGA_get_status'                ).add_transformation( FT.output( 'rdata' ) )
jtag_cmdbase.member_function( 'FPGA_get_rx_channel'            ).add_transformation( FT.output( 'rdata' ) )
jtag_cmdbase.member_function( 'FPGA_get_rx_data'               ).add_transformation( FT.output( 'rdata' ) )
jtag_cmdbase.member_function( 'HICANN_set_data_delay'          ).add_transformation( FT.output( 'rdata' ) )
jtag_cmdbase.member_function( 'HICANN_arq_read_tx_timeout_num' ).add_transformation( FT.output( 'rdata0' ), FT.output( 'rdata1' ) )
jtag_cmdbase.member_function( 'HICANN_arq_read_rx_timeout_num' ).add_transformation( FT.output( 'rdata0' ), FT.output( 'rdata1' ) )
jtag_cmdbase.member_function( 'HICANN_arq_read_tx_packet_num'  ).add_transformation( FT.output( 'rdata0' ), FT.output( 'rdata1' ) )
jtag_cmdbase.member_function( 'HICANN_arq_read_rx_packet_num'  ).add_transformation( FT.output( 'rdata0' ), FT.output( 'rdata1' ) )
jtag_cmdbase.member_function( 'HICANN_arq_read_rx_drop_num'    ).add_transformation( FT.output( 'rdata0' ), FT.output( 'rdata1' ) )
jtag_cmdbase.member_function( 'HICANN_receive_ocp_packet'      ).add_transformation( FT.output( 'rdata' ) )
jtag_cmdbase.member_function( 'FPGA_i2c_getctrl'               ).add_transformation( FT.output( 'ctrlreg' ) )
jtag_cmdbase.member_function( 'FPGA_i2c_byte_read'             ).add_transformation( FT.output( 'data' ) )


# units/jtag_lib/source/jtag_cmdbase_fpgadnchicann.h
jtag_cmdbase_full = mb.class_( 'jtag_cmdbase_fpgadnchicann_full<jtag_lib::jtag_hub>' )
jtag_cmdbase_full.include()
jtag_cmdbase_full.member_function( 'set_get_jtag_data_chain', arg_types=['::uint64_t']+[None]*3 ).add_transformation( FT.output( 'rdata' ), alias='set_get_jtag_data_chain' )
jtag_cmdbase_full.member_function( 'set_get_jtag_data_chain', arg_types=['::std::vector<bool, std::allocator<bool> > const']+[None]*2 ).exclude() # exclude strange vector-based method ;)
#jtag_cmdbase_full.member_function( 'set_get_jtag_data_chainb' ).add_transformation( FT.output( 'rdata' ) )
jtag_cmdbase_full.member_function( 'get_jtag_data_chain' ).add_transformation( FT.output( 'rdata' ) )
#jtag_cmdbase_full.member_function( 'get_jtag_data_chainb' ).add_transformation( FT.output( 'rdata' ) )
#jtag_cmdbase_full.member_function( 'read_id' ).add_transformation( FT.output( 'rdata' ) )

# units/jtag_lib/source/jtag_lib.h
jtag_lib_ns = mb.namespace( '::jtag_lib' ) # jtag_lib header file
jtag_lib_ns.include()
#jtag_lib_ns.enumeration( 'jtag_transfer_type' ).exclude() # W1009
jtag_hub = jtag_lib_ns.class_( 'jtag_hub' )
jtag_hub.variable( 'jtag_obj' ).exclude() # should be private, but class is a struct ;)
jtag_hub.mem_funs( 'set_logfile' ).exclude()
jtag_hub.mem_fun( 'logvprintf' ).exclude()
jtag_hub.member_function( 'jtag_read', arg_types=['char &',               None] ).add_transformation( FT.output( 'val' ), alias = 'jtag_read_char'           )
jtag_hub.member_function( 'jtag_read', arg_types=['unsigned char &',      None] ).add_transformation( FT.output( 'val' ), alias = 'jtag_read_unsigned_char'  )
jtag_hub.member_function( 'jtag_read', arg_types=['short int &',          None] ).add_transformation( FT.output( 'val' ), alias = 'jtag_read_short'          )
jtag_hub.member_function( 'jtag_read', arg_types=['short unsigned int &', None] ).add_transformation( FT.output( 'val' ), alias = 'jtag_read_unsigned_short' )
jtag_hub.member_function( 'jtag_read', arg_types=['int &',                None] ).add_transformation( FT.output( 'val' ), alias = 'jtag_read_int'            )
jtag_hub.member_function( 'jtag_read', arg_types=['unsigned int &',       None] ).add_transformation( FT.output( 'val' ), alias = 'jtag_read_unsigned_int'   )

execfile(os.path.join(os.path.dirname(os.path.realpath(__file__)), 'footer.py'))

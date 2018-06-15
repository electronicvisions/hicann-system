project_specify_options -format verilog -severity warning -version 01
project_specify_options -format vhdl -severity warning -version 93
#project_specify_files -format vhdl -work LEDA_WORK -file_extension {.vhd .vhdl} {/home1/eisenrei/ICPRO/ads/tutorial/units/state_machine/source/rtl/vhdl/state_machine.vhd}
project_specify_libraries -format vhdl -resource {{IEEE {$LEDA_PATH/resources/resource_93/IEEE}} {STD {$LEDA_PATH/resources/resource_93/STD}} {SYNOPSYS {$LEDA_PATH/resources/resource_93/SYNOPSYS}} }
project_build
#gui_start
#checker_set_design_constraints -top { LEDA_WORK VHDL_RTL/STATE_MACHINE }
checker_run
exit

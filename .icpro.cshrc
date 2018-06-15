# $ICPRO_DIR/.icpro.cshrc
# (schlue) 2007/09/08
#
# This file will be sourced from every subshell (tcsh) spawned
# within a ICPROJECT.
# - specific project alias definitions
# - NOT for loading of modules or environment settings
#   (put them into $ICPRO_DIR/.icpro.login)
#
# Project:    p_facets
# Subproject: s_system
#
#####################################################
# Example:
#alias lsv 'ls *.v*'

#####################################################
# load user specific environment
if ( -e ./env/user/$USER/.icpro_user.cshrc ) then
  source ./env/user/$USER/.icpro_user.cshrc
endif

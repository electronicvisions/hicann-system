# $ICPRO_DIR/.icpro.login
# (schlue) 2005/09/14
#
# This file will be sourced from tcsh only once when first
# going into the ICPROJECT.
# - specific project module loads
# - NOT for alias settings (put them into $ICPRO_DIR/.icpro.cshrc)
#
# Project:    p_facets
# Subproject: s_system
#
#####################################################
if (-e .modules.$ICPRO_GROUP) then
  source .modules.$ICPRO_GROUP
endif

#####################################################
if ( -e ./env/user/$USER/.icpro_user.login ) then
  source ./env/user/$USER/.icpro_user.login
endif




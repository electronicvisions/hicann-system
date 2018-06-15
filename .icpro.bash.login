# $ICPRO_DIR/.icpro.bash.login
# (md) 2006/07/07
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
if [ -f .modules.$ICPRO_GROUP ]; then
  source .modules.$ICPRO_GROUP
fi

#####################################################
if [ -f ./env/user/$USER/.icpro_user.bash.login ]; then
  source ./env/user/$USER/.icpro_user.bash.login
fi





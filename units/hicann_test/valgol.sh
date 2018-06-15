module load gcc/4.4.3
CPLUS_INCLUDE_PATH=/usr/local/xerces_310/include LIBRARY_PATH=/usr/local/xerces_310/lib python2.6 waf configure --boost-include=/usr/local/boost_1.40/include/ --boost-libs=/usr/local/boost_1.40/lib/
python2.6 waf build

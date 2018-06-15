/*
 * Class for loading Hicann configuration from XML-file 
 *
 * @author: Sebstian Millner, KIP
 * @date: 23.03.2010
 * @email: sebastian.millner@kip.uni-heidelberg.de
 *
 */
#ifndef _STAGE2CONF_H
#define _STAGE2CONF_H

#include "common.h"
#ifndef PYPLUSPLUS
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#endif

#include <boost/scoped_ptr.hpp>

#include "logger.h"

#ifndef PYPLUSPLUS
using namespace xercesc;
#endif


class stage2_conf 
{
	private:
#ifndef PYPLUSPLUS
		Logger& log;
		boost::scoped_ptr<XercesDOMParser> parser;
		boost::scoped_ptr<ErrorHandler> errHandler;

		// getted from xerces
		DOMDocument* document;
		DOMElement* currentHead;

		DOMElement* getModuleByID(DOMElement* root, const XMLCh* tagName, int id);
		int getValueByTagName(DOMElement* root, const char* tagName);
		bool tagExists(DOMElement* root, const char* tagName);
		const char* filenamebuffer;
#endif

	public:
		std::string ClassName();
		stage2_conf();
		~stage2_conf();
		int getHicann(int id); //Try to set currentHead to Hicann id - only possible if we are on a reticle
		int readFile(const char* filename);
		int reLoad();
		int writeFile(const char* filename); // writes internal DOM document to file
		int updateFG(const int fg_no, const int line_no, const int value_no, const int value);
		std::vector<int> getFgLine(fg_loc loc, uint lineNumber);//only possible if we are on a Hicann
		//get configuration for an IBoardV2
		bool getIBoardV2(int boardid, std::vector<double>* doub_values, std::vector<uint>* uint_values);
};
#endif //_STAGE2CONF_H


/*
 * Class for loading Hicann configuration from XML-file 
 *
 * @author: Sebstian Millner, KIP
 * @date: 23.03.2010
 * @email: sebastian.millner@kip.uni-heidelberg.de
 *
 */


#include "stage2_conf.h"
#include <cstring>
#ifndef COMMON
#include "common.h"
#define COMMON
#endif
#include "logger.h"

#include <cassert>
#include <cstdio>

#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/dom/DOM.hpp>

#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>


using namespace std;
using namespace xercesc;


stage2_conf::stage2_conf() :
	log(Logger::instance("hicann-system.stage2_conf", Logger::INFO, ""))
{
	XMLPlatformUtils::Initialize();
	parser.reset(new XercesDOMParser);

	parser->setValidationScheme(XercesDOMParser::Val_Always);
	parser->setDoNamespaces(true);    // optional

	errHandler.reset(new HandlerBase);
	parser->setErrorHandler(errHandler.get());
}

stage2_conf::~stage2_conf() {
	XMLPlatformUtils::Terminate();
}

string stage2_conf::ClassName(){return "stage2_conf";}

int stage2_conf::reLoad(){

	log(Logger::INFO)<<"Stage2_conf: Reloading xml file";
	return readFile(filenamebuffer);
}

int stage2_conf::readFile(const char* filename){
	filenamebuffer = filename;
	try {
	    parser->parse(filename);
	}
	catch (const XMLException& toCatch) {
	    char* message = XMLString::transcode(toCatch.getMessage());
	    log(Logger::ERROR) << "Stage2_conf: Exception message is: \n"
		 << message << "\n";
	    XMLString::release(&message);
	    return -1;
	}
	catch (const DOMException& toCatch) {
	    char* message = XMLString::transcode(toCatch.msg);
	    log(Logger::ERROR) << "Stage2_conf: Exception message is: \n"
		 << message << "\n";
	    XMLString::release(&message);
	    return -1;
	}
	catch (...) {
	    log(Logger::ERROR) << "Stage2_conf: Unexpected Exception(mostly file not found. Default file name is \"hicann_cfg.xml\") " ;
	    return -1;
	}
	log(Logger::INFO)<<"Stage2_conf: loaded xml file: "<<filename;
	document = parser->getDocument();
	log(Logger::INFO) <<"Stage2_conf: Document type is: " <<XMLString::transcode(document->getDoctype()->getName());
	log(Logger::INFO) <<"Stage2_conf: Root node is: " <<XMLString::transcode(document->getDocumentElement()->getTagName());


/*following code is sample code -- can bee deleted any time
	DOMNodeList* books = document->getDocumentElement()->getElementsByTagName(XMLString::transcode("book"));
	DOMElement* book;

	for(uint i=0; i<books->getLength(); i++){
		book = (DOMElement*)(books->item(i));
		log(Logger::INFO) <<"Found book: " <<XMLString::transcode(book->getAttribute(XMLString::transcode("id")));
		log(Logger::INFO) <<"\t Author:  " <<XMLString::transcode(book->getElementsByTagName(XMLString::transcode("author"))->item(0)->getTextContent());
		log(Logger::INFO) <<"Found book:\t " <<XMLString::transcode(book->getAttribute(XMLString::transcode("id")));
	
	}*/

 	currentHead=(DOMElement*)document->getDocumentElement(); // TODO: dynamic_cast?
	return 1;
}


// TODO: Wafer id, Hicann id, ... are missing 
int stage2_conf::updateFG(const int fgId, const int lineId, const int valueId, const int value) {
	// some definitions for common xml strings
	const XMLCh* xmlStrFg    = XMLString::transcode(   "fg");
	const XMLCh* xmlStrLine  = XMLString::transcode( "line");
	const XMLCh* xmlStrValue = XMLString::transcode("value");
	const XMLCh* xmlStrId    = XMLString::transcode(   "id");
	const XMLCh* xmlStrAll   = XMLString::transcode(  "all");

	char strValueId[20]; // should be enough
	char strValue  [20];
	snprintf(strValueId, 20, "%d", valueId);
	snprintf(strValue,   20, "%d", value);

	// get fg block by fg number
	DOMElement* fgElem    = getModuleByID(document->getDocumentElement(), xmlStrFg, fgId);
	// get line(#) in this fg
	DOMElement* lineElem  = getModuleByID(fgElem, xmlStrLine, lineId);
	// get value(#) in this line
	DOMElement* valueElem = getModuleByID(lineElem, xmlStrValue, valueId);

	// all your base are belong to us!
	assert(fgElem    != NULL);
	assert(lineElem  != NULL);
	assert(valueElem != NULL);

	if ( XMLString::equals( xmlStrAll, valueElem->getAttribute( xmlStrId ))) {
		// Found element! <fg id="fgId"><line id="lineId"><value id="valueId">HERE I AM
		log(Logger::DEBUG2) << ClassName() << "::updateFG() found default valueElem => adding new node..." << std::endl;
		// create new element
		DOMElement* newValueElem = document->createElement( xmlStrValue );

		// set attribute id to valueId
		XMLCh* xmlStrValueId = XMLString::transcode( strValueId );
		newValueElem->setAttribute( xmlStrId, xmlStrValueId );
		XMLString::release(&xmlStrValueId);

		// set content to value
		newValueElem->setTextContent( XMLString::transcode( strValue ));

		// append element to line at lineId
		lineElem->appendChild( newValueElem );
	} else {
		char* oldValue = XMLString::transcode( valueElem->getTextContent() );
		log(Logger::DEBUG2) << ClassName() << "::updateFG() overwriting old oldValue (" << oldValue;
		XMLString::release( &oldValue );

		XMLCh* newValue = XMLString::transcode( strValue );
		valueElem->setTextContent( newValue );
		// ECM (2020-05-19) Not sure what to print here, XMLCh is uint16_t; let's just print the lower 8 bits
		log(Logger::DEBUG2) << "), setting " << reinterpret_cast<char*>(newValue) << std::endl;
		XMLString::release( &newValue );
	}


	return 0; // TODO: what is this ;)
}


int stage2_conf::writeFile(const char *strPath) {
	XMLCh tempStr[100];
	XMLString::transcode("LS", tempStr, 99);
	DOMImplementation *impl = DOMImplementationRegistry::getDOMImplementation(tempStr);
	DOMLSSerializer* theSerializer = static_cast<DOMImplementationLS*>(impl)->createLSSerializer();

	// you can set some features on this serializer
	if (theSerializer->getDomConfig()->canSetParameter( XMLUni::fgDOMWRTDiscardDefaultContent, true ))
		theSerializer->getDomConfig()->setParameter(    XMLUni::fgDOMWRTDiscardDefaultContent, true );

	if (theSerializer->getDomConfig()->canSetParameter( XMLUni::fgDOMWRTFormatPrettyPrint, true ))
		theSerializer->getDomConfig()->setParameter(    XMLUni::fgDOMWRTFormatPrettyPrint, true );

	// LocalFileFormatTarget prints the resultant XML stream to a file
	// (XMLCh* filename) once it receives any thing from the serializer.
	XMLString::transcode(strPath,tempStr, 99);
	boost::scoped_ptr<XMLFormatTarget> myFormTarget(new LocalFileFormatTarget( tempStr ));
	DOMLSOutput* theOutput = static_cast<DOMImplementationLS*>(impl)->createLSOutput();
	theOutput->setByteStream( myFormTarget.get() );

	try {
		// do the serialization through DOMLSSerializer::write();
		theSerializer->write( document, theOutput );
	}
	catch (const XMLException& toCatch) {
		char* message = XMLString::transcode( toCatch.getMessage() );
		log(Logger::WARNING) << "Exception message is: \n" << message << std::endl;
		XMLString::release(&message);
		return -1;
	}
	catch (const DOMException& toCatch) {
		char* message = XMLString::transcode( toCatch.msg );
		log(Logger::WARNING) << "Exception message is: \n" << message << std::endl;
		XMLString::release(&message);
		return -1;
	}
	catch (...) {
		log(Logger::WARNING) << "Unexpected Exception" << std::endl;
		return -1;
	}

	theOutput->release();
	theSerializer->release();
	return 0;
}


DOMElement* stage2_conf::getModuleByID(DOMElement* root, const XMLCh* tagName, int id){
	DOMNodeList* Nlist = root->getElementsByTagName(tagName);
	if(Nlist->getLength()==0){
		log(Logger::ERROR)<<"Stage2_conf: No "<<XMLString::transcode(tagName)<<" found";	
		return NULL;
	} else {
		log(Logger::DEBUG2)<<"Stage2_conf: Found "<<Nlist->getLength()<<" "<<XMLString::transcode(tagName);
	}
	DOMElement* all=NULL;
	DOMElement* element=NULL;
	for(uint i=0; i<Nlist->getLength(); i++){
		element=(DOMElement*)Nlist->item(i); // TODO: dynamic_cast?

		if (XMLString::equals(element->getAttribute(XMLString::transcode("id")),XMLString::transcode("all"))) { 
			all = element;
		} else if(XMLString::parseInt(element->getAttribute(XMLString::transcode("id")))==id) {
			log(Logger::DEBUG2)<<"Stage2_conf: Found "<<XMLString::transcode(tagName)<<" "<<id<<".";
			return element;
		} 
	}
	if (all) {
		log(Logger::DEBUG2)<<"Stage2_conf: Found default "<<XMLString::transcode(tagName)<<".";
		return all;
	} else {
		log(Logger::ERROR)<<"Stage2_conf: No fitting "<<XMLString::transcode(tagName)<<"found";
		return NULL;
	}
	
}


vector<int> stage2_conf::getFgLine(fg_loc loc, uint lineNumber){
	DOMElement* fg = getModuleByID(currentHead, XMLString::transcode("fg"),(int)loc); 
	log(Logger::DEBUG2)<<"Stage2_conf: getFgLine";
	DOMElement* line = getModuleByID(fg, XMLString::transcode("line"),lineNumber); 
	DOMTreeWalker* walker = document->createTreeWalker(line,DOMNodeFilter::SHOW_ELEMENT,NULL,true); 
	DOMElement* currentNode = (DOMElement*)walker->firstChild(); // TODO: dynamic_cast?
	int all=0; //default retrun value is 
	if(currentNode && XMLString::equals(currentNode->getAttribute(XMLString::transcode("id")),XMLString::transcode("all"))) {
		all=XMLString::parseInt(currentNode->getTextContent());
		log(Logger::DEBUG2)<<"Stage2_conf: Found defaul node for FG"<<loc<<" line number "<<lineNumber<<".";
		currentNode = (DOMElement*)walker->nextNode(); // TODO: dynamic_cast?
	}
	vector<int> result (FG_pkg::fg_lines +1, all); //set return value to default from xml; fg_lines is incremented as we need an even number for programming - last value can be written to hHicann but is not used for anything
	
	for (int i = 0; i<(int)FG_pkg::fg_lines + 1; i++){
		if((currentNode && XMLString::parseInt(currentNode->getAttribute(XMLString::transcode("id")))==i)){//found value for current cell;
			result[i] = XMLString::parseInt(currentNode->getTextContent());
			currentNode=(DOMElement*)walker->nextNode();	 // TODO: dynamic_cast?
		}
		log(Logger::DEBUG3)<<"Value of element "<<i<<" of line "<<lineNumber<<" is: "<<result[i]<<".";
	}
	return result;

} 

// configure the iBoard (resistor values, voltages, etc)
bool stage2_conf::getIBoardV2(int boardid, std::vector<double>* doub_values, std::vector<uint>* uint_values){
	log(Logger::DEBUG2)<<"Stage2_conf: getIBoardV2";
	// find elements with tag name "iboardv2"
	DOMNodeList* boards = currentHead->getElementsByTagName(XMLString::transcode("iboardv2"));
	const XMLSize_t boardsCount = boards->getLength();
	for (XMLSize_t a = 0; a < boardsCount; ++a){
		//for all available board configurations look for one with matching ID
		DOMNode* boardsNode = boards->item(a); 
		DOMElement* board = dynamic_cast<xercesc::DOMElement*>(boardsNode);
		if (XMLString::parseInt(board->getAttribute(XMLString::transcode("id")))==boardid){
			// get all children nodes of board with matching ID
			DOMNodeList* children = board->getChildNodes();
			const  XMLSize_t nodeCount = children->getLength();
			for( XMLSize_t i = 0; i < nodeCount; ++i ){
				DOMNode* currentNode = children->item(i);
				// check it the node is an element
				if( currentNode->getNodeType() &&  currentNode->getNodeType() == DOMNode::ELEMENT_NODE ){
				DOMElement* currentElement = dynamic_cast<xercesc::DOMElement*>(currentNode);
					if(XMLString::equals(currentElement->getTagName(), XMLString::transcode("dac"))){
						// Now read attributes of element "dac". Attention, all elements must be in correct order
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vrefdac"))),NULL));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("dacres"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("dac0addr"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("dac1addr"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("dacvol"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("dacvoh"))),NULL,0)); 
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("dacvddbus"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("dacvdd25"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("dacvdd5"))),NULL,0));  
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("dacvdd11"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("dacvref_dac"))),NULL,0));	
					}
					if(XMLString::equals(currentElement->getTagName(), XMLString::transcode("adc"))){
						// Now read attributes of element "adc". Attention, all elements must be in correct order
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vrefadc"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vref_ina"))),NULL));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcres"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adc0addr"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adc1addr"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("readiter"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcivdda"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcivdd_dncif"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcivdda_dncif"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcivdd33"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcivol"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcivoh"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcivdd"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcivddbus"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcivdd25"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcivdd5"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcivdd11"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcaout0"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcaout1"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcadc0"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcadc1"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("adcadc2"))),NULL,0));
					}
					if(XMLString::equals(currentElement->getTagName(), XMLString::transcode("mux"))){
						// Now read attributes of element "adc". Attention, all elements must be in correct order
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("mux0addr"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("mux1addr"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("mux0cmd"))),NULL,0));
						uint_values->push_back(std::strtoul(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("mux1cmd"))),NULL,0));
					}
					if(XMLString::equals(currentElement->getTagName(), XMLString::transcode("resistors"))){
						// Now read attributes of element "resistors". Attention, all elements must be in correct order
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vdd11_r1"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vdd11_r2"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vdd5_r1"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vdd5_r2"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vddbus_r1"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vddbus_r2"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vdd25_r1"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vdd25_r2"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vol_r1"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vol_r2"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("voh_r1"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("voh_r2"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vref_r1"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vref_r2"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rg_vdd"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rg_vdddnc"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rg_vdda0"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rg_vddadnc"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rg_vdd33"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rg_vol"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rg_voh"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rg_vddbus"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rg_vdd25"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rg_vdd5"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rg_vdd11"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rm_vdd"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rm_vdddnc"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rm_vdda"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rm_vddadnc"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rm_vdd33"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rm_vol"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rm_voh"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rm_vddbus"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rm_vdd25"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rm_vdd5"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("rm_vdd11"))),NULL));
					}
					if(XMLString::equals(currentElement->getTagName(), XMLString::transcode("voltages"))){
						// Now read attributes of element "resistors". Attention, all elements must be in correct order
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vdd5value"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vdd25value"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vdd11value"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vohvalue"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("volvalue"))),NULL));
						doub_values->push_back(std::strtod(XMLString::transcode(currentElement->getAttribute(XMLString::transcode("vddbusvalue"))),NULL));
					}
				}
			}
			return true;
		}
	}
	log(Logger::ERROR)<<"No matching ID for an Iboard found";
	return false;
}
                                                                                                                                                                    
int stage2_conf::getValueByTagName(DOMElement* root, const char* tagName){                                                                                          
	//Check if element exists befor using this funcion                                                                                                              
	DOMElement* element=(DOMElement*)(root->getElementsByTagName(XMLString::transcode(tagName))->item(0)); // TODO: dynamic_cast?
	return XMLString::parseInt(element->getTextContent());
}

bool stage2_conf::tagExists(DOMElement* root, const char* tagName){
	return 0<root->getElementsByTagName(XMLString::transcode(tagName))->getLength();
}

int stage2_conf::getHicann(int id){
	if(!currentHead || XMLString::compareString(currentHead->getTagName(), XMLString::transcode("reticle"))!=0){
		log(Logger::ERROR)<<"Stage2_conf: CurrentHead is no reticle";
		return 0;
	}
	DOMElement* result = getModuleByID(currentHead, XMLString::transcode("hicann"),id);
	if (result){
		currentHead=result;
		return 1;
	} 
	return 0;
}


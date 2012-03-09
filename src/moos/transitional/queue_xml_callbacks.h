
// xml code based initially on work in C++ Cookbook by D. Ryan Stephens, Christopher Diggins, Jonathan Turkanis, and Jeff Cogswell. Copyright 2006 O'Reilly Media, INc., 0-596-00761-2

#ifndef QUEUE_XML_CALLBACKS20091211H
#define QUEUE_XML_CALLBACKS20091211H

#include <stdexcept>
#include <vector>
#include <sstream>
#include <iostream>

#include <boost/algorithm/string.hpp>

#include <xercesc/sax2/Attributes.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>

#include "goby/util/as.h"
#include "goby/common/exception.h"
#include "goby/moos/protobuf/transitional.pb.h"
#include "goby/moos/transitional/xml/tags.h"

namespace goby
{
    namespace transitional
    {
    
// Implements callbacks that receive character data and
// notifications about the beginnings and ends of elements 
        class QueueContentHandler : public xercesc::DefaultHandler {
          public:
          QueueContentHandler(std::vector<goby::transitional::protobuf::QueueConfig>& q)
              : q_(q)
            { xml::initialize_tags(tags_map_); }
        
            void startElement( 
                const XMLCh *const uri,        // namespace URI
                const XMLCh *const localname,  // tagname w/ out NS prefix
                const XMLCh *const qname,      // tagname + NS pefix
                const xercesc::Attributes &attrs );      // elements's attributes

            void endElement(          
                const XMLCh *const uri,        // namespace URI
                const XMLCh *const localname,  // tagname w/ out NS prefix
                const XMLCh *const qname );     // tagname + NS pefix

#if XERCES_VERSION_MAJOR < 3
            void characters(const XMLCh* const chars, const unsigned int length)
	      { current_text.append(toNative(chars), 0, length); }
#else
            void characters(const XMLCh* const chars, const XMLSize_t length )
	      { current_text.append(toNative(chars), 0, length); }
#endif
    
          private:
            bool in_message_var()
            { return xml::in_message_var(parents_); }
            bool in_header_var()
            { return xml::in_header_var(parents_); }
            bool in_publish()
            { return xml::in_publish(parents_); }
 
          private:
            std::vector<goby::transitional::protobuf::QueueConfig>& q_;
            std::string current_text;
        
            std::set<xml::Tag> parents_;
            std::map<std::string, xml::Tag> tags_map_;
        };

// Receives Error notifications.
        class QueueErrorHandler : public xercesc::DefaultHandler 
        {

          public:
            void warning(const xercesc::SAXParseException& e)
            {
                std::cout << "warning:" << toNative(e.getMessage());
            }
            void error(const xercesc::SAXParseException& e)
            {
                XMLSSize_t line = e.getLineNumber();
                std::stringstream ss;
                ss << "xml parsing error on line " << line << ": " << std::endl << toNative(e.getMessage());
                throw goby::Exception(ss.str());
            }
            void fatalError(const xercesc::SAXParseException& e) 
            {
                error(e);
            }
        };
    }
}

#endif

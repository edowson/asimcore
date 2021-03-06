/**************************************************************************
 * Copyright (c) 2014, Intel Corporation
 *
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are 
 * met:
 * 
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright 
 *   notice, this list of conditions and the following disclaimer in the 
 *   documentation and/or other materials provided with the distribution.
 * - Neither the name of the Intel Corporation nor the names of its 
 *   contributors may be used to endorse or promote products derived from 
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * @author Artur Klauser
 * @brief Implementation of XML Output Object (w/ internal XML writer)
 * @see @ref XMLOut_Documentation "XML Output Documentation"
 * @ingroup XMLOut
 */

// ASIM core
#include "asim/mesg.h"
#include "asim/xmlout.h"

/**
 * Creates a new XML Output object associated with an output file.
 * Debugging turns on synchronous output on all operations, which
 * helps determine the output state of the XMLOut object in case the
 * application crashes before the XMLOut object would normally be
 * written to disk.
 */
XMLOut::XMLOut (
    const char* filename,    ///< output file name
    const char* root,        ///< name of XML root element
    const char* dtdPublicId, ///< PUBLIC-ID of XML <!DOCTYPE
    const char* dtdSystemId, ///< SYSTEM-ID of XML <!DOCTYPE
    const bool debug)        ///< if true, turn on synchronous output, ie.
                             ///< the XML tree is written out anew after
                             ///< every operation
: writer(filename)
{
    this->filename = filename;
    this->debug = debug;

    // write out (extended) XML header
    char* creatorComment = "Created by ASIM XMLOut";
    writer.CreateHeader(root, dtdPublicId, dtdSystemId, creatorComment);
}

/**
 * Dump the XML Object to its output file and destroy the object.
 */
XMLOut::~XMLOut ()
{
    // generate output
    writer.Finish();
}

/**
 * Add an element node to the XML tree and make it the current node
 */
void
XMLOut::AddElement (
    const char* name)  ///< tag name of the new element
{
    writer.AddElement (name);
}

/**
 * Close the current node and make its parent the new current node
 */
void
XMLOut::CloseElement(void)
{
    writer.CloseElement();
}

/**
 * Add an attribute node to the current node
 */
void
XMLOut::AddAttribute (
    const char* name,   ///< attribute name to add
    const char* value)  ///< attribute value to add
{
    writer.AddAttribute(name, value);
}

/**
 * Add a text node to the current node
 */
void
XMLOut::AddText (
    const char* text)  ///< text to add
{
    writer.AddText(text);
}

/**
 * Add a CDATA node to the current node
 */
void
XMLOut::AddCDATA (
    const char* text)  ///< text to add
{
    writer.AddCDATA(text);
}

/**
 * Add a comment node to the current node
 */
void
XMLOut::AddComment (
    const char* text)  ///< text to add
{
    writer.AddComment(text);
}

/**
 * Add a processing instruction. Processing instructions are always
 * added just before the root node, and do not affect the position of
 * the current node.
 */
void
XMLOut::AddProcessingInstruction (
    const char* target,  ///< target part of processing instruction to add
                         ///< (ie. the first string after opening <? )
    const char* data)    ///< data part of processing instruction
                         ///< (ie. everything between target and closing ?> )
{
    writer.AddProcessingInstruction(target, data);
}

//----------------------------------------------------------------------------
// XML Writer (lowest level - replaces external XML libraries)
//----------------------------------------------------------------------------

/**
 * Instantiate a writer and open its output file
 */
XMLOut::Writer::Writer (
    const char* filename)    ///< output file name
//: out(filename)
{

    unsigned int string_length = strlen(filename);

    if(filename[string_length-1] == 'z' &&
       filename[string_length-2] == 'g' &&
       filename[string_length-3] == '.') 
    {
        char temp_string[1024];
        if(string_length > 1000) { ASSERTX(false); }
        sprintf(temp_string,"gzip >%s", filename);
        out = popen(temp_string, "w");
    } else {
        out = fopen(filename, "w");
    }
    
    // check if output file opened OK
    if ( ! out) {
        ASIMERROR("XMLOut: could not open output file " << filename << endl);
    }
}

/**
 * Finish up writing, close the output file, and clean up
 */
XMLOut::Writer::~Writer ()
{
    Finish();
    pclose(out);
}

/**
 * Check if the writing of partial tags was delayed and write it now
 */
inline void
XMLOut::Writer::DelayedWrite (void)
{
    if ( ! delayedWrite.empty()) {
        //out << delayedWrite;
        fprintf(out, delayedWrite.c_str());
        delayedWrite.clear();
    }
}

/**
 * Check if the writing of the root element tag was delayed and add
 * the root element now.
 */
inline void
XMLOut::Writer::DelayedRootTag (void)
{
    if ( ! rootTag.empty()) {
        string tmp = rootTag;
        // clear root tag before recursing
        rootTag.clear();
        AddElement (tmp.c_str());
    }
}

/**
 * Create the XML file header
 */
void
XMLOut::Writer::CreateHeader (
    const char* root,           ///< name of XML root element
    const char* dtdPublicId,    ///< PUBLIC-ID of XML <!DOCTYPE
    const char* dtdSystemId,    ///< SYSTEM-ID of XML <!DOCTYPE
    const char* creatorComment) ///< comment indicating who created output
{
    //out << "<?xml version=\"1.0\"?>";
    fprintf(out, "<?xml version=\"1.0\"?>");
    AddComment(creatorComment);
    //out << endl
    //    << "<!DOCTYPE " << root
    //    << " PUBLIC \"" << dtdPublicId << "\""
    //    << " \"" << dtdSystemId << "\">";
    fprintf(out, "\n");
    fprintf(out, "<!DOCTYPE %s", root);
    fprintf(out, " PUBLIC \"%s\"", dtdPublicId);
    fprintf(out, " \"%s\">", dtdSystemId);


    // delay writing out of root element for a bit
    rootTag = root;
}

/**
 * Finish up writing. Closes all pending tags.
 */
void
XMLOut::Writer::Finish (void)
{
    while ( ! activeTags.empty()) {
        CloseElement();
    }
}

/**
 * Add a new element. Write out initial portion of start tag (end
 * portion remains pending for addition of attributes). Also puts the
 * element on the tag stack to close tags later in the right order.
 */
void
XMLOut::Writer::AddElement (
    const char* name)           ///< name of XML tag
{
    DelayedRootTag();
    DelayedWrite();

    // write beginning of tag
    //out << endl << "<" << name;
    fprintf(out, "\n<%s", name);
    // remember delayed part
    delayedWrite = ">";

    // remember tag on stack
    activeTags.push_back(name);
}

/**
 * Close the top-most element on the tag stack (and pop it).
 */
void
XMLOut::Writer::CloseElement (void)
{
    DelayedWrite();

    // get most recent tag from stack
    string & name = activeTags.back();

    // write closing tag
    //out << "</" << name << ">";
    fprintf(out, "</%s>", name.c_str());

    // pop the tag
    activeTags.pop_back();
}

/**
 * Add an attribute to the top-most element. This is only valid right
 * after opening the element (and other AddAttribute calls). If
 * anything else has been written in the meantime, it will create
 * non-well-formed XML - ie. badly broken output! - because the start
 * tag will already have been closed.
 */
void
XMLOut::Writer::AddAttribute (
    const char* name,   ///< attribute name to add
    const char* value)  ///< attribute value to add
{
    // awefull hack to correctly associate attributes with root element
    DelayedRootTag();

    //out << " " << name << "=\"" << value << "\"";
    fprintf(out, " %s=\"%s\"",name,value);
}

/**
 * Add plain text to the output.
 * Text needs to be scanned for characters illegal in XML which need to
 * be excaped by their appropriate escaped sequences. This is only done
 * for some characters here (for speed reasons).
 */
void
XMLOut::Writer::AddText (
    const char* text)  ///< text to add
{
    DelayedWrite();

    string fixedup(text);
    string::size_type idx = 0;
    while ((idx = fixedup.find_first_of("&<>", idx)) != string::npos)
    {
        string repl;
        switch (fixedup[idx])
        {
          case '&': repl = "&amp;";
                    break;
          case '<': repl = "&lt;";
                    break;
          case '>': repl = "&gt;";
                    break;
          default: ASSERTX(false);
        }
        fixedup.replace(idx, 1, repl);
        idx += repl.size();
    }

    //out << fixedup;
    fprintf(out, "%s", fixedup.c_str());
}

/**
 * Add a CDATA section to the output.
 */
void
XMLOut::Writer::AddCDATA (
    const char* text)  ///< cdata to add
{
    DelayedWrite();

    //out << endl << "<![CDATA[" << text << "]]>";
    fprintf(out , "\n<![CDATA[%s]]", text);
}

/**
 * Add a comment to the output.
 */
void
XMLOut::Writer::AddComment (
    const char* text)  ///< comment to add
{
    DelayedWrite();

    //out << endl << "<!--" << text << "-->";
    fprintf(out , "\n<!--%s-->", text);
}

/**
 * Add a processing instruction to the output.
 */
void
XMLOut::Writer::AddProcessingInstruction (
    const char* target, ///< target (first) part of PI to add
    const char* data)   ///< data (second) part of PI to add
{
    DelayedWrite();

    //out << endl << "<?" << target << " " << data << "?>";
    fprintf(out , "\n<?%s %s?>", target, data);
}



/**
 * @file thconfig.cxx
 * Configuration module.
 */
  
/* Copyright (C) 2000 Stacho Mudrak
 * 
 * $Date: $
 * $RCSfile: $
 * $Revision: $
 *
 * -------------------------------------------------------------------- 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * --------------------------------------------------------------------
 */

#include "thconfig.h"
#include "therion.h"
#include "thparse.h"
#include <stdlib.h>
#include <stdio.h>
#include "thchenc.h"
#include "thexception.h"
#include "thdatabase.h"
#include "thdatareader.h"
#include "thdataobject.h"


enum {TT_UNKNOWN_CFG, TT_SOURCE, TT_SELECT, TT_UNSELECT, TT_EXPORT};


static const thstok thtt_cfg[] = {
  {"export", TT_EXPORT},
  {"select", TT_SELECT},
  {"source", TT_SOURCE},
  {"unselect", TT_UNSELECT},
  {NULL, TT_UNKNOWN_CFG}
};


//const char * THCCC_ENCODING = "# Character encoding of this configuration "//  "file.\n";
const char * THCCC_ENCODING = "";
const char * THCCC_SOURCE = "# Name of the source file.\n";


thconfig::thconfig()
{

#ifdef THWIN32
  const char * winini = "C:/WINDOWS;C:/WINNT;C:/Program files/therion";
  const char * wincfg = "C:/Program files/therion";
#else
  const char * unixini = "/etc:/usr/etc:/usr/local/etc";
  const char * unixcfg = "/usr/share/therion:/usr/local/share/therion";
#endif
  
  this->fname = "thconfig";
  this->skip_comments = false;
  this->generate_xthcfg = false;
  this->cfg_fenc = TT_UTF_8;
  this->fstate = THCFG_READ;

  char * sp = getenv("THERION");
  if (sp != NULL)
    this->search_path = sp;
  else {
    sp = getenv("HOME");
    if (sp != NULL) {
      this->search_path = sp;
#ifdef THWIN32
      this->search_path += "/.therion;";
      this->search_path += wincfg;
#else
      this->search_path += "/.therion:";
      this->search_path += unixcfg;
#endif
    }
    else {
#ifdef THWIN32
      this->search_path += wincfg;
#else
      this->search_path += unixcfg;
#endif
    }
  }


  sp = getenv("THINIT");
  if (sp != NULL)
    this->init_path = sp;
  else {
    sp = getenv("HOME");
    if (sp != NULL) {
      this->init_path = sp;
#ifdef THWIN32
      this->init_path += "/.therion;";
      this->init_path += winini;
#else
      this->init_path += "/.therion:";
      this->init_path += unixini;
#endif
    }
    else {
#ifdef THWIN32
      this->init_path += winini;
#else
      this->init_path += unixini;
#endif
    }
  }

  this->exporter.assign_config(this);
  this->selector.assign_config(this);
}


thconfig::~thconfig()
{
}


void thconfig::set_file_name(char * fn)
{
  this->fname = fn;
}

   
char * thconfig::get_file_name()
{
  return this->fname;
}
  

void thconfig::set_source_file_name(char * fn)
{
  this->src_fnames.append(fn);
}

  
thmbuffer * thconfig::get_source_file_names() 
{
  return & this->src_fnames;
}  


void thconfig::set_comments_skip(bool state)
{
  this->skip_comments = state;
}
  

void thconfig::comments_skip_on()
{
  this->skip_comments = true;
}
  

void thconfig::comments_skip_off()
{
  this->skip_comments = false;
}
  

bool thconfig::get_comments_skip()
{
  return this->skip_comments;
}


void thconfig::set_file_state(thcfg_fstate fs)
{
  this->fstate = fs;
}
 
 
thcfg_fstate thconfig::get_file_state()
{
  return this->fstate;
}


void thconfig::set_search_path(char * pth)
{
  this->search_path = pth;
}

  
char * thconfig::get_search_path()
{
  return this->search_path;
}

char * thconfig::get_initialization_path()
{
  return this->init_path;
}


void thconfig::load() 
{
  thmbuffer valuemb;
  if ((this->fstate == THCFG_UPDATE) || (this->fstate == THCFG_READ)) {
    this->cfg_file.cmd_sensitivity_on();
    this->cfg_file.sp_scan_off();
    this->cfg_file.set_file_name(this->fname);
    this->cfg_file.reset();
    try {
      char * cfgln = this->cfg_file.read_line();
      while(cfgln != NULL) {
        this->cfg_fenc = this->cfg_file.get_cif_encoding();
        thsplit_args(&valuemb, this->cfg_file.get_value());
        switch (thmatch_token(this->cfg_file.get_cmd(),thtt_cfg)) {
  
          case TT_SOURCE:
            if (valuemb.get_size() != 1)
              ththrow(("one file name expected"))            
            this->src_fnames.append(valuemb.get_buffer()[0]);
            break;
  
          case TT_SELECT:
            this->selector.parse_selection(false, valuemb.get_size(), valuemb.get_buffer());
            break;
            
          case TT_UNSELECT:
            this->selector.parse_selection(true, valuemb.get_size(), valuemb.get_buffer());
            break;
            
          case TT_EXPORT:
            this->exporter.parse_export(valuemb.get_size(), valuemb.get_buffer());
            break;
  
          case TT_UNKNOWN_CFG:
	          // skusi povolene databazove prikazy
            switch (thmatch_token(this->cfg_file.get_cmd(),thtt_commands)) {
            
              case TT_LAYOUT_CMD:
                this->load_dbcommand(&valuemb);
                break;

              default:
                ththrow(("unknown configuration command -- %s", this->cfg_file.get_cmd()));
                
	          }
        }
        cfgln = this->cfg_file.read_line(); 
      }
    }
    catch (...)
      threthrow(("%s [%d]", this->cfg_file.get_cif_name(), this->cfg_file.get_cif_line_number()));
  }
}


void thconfig::load_dbcommand(thmbuffer * valmb) {

  // vytvori objekt
  // analyzuje jeho argumenty a prida riadok do dblines
  // nacita ostatne riadky, ak je to potrebne a analyzuje ich
  // vlozi ho do databazy
  // kazdy riadok prida do prikazov na ulozenie

  thdataobject * objptr;  // pointer to the newly created object
  thcmd_option_desc optd;  // option descriptor
  char * ln, * endlnopt = NULL, * opt, ** opts;
  int ai, ait, ant;
  thobjectsrc osrc;
  bool inside_cmd = false;
  
  try {

    // command source
    osrc.line = this->cfg_file.get_cif_line_number();
    if (strcmp(osrc.name, this->cfg_file.get_cif_name()) != 0)
      osrc.name = dbptr->strstore(this->cfg_file.get_cif_name(), true);
    dbptr->csrc = osrc;

    objptr = dbptr->create(this->cfg_file.get_cmd(), osrc);
    if (objptr == NULL)
      ththrow(("unknown command -- %s", this->cfg_file.get_cmd()));
    thencode(&this->bf1, this->cfg_file.get_line(), this->cfg_file.get_cif_encoding());  
    this->cfg_dblines.append(this->bf1.get_buffer());  

    // analyze the commands options

    // first, let's parse arguments
    ant = valmb->get_size();
    opts = valmb->get_buffer();
    if (ant < objptr->get_cmd_nargs())
      ththrow(("not enought command arguments -- must be %d",
        objptr->get_cmd_nargs()));
    optd.nargs = 1;

    // set obligatory arguments
    for (ai = 0; ai < objptr->get_cmd_nargs(); ai++, opts++) {
      optd.id = ai + 1;
      objptr->set(optd, opts, this->cfg_file.get_cif_encoding(),
        thdatareader__get_opos(false,false));
    }

    // set options
    ait = ai;
    while (ait < ant) {
      optd = objptr->get_cmd_option_desc(*opts + 1);
      if (optd.id == TT_DATAOBJECT_UNKNOWN) {
        optd.id = ++ai;
        optd.nargs = 1;
      }
      else {
        if ((ait + optd.nargs) >= ant)
          ththrow(("not enought option arguments -- %s -- must be %d", *opts, optd.nargs));
        opts++;
        ait++;
      }
 
      objptr->set(optd, opts, this->cfg_file.get_cif_encoding(),
        thdatareader__get_opos(false,false));
      opts += optd.nargs;
      ait += optd.nargs;
    }       
    
    // if multi line, set that we're inside the command
    // and switch of sensitivity       
    // else insert object into database
    if ((endlnopt = objptr->get_cmd_end()) != NULL) {
      inside_cmd = true;
      this->cfg_file.cmd_sensitivity_off();
  
      while (inside_cmd) {
      
        if ((ln = this->cfg_file.read_line()) == NULL)
          ththrow(("%s [%d] -- %s is missing",this->cfg_file.get_cif_name(),
            this->cfg_file.get_cif_line_number(),endlnopt))
                      
        thencode(&this->bf1, ln, this->cfg_file.get_cif_encoding());  
        this->cfg_dblines.append(this->bf1.get_buffer());  

        thsplit_word(&this->bf1, &this->bf2, ln);
             
        // if end_command option, set turn off inside_cmd
        // and insert object into database
        if (strcmp(this->bf1.get_buffer(), endlnopt) == 0) {
          inside_cmd = false;
          this->cfg_file.cmd_sensitivity_on();
          continue;   
        }
  
        // let's parse if an option line
        optd = objptr->get_cmd_option_desc(this->bf1.get_buffer());
        if (optd.id != TT_DATAOBJECT_UNKNOWN) {
          thsplit_args(&this->mbf1, this->bf2.get_buffer());
          if (this->mbf1.get_size() < optd.nargs)
            ththrow(("not enought option arguments -- %s -- must be %d",
              this->bf1.get_buffer(), optd.nargs));
          optd.nargs = this->mbf1.get_size();
          objptr->set(optd, this->mbf1.get_buffer(), 
            this->cfg_file.get_cif_encoding(),
            thdatareader__get_opos(inside_cmd,false));
          continue;
        }
        
        // if data line (!) set data option      
        optd.id = 0;
        optd.nargs = 1;
        opt = ln;
        while(strcmp(opt,"!") < 0) opt++;
        if (*opt == '!') opt++;
        objptr->set(optd, & opt, this->cfg_file.get_cif_encoding(),
          thdatareader__get_opos(inside_cmd,false));
      }  
    }
    
    // vlozi objekt do databazy
    dbptr->insert(objptr);
  }
    
  // put everything into try block and throw exception, if error
  catch (...)
    threthrow(("%s [%d]", this->cfg_file.get_cif_name(), this->cfg_file.get_cif_line_number()))

}


void thconfig::save()
{
  if ((this->fstate == THCFG_UPDATE) || (this->fstate == THCFG_GENERATE)) {
  
#ifdef THDEBUG
    thprintf("\nwriting configuration file -- %s\n", this->fname.get_buffer());
#else
    thprintf("writing configuration file ... ");
    thtext_inline = true;
#endif 

    // OK, let's open configuration file for output
    FILE * cf;
    cf = fopen(this->fname.get_buffer(),"w");
    if (cf == NULL) {
      thwarning(("can't open configuration file for output -- %s",           this->fname.get_buffer()));
      return;
    }
  
    // first, let's write file encoding
    if (!this->skip_comments)
      fprintf(cf,"%s",THCCC_ENCODING);
    fprintf(cf,"encoding %s\n\n",thmatch_string(this->cfg_fenc, thtt_encoding));
    
    // source file
    long sid;
    char ** srcn = src_fnames.get_buffer();
    bool some_src = false;
    for(sid = 0; sid < this->src_fnames.get_size(); sid++, srcn++) {
      if (!some_src) {
        if (!this->skip_comments)
          fprintf(cf,"%s",THCCC_SOURCE);
      }
      fprintf(cf,"source \"%s\"\n",*srcn);
      some_src = true;
    }
    if (some_src)
      fprintf(cf,"\n");

    // layouts etc.
    srcn = this->cfg_dblines.get_buffer();
    bool some_layout = false;
    for(sid = 0; sid < this->cfg_dblines.get_size(); sid++, srcn++) {
      thdecode(&(this->bf1), this->cfg_fenc, *srcn);
      fprintf(cf, "%s\n", this->bf1.get_buffer());
      some_layout = true;
    }
    if (some_layout)
      fprintf(cf,"\n");
    
    // selected objects
    this->selector.dump_selection(cf);    
    
    // dump readed export
    this->exporter.dump_export(cf);
    
    // dump possibilities objects
    // this->selector.dump_selection_db(cf, this->dbptr);
    
    // close config file
    fclose(cf);
    
#ifdef THDEBUG
    thprintf("\n");
#else
    thprintf("done.\n");
    thtext_inline = false;
#endif 

  }
}
  

void thconfig::assign_db(class thdatabase * dp)
{
  this->dbptr = dp;
}


void thconfig::select_data()
{
  this->selector.select_db(this->dbptr);
}


void thconfig::export_data()
{
  this->exporter.export_db(this->dbptr);
}



void thconfig::xth_save()
{
  if (this->generate_xthcfg) {
  
#ifdef THDEBUG
    thprintf("\nwriting xtherion file -- .xth-%s\n", this->fname.get_buffer());
#else
    thprintf("writing xtherion file ... ");
    thtext_inline = true;
#endif 

    // OK, let's open configuration file for output
    FILE * cf;
    this->dbptr->buff_tmp = ".xth-";
    this->dbptr->buff_tmp += this->fname.get_buffer();
    cf = fopen(this->dbptr->buff_tmp.get_buffer(),"w");
    if (cf == NULL) {
      thwarning(("can't open xtherion file for output -- %s.xth", this->fname.get_buffer()));
      return;
    }
  
    // dump possibilities objects
    this->selector.dump_selection_db(cf, this->dbptr);
    
    // close config file
    fclose(cf);
    
#ifdef THDEBUG
    thprintf("\n");
#else
    thprintf("done.\n");
    thtext_inline = false;
#endif 

  }
}
  




thconfig thcfg;




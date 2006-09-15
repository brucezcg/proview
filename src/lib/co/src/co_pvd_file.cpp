/* 
 * Proview   $Id: co_pvd_file.cpp,v 1.1 2006-09-14 14:16:07 claes Exp $
 * Copyright (C) 2005 SSAB Oxel�sund AB.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation, either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with the program, if not, write to the Free Software 
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 **/

#include <vector.h>
#include <string.h>
#include <stdio.h>
#include <iostream.h>
#include <fstream.h>
#include "pwr.h"
#include "pwr_class.h"
#include "pwr_baseclasses.h"
#include "wb_vext.h"
#include "co_pvd_file.h"
#include "wb_ldh.h"
#include "wb_ldh_msg.h"
#include "rt_gdh_msg.h"

extern "C" {
#include "co_cdh.h"
#include "co_dcli.h"
}

#define START_OIX 1000

// Wb only
void co_pvd_file::object( co_procom *pcom)
{
  if ( m_list.size() <= 1 || m_list[0].fchoix == 0) {
    pcom->provideObject( LDH__NOSUCHOBJ,0,0,0,0,0,0,0,"","");
    return;
  }
  objectOid( pcom, m_list[0].fchoix);
}

void co_pvd_file::objectOid( co_procom *pcom, pwr_tOix oix)
{
  if ( m_env == pvd_eEnv_Wb) {
    if ( oix >= m_list.size() || oix <= 0) {
      pcom->provideObject( LDH__NOSUCHOBJ,0,0,0,0,0,0,0,"","");
      return;
    }
    pcom->provideObject( 1, oix, m_list[oix].fthoix, m_list[oix].bwsoix,
			 m_list[oix].fwsoix, m_list[oix].fchoix, m_list[oix].lchoix,
			 m_list[oix].cid, m_list[oix].name, longname(oix));
  }
  else {
    if ( oix >= m_list.size() || oix < 0) {
      pcom->provideStatus( GDH__NOSUCHOBJ);
      return;
    }
    pcom->provideObjects( GDH__SUCCESS, m_list);
  }
}

void co_pvd_file::objectName( co_procom *pcom, char *name)
{

  for ( int i = 0; i < (int) m_list.size(); i++) {
    if  ( !m_list[i].flags & procom_obj_mFlags_Deleted) {
      if ( cdh_NoCaseStrcmp( name, longname(m_list[i].oix)) == 0) {
	objectOid( pcom, i);
	return;
      }
    }
  }
  if ( m_env == pvd_eEnv_Wb)
    pcom->provideObject( 0,0,0,0,0,0,0,0,"","");
  else
    pcom->provideStatus( GDH__NOSUCHOBJ);

}

// Wb only
void co_pvd_file::objectBody( co_procom *pcom, pwr_tOix oix)
{
  if ( oix >= m_list.size() || oix <= 0) {
    pcom->provideBody( 0,0,0,0);
    return;
  }

  if ( m_list[oix].body)
    pcom->provideBody( 1, oix, m_list[oix].body_size, m_list[oix].body);
  else
    pcom->provideBody( 0, 0, 0, 0);
}

// Wb only
void co_pvd_file::createObject( co_procom *pcom, pwr_tOix destoix, int desttype,
		     pwr_tCid cid, char *name)
{
  procom_obj item;

  if ( destoix >= m_list.size()) {
    pcom->provideObject( LDH__NOSUCHOBJ,0,0,0,0,0,0,0,"","");
    return;
  }

  if ( destoix == 0 && m_list.size() == 1) {
    // First object
    item.cid = cid;
    item.oix = next_oix++;
    item.fthoix = destoix;
    item.fwsoix = 0;
    item.bwsoix = 0;
    item.fchoix = 0;
    item.lchoix = 0;
    m_list[0].fchoix = item.oix;
  }
  else {
    if ( destoix >= m_list.size() || destoix <= 0) {
      pcom->provideObject( 0,0,0,0,0,0,0,0,"","");
      return;
    }

    item.cid = cid;
    item.oix = next_oix++;
    switch ( desttype) {
    case ldh_eDest_IntoFirst:
      item.fthoix = destoix;
      item.fwsoix = m_list[item.fthoix].fchoix;
      item.bwsoix = 0;
      item.fchoix = 0;
      item.lchoix = 0;
      if ( item.fwsoix != 0)
	m_list[item.fwsoix].bwsoix = item.oix;
      m_list[item.fthoix].fchoix = item.oix;
      if ( m_list[item.fthoix].lchoix == 0)
	m_list[item.fthoix].lchoix = item.oix;
      break;
    case ldh_eDest_IntoLast:
      item.fthoix = destoix;
      item.bwsoix = m_list[item.fthoix].lchoix;
      item.fwsoix = 0;
      item.fchoix = 0;
      item.lchoix = 0;
      if ( item.bwsoix != 0)
	m_list[item.bwsoix].fwsoix = item.oix;
      m_list[item.fthoix].lchoix = item.oix;
      if ( m_list[item.fthoix].fchoix == 0)
	m_list[item.fthoix].fchoix = item.oix;
      break;
    case ldh_eDest_After:
      item.fthoix = m_list[destoix].fthoix;
      item.fwsoix = m_list[destoix].fwsoix;
      item.bwsoix = destoix;
      item.fchoix = 0;
      item.lchoix = 0;
      m_list[destoix].fwsoix = item.oix;
      if ( item.fwsoix != 0)
	m_list[item.fwsoix].bwsoix = item.oix;
      if ( item.fthoix != 0) 
	if ( m_list[item.fthoix].lchoix == destoix)
	  m_list[item.fthoix].lchoix = item.oix;
      break;
    case ldh_eDest_Before:
      item.fthoix = m_list[destoix].fthoix;
      item.bwsoix = m_list[destoix].bwsoix;
      item.fwsoix = destoix;
      item.fchoix = 0;
      item.lchoix = 0;
      m_list[destoix].bwsoix = item.oix;
      if ( item.bwsoix != 0)
	m_list[item.bwsoix].fwsoix = item.oix;
      if ( item.fthoix != 0)
	if ( m_list[item.fthoix].fchoix == destoix)
	  m_list[item.fthoix].fchoix = item.oix;
      break;
    default: ;
    }
  }

  
  switch (cid) {
  case pwr_eClass_Hier:
    item.body_size = sizeof(pwr_sHier);
    item.body = calloc( 1, item.body_size);
    break;
  case pwr_cClass_VolumeReg:
    item.body_size = sizeof( pwr_sClass_VolumeReg);
    item.body = calloc( 1, item.body_size);
    break;
  case pwr_cClass_SystemGroupReg:
    item.body_size = sizeof( pwr_sClass_SystemGroupReg);
    item.body = calloc( 1, item.body_size);
    break;
  case pwr_cClass_UserReg:
    item.body_size = sizeof( pwr_sClass_UserReg);
    item.body = calloc( 1, item.body_size);
    break;
  case pwr_cClass_ProjectReg:
    item.body_size = sizeof( pwr_sClass_ProjectReg);
    item.body = calloc( 1, item.body_size);
    break;
  case pwr_cClass_BaseReg:
    item.body_size = sizeof( pwr_sClass_BaseReg);
    item.body = calloc( 1, item.body_size);
    break;
  }
  if ( strcmp( name, "") == 0)
    sprintf( item.name, "O%d", item.oix);
  else
    strcpy( item.name, name);
  item.flags |= procom_obj_mFlags_Created;
  m_list.push_back(item);

  pcom->provideObject( 1, item.oix, item.fthoix, item.bwsoix, item.fwsoix,
		       item.fchoix, item.lchoix, item.cid, item.name, longname(item.oix));
}

// Wb only
void co_pvd_file::moveObject( co_procom *pcom, pwr_tOix oix, pwr_tOix destoix, 
				 int desttype)
{
  if ( destoix >= m_list.size() || destoix <= 0) {
    pcom->provideStatus( 0);
    return;
  }
  if ( oix >= m_list.size() || oix <= 0) {
    pcom->provideStatus( 0);
    return;
  }

  // Remove from current position
  if ( m_list[m_list[oix].fthoix].fchoix == oix)
    m_list[m_list[oix].fthoix].fchoix = m_list[oix].fwsoix;
  if ( m_list[m_list[oix].fthoix].lchoix == oix)
    m_list[m_list[oix].fthoix].lchoix = m_list[oix].bwsoix;
  if ( m_list[oix].bwsoix)
    m_list[m_list[oix].bwsoix].fwsoix = m_list[oix].fwsoix;
  if ( m_list[oix].fwsoix)
    m_list[m_list[oix].fwsoix].bwsoix = m_list[oix].bwsoix;

  // Insert in new position
  switch ( desttype) {
  case ldh_eDest_IntoFirst:
    m_list[oix].fthoix = destoix;
    m_list[oix].fwsoix = m_list[destoix].fchoix;
    if ( m_list[destoix].fchoix == 0)
      m_list[destoix].lchoix = oix;
    m_list[destoix].fchoix = oix;
    if ( m_list[oix].fwsoix)
      m_list[m_list[oix].fwsoix].bwsoix = oix;
    m_list[oix].bwsoix = 0;
    break;
  case ldh_eDest_IntoLast:
    m_list[oix].fthoix = destoix;
    m_list[oix].bwsoix = m_list[destoix].lchoix;
    if ( m_list[destoix].fchoix == 0)
      m_list[destoix].fchoix = oix;
    m_list[destoix].lchoix = oix;
    if ( m_list[oix].bwsoix)
      m_list[m_list[oix].bwsoix].fwsoix = oix;
    m_list[oix].fwsoix = 0;
    break;
  case ldh_eDest_After:
    m_list[oix].fthoix = m_list[destoix].fthoix;
    m_list[oix].fwsoix = m_list[destoix].fwsoix;
    m_list[destoix].fwsoix = oix;
    m_list[oix].bwsoix = destoix;
    if ( m_list[m_list[oix].fthoix].lchoix == destoix)
      m_list[m_list[oix].fthoix].lchoix = oix;
    if ( m_list[oix].fwsoix)
      m_list[m_list[oix].fwsoix].bwsoix = oix;
    break;
  case ldh_eDest_Before:
    m_list[oix].fthoix = m_list[destoix].fthoix;
    m_list[oix].bwsoix = m_list[destoix].bwsoix;
    m_list[destoix].bwsoix = oix;
    m_list[oix].fwsoix = destoix;
    if ( m_list[m_list[oix].fthoix].fchoix == destoix)
      m_list[m_list[oix].fthoix].fchoix = oix;
    if ( m_list[oix].bwsoix)
      m_list[m_list[oix].bwsoix].fwsoix = oix;
    break;
  default: ;
  }

  pcom->provideStatus( 1);
}

// Wb only
void co_pvd_file::deleteObject( co_procom *pcom, pwr_tOix oix)
{
  if ( oix >= m_list.size() || oix <= 0) {
    pcom->provideStatus( 0);
    return;
  }
  m_list[oix].flags |= procom_obj_mFlags_Deleted;

  // Remove from current position
  if ( m_list[oix].fthoix && m_list[m_list[oix].fthoix].fchoix == oix)
    m_list[m_list[oix].fthoix].fchoix = m_list[oix].fwsoix;
  if ( m_list[oix].fthoix && m_list[m_list[oix].fthoix].lchoix == oix)
    m_list[m_list[oix].fthoix].lchoix = m_list[oix].bwsoix;
  if ( m_list[oix].bwsoix)
    m_list[m_list[oix].bwsoix].fwsoix = m_list[oix].fwsoix;
  if ( m_list[oix].fwsoix)
    m_list[m_list[oix].fwsoix].bwsoix = m_list[oix].bwsoix;

  pcom->provideStatus( 1);
}

// Wb only
void co_pvd_file::copyObject( co_procom *pcom, pwr_tOix oix, pwr_tOix destoix, int desttype,
				 char *name)
{

  if ( destoix >= m_list.size() || oix >= m_list.size() || oix <= 0) {
    pcom->provideObject( LDH__NOSUCHOBJ,0,0,0,0,0,0,0,"","");
    return;
  }
  procom_obj item = m_list[oix];
  item.oix = next_oix++;

  if ( strcmp( name, "") == 0)
    sprintf( item.name, "O%d_%s", next_oix, m_list[oix].name);
  else
    strcpy( item.name, name);

  // Insert in new position
  switch ( desttype) {
  case ldh_eDest_IntoFirst:
    item.fthoix = destoix;
    item.fwsoix = m_list[destoix].fchoix;
    if ( m_list[destoix].fchoix == 0)
      m_list[destoix].lchoix = item.oix;
    m_list[destoix].fchoix = item.oix;
    if ( item.fwsoix)
      m_list[item.fwsoix].bwsoix = item.oix;
    item.bwsoix = 0;
    break;
  case ldh_eDest_IntoLast:
    item.fthoix = destoix;
    item.bwsoix = m_list[destoix].lchoix;
    if ( m_list[destoix].fchoix == 0)
      m_list[destoix].fchoix = item.oix;
    m_list[destoix].lchoix = item.oix;
    if ( item.bwsoix)
      m_list[item.bwsoix].fwsoix = item.oix;
    item.fwsoix = 0;
    break;
  case ldh_eDest_After:
    item.fthoix = m_list[destoix].fthoix;
    item.fwsoix = m_list[destoix].fwsoix;
    m_list[destoix].fwsoix = item.oix;
    item.bwsoix = destoix;
    if ( m_list[item.fthoix].lchoix == destoix)
      m_list[item.fthoix].lchoix = item.oix;
    if ( item.fwsoix)
      m_list[item.fwsoix].bwsoix = item.oix;
    break;
  case ldh_eDest_Before:
    item.fthoix = m_list[destoix].fthoix;
    item.bwsoix = m_list[destoix].bwsoix;
    m_list[destoix].bwsoix = item.oix;
    item.fwsoix = destoix;
    if ( m_list[item.fthoix].fchoix == destoix)
      m_list[item.fthoix].fchoix = item.oix;
    if ( item.bwsoix)
      m_list[item.bwsoix].fwsoix = item.oix;
    break;
  default: ;
  }
  m_list.push_back(item);

  pcom->provideObject( 1, item.oix, item.fthoix, item.bwsoix, item.fwsoix,
		       item.fchoix, item.lchoix, item.cid, item.name, longname(item.oix));
}

// Wb only
void co_pvd_file::deleteFamily( co_procom *pcom, pwr_tOix oix)
{
  if ( oix >= m_list.size() || oix <= 0) {
    pcom->provideStatus( 0);
    return;
  }
  delete_tree( oix);

  // Remove from current position
  if ( m_list[oix].fthoix && m_list[m_list[oix].fthoix].fchoix == oix)
    m_list[m_list[oix].fthoix].fchoix = m_list[oix].fwsoix;
  if ( m_list[oix].fthoix && m_list[m_list[oix].fthoix].lchoix == oix)
    m_list[m_list[oix].fthoix].lchoix = m_list[oix].bwsoix;
  if ( m_list[oix].bwsoix)
    m_list[m_list[oix].bwsoix].fwsoix = m_list[oix].fwsoix;
  if ( m_list[oix].fwsoix)
    m_list[m_list[oix].fwsoix].bwsoix = m_list[oix].bwsoix;

  pcom->provideStatus( 1);
}

// Wb only
void co_pvd_file::renameObject( co_procom *pcom, pwr_tOix oix, char *name)
{
  if ( oix >= m_list.size() || oix <= 0) {
    pcom->provideStatus( 0);
    return;
  }
  strcpy( m_list[oix].name, name);

  pcom->provideStatus( 1);
}

void co_pvd_file::writeAttribute( co_procom *pcom, pwr_tOix oix, unsigned int offset,
		       unsigned int size, char *buffer)
{
  if ( oix >= m_list.size() || oix <= 0) {
    pcom->provideStatus( LDH__NOSUCHOBJ);
    return;
  }

  if ( offset + size > m_list[oix].body_size) {
    pcom->provideStatus( LDH__NOSUCHATTR);
    return;
  }

  memcpy( (void *)((unsigned long)m_list[oix].body + (unsigned long)offset), buffer, size);
  pcom->provideStatus( 1);
}

// Rt only
void co_pvd_file::readAttribute( co_procom *pcom, pwr_tOix oix, unsigned int offset,
		       unsigned int size)
{
  if ( oix >= m_list.size() || oix <= 0) {
    pcom->provideStatus( GDH__NOSUCHOBJ);
    return;
  }

  if ( offset + size > m_list[oix].body_size) {
    pcom->provideStatus( GDH__NOSUCHOBJ);
    return;
  }

  void *p = (void *)((unsigned long)m_list[oix].body + (unsigned long)offset);
  pcom->provideAttr( GDH__SUCCESS, oix, size, p);
}

// Rt only
void co_pvd_file::subAssociateBuffer( co_procom *pcom, void **buff, int oix, int offset, 
				      int size, pwr_tSubid sid) 
{
  if ( oix < (int)m_list.size())
    *buff = (char *)m_list[oix].body + offset;
  else 
    *buff = 0;
}

// Wb only
void co_pvd_file::commit( co_procom *pcom)
{
  pwr_tStatus sts;

  save( &sts);
  pcom->provideStatus( sts);
}

// Wb only
void co_pvd_file::abort( co_procom *pcom)
{
  pwr_tStatus sts;

  m_list.clear();
  next_oix = 1;
  load( &sts);
  pcom->provideStatus( sts);
}

void co_pvd_file::delete_tree( pwr_tOix oix)
{
  m_list[oix].flags = procom_obj_mFlags_Deleted;

  for ( pwr_tOix ix = m_list[oix].fchoix;
	ix;
	ix = m_list[ix].fwsoix)
    delete_tree( ix);
}

char *co_pvd_file::longname( pwr_tOix oix)
{
  if ( m_list[oix].fthoix == 0)
    strcpy( m_list[oix].lname, m_list[oix].name);
  else {
    strcpy( m_list[oix].lname, longname( m_list[oix].fthoix));
    strcat( m_list[oix].lname, "-");
    strcat( m_list[oix].lname, m_list[oix].name);
  }
  return m_list[oix].lname;
}

bool co_pvd_file::find( pwr_tOix fthoix, char *name, pwr_tOix *oix)
{

  for ( int i = 0; i < (int) m_list.size(); i++) {
    if  ( !m_list[i].flags & procom_obj_mFlags_Deleted) {
      if ( m_list[i].fthoix == fthoix && 
	   cdh_NoCaseStrcmp( name, m_list[i].name) == 0) {
	*oix = m_list[i].oix;
	return true;
      }
    }
  }
  return false;
}


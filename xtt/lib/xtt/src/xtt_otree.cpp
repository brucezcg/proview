/* 
 * Proview   Open Source Process Control.
 * Copyright (C) 2005-2017 SSAB EMEA AB.
 *
 * This file is part of Proview.
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
 * along with Proview. If not, see <http://www.gnu.org/licenses/>
 *
 * Linking Proview statically or dynamically with other modules is
 * making a combined work based on Proview. Thus, the terms and 
 * conditions of the GNU General Public License cover the whole 
 * combination.
 *
 * In addition, as a special exception, the copyright holders of
 * Proview give you permission to, from the build function in the
 * Proview Configurator, combine Proview with modules generated by the
 * Proview PLC Editor to a PLC program, regardless of the license
 * terms of these modules. You may copy and distribute the resulting
 * combined work under the terms of your choice, provided that every 
 * copy of the combined work is accompanied by a complete copy of 
 * the source code of Proview (the version used to produce the 
 * combined work), being distributed under the terms of the GNU 
 * General Public License plus this exception.
 **/

#include "pwr.h"
#include "rt_gdh.h"
#include "co_cdh.h"
#include "xtt_otree.h"

/* xtt_otree.h -- Object tree viewer */


XttOTree::XttOTree( void *xn_parent_ctx, pwr_tAttrRef *xn_itemlist, int xn_item_cnt, unsigned int xn_options,
		    pwr_tStatus (*xn_action_cb)( void *, pwr_tAttrRef *)) :
  parent_ctx(xn_parent_ctx), close_cb(0)
{
  action_cb = xn_action_cb;
  // cowtree = new CowTree( this, xn_itemlist, xn_item_cnt, xn_layout, get_object_info, get_node_info);
}

void XttOTree::pop()
{
  cowtree->pop();
}

pwr_tStatus XttOTree::get_object_info( void *ctx, pwr_tAttrRef *aref, char *name, int nsize, char *cname, 
				       char *descr, int dsize) 
{
  pwr_tStatus sts;
  pwr_tAttrRef daref;
  pwr_tCid cid;

  sts = gdh_AttrrefToName( aref, name, nsize, cdh_mNName);
  if ( EVEN(sts)) return sts;

  sts = gdh_GetAttrRefTid( aref, &cid);
  if ( EVEN(sts)) return sts;

  sts = gdh_ObjidToName( cdh_ClassIdToObjid(cid), cname, sizeof(pwr_tObjName), cdh_mName_object);
  if ( EVEN(sts)) return sts;

  sts = gdh_ArefANameToAref( aref, "Description", &daref);
  if ( ODD(sts)) {
    sts = gdh_GetObjectInfoAttrref( &daref, descr, dsize);
  }
  if ( EVEN(sts))
    strcpy( descr, "");

  return 1;
}

pwr_tStatus XttOTree::get_node_info( void *ctx, char *name, char *descr, int dsize) 
{
  pwr_tStatus sts;
  pwr_tAName dname;

  strncpy( dname, name, sizeof(dname));
  strncat( dname, ".Description", sizeof(dname));
  sts = gdh_GetObjectInfo( dname, descr, dsize);
  if ( EVEN(sts))
    strcpy( descr, "");
  return sts;
}

pwr_tStatus XttOTree::action( void *ctx, pwr_tAttrRef *aref)
{
  XttOTree *otree = (XttOTree *)ctx;

  if ( otree->action_cb)
    return (otree->action_cb)( otree->parent_ctx, aref);
  return 0;
}

void XttOTree::close( void *ctx)
{
  XttOTree *otree = (XttOTree *)ctx;

  if ( otree->close_cb)
    (otree->close_cb)( otree->parent_ctx);
  else 
    delete otree;
}

XttOTree::~XttOTree() {
}

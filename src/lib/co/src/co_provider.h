/* 
 * Proview   $Id: co_provider.h,v 1.1 2006-09-14 14:16:07 claes Exp $
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
 */

#ifndef co_provider_h
#define co_provider_h


class co_procom;

typedef enum {
  pvd_eEnv_Wb,
  pvd_eEnv_Rt
} pvd_eEnv;

class co_provider {
public:
  pvd_eEnv m_env;

  co_provider( pvd_eEnv env = pvd_eEnv_Wb) : m_env(env) {}
  virtual ~co_provider() {}
  virtual void object( co_procom *pcom) {}
  virtual void objectOid( co_procom *pcom, pwr_tOix oix) {}
  virtual void objectName( co_procom *pcom, char *name) {}
  virtual void objectBody( co_procom *pcom, pwr_tOix oix) {}
  virtual void createObject( co_procom *pcom, pwr_tOix destoix, int desttype,
			     pwr_tCid cid, char *name) {}
  virtual void moveObject( co_procom *pcom, pwr_tOix oix, pwr_tOix destoix, 
			   int desttype) {}
  virtual void copyObject( co_procom *pcom, pwr_tOix oix, pwr_tOix destoix, int desttype,
			   char *name) {}
  virtual void deleteObject( co_procom *pcom, pwr_tOix oix) {}
  virtual void deleteFamily( co_procom *pcom, pwr_tOix oix) {}
  virtual void renameObject( co_procom *pcom, pwr_tOix oix, char *name) {}
  virtual void writeAttribute( co_procom *pcom, pwr_tOix oix, unsigned int offset,
			       unsigned int size, char *buffer) {}
  virtual void readAttribute( co_procom *pcom, pwr_tOix oix, unsigned int offset,
			       unsigned int size) {}
  virtual void commit( co_procom *pcom) {}
  virtual void abort( co_procom *pcom) {}
  virtual void subAssociateBuffer( co_procom *pcom, void **buff, int oix, int offset, 
				    int size, pwr_tSubid sid) {}
  virtual void subDisassociateBuffer( co_procom *pcom, pwr_tSubid sid) {}
  virtual void cyclic( co_procom *pcom) {}
};

#endif
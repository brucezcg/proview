/* 
 * Proview   $Id: wb_c_ao_ao8up.cpp,v 1.1 2007-01-04 08:46:09 claes Exp $
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

/* wb_c_ao_ao8up.c -- work bench methods of the Ao_AO8uP class */

#include <stdio.h>
#include <string.h>
#include "wb_pwrs.h"
#include "wb_ldh_msg.h"
#include "wb_pwrb_msg.h"
#include "pwr_baseclasses.h"
#include "pwr_basecomponentclasses.h"
#include "pwr_ssaboxclasses.h"
#include "wb_ldh.h"
#include "wb_wsx.h"


/* AnteAdopt -- check if it's ok to adopt a new object  */

static pwr_tStatus AnteAdopt (
  ldh_tSesContext Session,
  pwr_tObjid Card,	      /* current card object */
  pwr_tClassId Class	      /* class of child to adopt */
) {
  pwr_tStatus sts;
  pwr_sClass_Ao_AO8uP RCard;
  pwr_sdClass_Ao_AO8uP DCard;
  pwr_tUInt32 MaxChan;
  pwr_tUInt32 Chan;
  int i;
  
  if (Class != pwr_cClass_ChanAo)
    return PWRB__CHANCLASS;

  sts = ldh_ReadObjectBody(Session, Card, "RtBody", &RCard, sizeof(RCard));
  if (EVEN(sts)) return sts;

  sts = ldh_ReadObjectBody(Session, Card, "DevBody", &DCard, sizeof(DCard));
  if (EVEN(sts)) return sts;

  MaxChan = co_min(32, RCard.MaxNoOfChannels);
  for (i = 0, Chan = 1; i < (int)MaxChan; i++, Chan <<= 1) {
    if ((DCard.ChannelAllocation & Chan) == 0)
      break;
  }

  if (i >= (int)MaxChan)
    return PWRB__ALOCHAN;
  else
    return LDH__ADOPTRENAME;
}

/*----------------------------------------------------------------------------*\
  Adopt a new channel.
\*----------------------------------------------------------------------------*/

static pwr_tStatus PostAdopt (
  ldh_tSesContext Session,
  pwr_tObjid Card,	      /* current card object */
  pwr_tObjid Channel,
  pwr_tClassId Class	      /* class of child to adopt */
) {
  pwr_tStatus sts;
  pwr_sClass_Ao_AO8uP RCard;
  pwr_sdClass_Ao_AO8uP DCard;
  pwr_sClass_ChanAo ChanAo;
  pwr_tUInt32 MaxChan;
  pwr_tUInt32 Chan;
  pwr_tString80 NewName;
  pwr_tString80 Description;
  pwr_tString80 Identity;
  pwr_tString80	DefName;
  pwr_sObject DefBody;
  pwr_tObjid DefObject;
  int i;
  
  if (Class != pwr_cClass_ChanAo)
    return PWRB__CHANCLASS;

  sts = ldh_ReadObjectBody(Session, Card, "RtBody", &RCard, sizeof(RCard));
  if (EVEN(sts)) return sts;

  sts = ldh_ReadObjectBody(Session, Card, "DevBody", &DCard, sizeof(DCard));
  if (EVEN(sts)) return sts;

  MaxChan = co_min(32, RCard.MaxNoOfChannels);
  for (i = 0, Chan = 1; i < (int)MaxChan; i++, Chan <<= 1) {
    if ((DCard.ChannelAllocation & Chan) == 0)
      break;
  }

  if (i >= (int)MaxChan)
    return PWRB__ALOCHAN;

  /* allocate new channel */
  DCard.ChannelAllocation |= Chan;
  sts = ldh_SetObjectBody(Session, Card, "DevBody", (char *)&DCard, sizeof(DCard));
  if (EVEN(sts)) return sts;

  /*
    change attributes of channel

    NOTE !
    this should be done by a method of the channel object,
    but is not implemented in this version of PROVIEW/R.
  */

  switch (Class) {
  case pwr_cClass_ChanAo:
    sts = ldh_ReadObjectBody(Session, Channel, "RtBody", &ChanAo,
      sizeof(ChanAo));
    if (EVEN(sts)) return sts;
    if (ChanAo.Description[0] != '\0') {
      sprintf(Description, ChanAo.Description, i);
      if (strlen(Description) <= sizeof(ChanAo.Description) - 1) {
	strcpy(ChanAo.Description, Description);
      }
    }
    if (ChanAo.Identity[0] != '\0') {
      sprintf(Identity, ChanAo.Identity, i);
      if (strlen(Identity) <= sizeof(ChanAo.Identity) - 1) {
	strcpy(ChanAo.Identity, Identity);
      }
    }

    ChanAo.Number = i;
    sts = ldh_SetObjectBody(Session, Channel, "RtBody", (char *)&ChanAo, sizeof(ChanAo));
    strcpy(DefName, "pwrb:Class-ChanAo-Defaults");
    break;
  }

  /* change name of channel */
  
  sts = ldh_NameToObjid(Session, &DefObject, DefName);
  if (EVEN(sts)) return PWRB__SUCCESS;
  sts = ldh_ReadObjectBody(Session, DefObject, "SysBody", &DefBody,
    sizeof(DefBody));
  if (DefBody.Name[0] != '\0') {
    sprintf(NewName, DefBody.Name, i+1);
    NewName[31] = '\0';
    sts = ldh_SetObjectName(Session, Channel, NewName);
  }

  return PWRB__SUCCESS;
}

/*----------------------------------------------------------------------------*\
  Unadopt a channel.
\*----------------------------------------------------------------------------*/

static pwr_tStatus PostUnadopt (
  ldh_tSesContext Session,
  pwr_tObjid Card,	      /* current card object */
  pwr_tObjid Channel,
  pwr_tClassId Class	      /* class of child to adopt */
) {
  pwr_tStatus sts;
  pwr_sClass_Ao_AO8uP RCard;
  pwr_sClass_ChanAo ChanAo;
  pwr_sdClass_Ao_AO8uP DCard;
  pwr_tUInt32 MaxChan;
  pwr_tUInt32 Chan;
  pwr_tString80 NewName;
  
  if (Class != pwr_cClass_ChanAo)
    return PWRB__SUCCESS;

  sts = ldh_ReadObjectBody(Session, Card, "RtBody", &RCard, sizeof(RCard));
  if (EVEN(sts)) return PWRB__SUCCESS;

  sts = ldh_ReadObjectBody(Session, Card, "DevBody", &DCard, sizeof(DCard));
  if (EVEN(sts)) return PWRB__SUCCESS;

  MaxChan = co_min(32, RCard.MaxNoOfChannels);

  /*
    get attributes of channel

    NOTE !
    this should be done by a method of the channel object,
    but is not implemented in this version of PROVIEW/R.
  */

  switch (Class) {
  case pwr_cClass_ChanAo:
    sts = ldh_ReadObjectBody(Session, Channel, "RtBody", &ChanAo,
      sizeof(ChanAo));
    if (EVEN(sts)) return PWRB__SUCCESS;

    Chan = ChanAo.Number;
    break;
  }

  /* deallocate channel */
  if (Chan > MaxChan)
    return PWRB__SUCCESS;

  DCard.ChannelAllocation &= ~(1 << Chan);
  sts = ldh_SetObjectBody(Session, Card, "DevBody", (char *)&DCard, sizeof(DCard));
  if (EVEN(sts)) return PWRB__SUCCESS;

  sts = ldh_GetUniqueObjectName(Session, Channel, NewName);
  if (EVEN(sts)) return PWRB__SUCCESS;

  sts = ldh_SetObjectName(Session, Channel, NewName);
  if (EVEN(sts)) return PWRB__SUCCESS;

  return PWRB__SUCCESS;
}
/*----------------------------------------------------------------------------*\
  Syntax check.
\*----------------------------------------------------------------------------*/

static pwr_tStatus SyntaxCheck (
  ldh_tSesContext Session,
  pwr_tObjid Object,	      /* current object */
  int *ErrorCount,	      /* accumulated error count */
  int *WarningCount	      /* accumulated waring count */
) {
  pwr_tStatus sts;

  sts = wsx_CheckCard( Session, Object, ErrorCount, WarningCount,
		wsx_mCardOption_DevName);
  if (EVEN(sts)) return sts;

  return PWRB__SUCCESS;
}

/*----------------------------------------------------------------------------*\
  Every method to be exported to the workbench should be registred here.
\*----------------------------------------------------------------------------*/

pwr_dExport pwr_BindMethods(Ao_AO8uP) = {
  pwr_BindMethod(AnteAdopt),
  pwr_BindMethod(PostUnadopt),
  pwr_BindMethod(PostAdopt),
  pwr_BindMethod(SyntaxCheck),
  pwr_NullMethod
};
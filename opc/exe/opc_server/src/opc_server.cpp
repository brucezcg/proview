/* 
 * Proview   $Id: opc_server.cpp,v 1.14 2007-04-05 13:32:03 claes Exp $
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

#include <map.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <pthread.h>

#include "pwr.h"
#include "pwr_version.h"
#include "pwr_baseclasses.h"
#include "pwr_opcclasses.h"
#include "co_cdh.h"
#include "co_time.h"
#include "rt_gdh.h"
#include "opc_utl.h"
#include "opc_soap_H.h"
#include "Service.nsmap"

typedef struct {
  int address;
  int access;
} opc_sClientAccess;

class opcsrv_sub {
 public:
  void *p;
  int size;
  opc_eDataType opc_type;
  pwr_eType pwr_type;
  pwr_tSubid subid;
  float deadband;
  std::string client_handle;
  pwr_tInt64 old_value;
};
  
class opcsrv_client {
 public:
  int access;
  pwr_tTime m_last_time;
  map< std::string, vector<opcsrv_sub> > m_sublist;
  bool m_multi_threaded;
};

typedef map< std::string, vector<opcsrv_sub> >::iterator sublist_iterator;
typedef map< int, opcsrv_client>::iterator client_iterator;

class opc_server {
 public:
  opc_server() : m_client_access_cnt(0), m_grant_all(true), m_client(0) {}

  map< int, opcsrv_client> m_clientlist;
  pwr_tTime m_start_time;
  pwr_sClass_Opc_ServerConfig *m_config;
  opc_sClientAccess m_client_access[20];
  int m_client_access_cnt;
  int m_current_access;
  bool m_grant_all;
  opcsrv_client *m_client;

  opcsrv_client *find_client( int sid);
  opcsrv_client *new_client( int sid);
  int fault( struct soap *soap, int code);
  int get_access( int sid);

};

static opc_server *opcsrv;

static void *opcsrv_cyclic( void *arg);
static void *opcsrv_process_request( void *soap);

static int
opcsrv_set_error(struct soap *soap, const char *faultcode, const char *faultsubcode, const char *faultstring, const char *faultdetail, int soaperror)
{ *soap_faultcode(soap) = faultcode;
  if (faultsubcode)
    *soap_faultsubcode(soap) = faultsubcode;
  *soap_faultstring(soap) = faultstring;
  if (faultdetail && *faultdetail)
  { register const char **s = soap_faultdetail(soap);
    if (s)
      *s = faultdetail;
  }
  return soap->error = soaperror;
}

static int
opcsrv_fault(struct soap *soap, const char *faultcode, const char *faultsubcode, const char *faultstring, const char *faultdetail)
{ char *r = NULL, *s = NULL, *t = NULL;
  if (faultsubcode)
    r = soap_strdup(soap, faultsubcode);
  if (faultstring)
    s = soap_strdup(soap, faultstring);
  if (faultdetail)
    t = soap_strdup(soap, faultdetail);
  return opcsrv_set_error(soap, faultcode, r, s, t, SOAP_FAULT);
}

int opc_server::fault( struct soap *soap, int code)
{    
  return opcsrv_fault( soap, opc_resultcode_to_string( code), 0, opc_resultcode_to_text( code), 0);
}

opcsrv_client *opc_server::find_client( int sid)
{
  client_iterator it = opcsrv->m_clientlist.find( sid);
  if ( it == opcsrv->m_clientlist.end())
    return 0;
  else {
    clock_gettime( CLOCK_REALTIME, &it->second.m_last_time);
    return &it->second;
  }
}

opcsrv_client *opc_server::new_client( int sid)
{
  opcsrv_client client;

  m_clientlist[sid] = client;
  m_clientlist[sid].access = get_access( sid);
  m_clientlist[sid].m_multi_threaded = false;
  clock_gettime( CLOCK_REALTIME, &m_clientlist[sid].m_last_time);

  fprintf( stderr, "New client IP=%d.%d.%d.%d\n",
	   (sid>>24)&0xFF,(sid>>16)&0xFF,(sid>>8)&0xFF,sid&0xFF);

  return &m_clientlist[sid];
}



int main()
{
  struct soap soap;
  int m,s;   // Master and slave sockets
  pwr_tStatus sts;
  pwr_tOid config_oid;

  sts = gdh_Init("opc_server");
  if ( EVEN(sts)) {
    exit(sts);
  }

  opcsrv = new opc_server();


  // Get OpcServerConfig object
  sts = gdh_GetClassList( pwr_cClass_Opc_ServerConfig, &config_oid);
  if ( EVEN(sts)) {
    // Not configured
    exit(sts);
  }

  sts = gdh_ObjidToPointer( config_oid, (void **)&opcsrv->m_config);
  if ( EVEN(sts)) {
    exit(sts);
  }

  for ( int i = 0; 
	i < (int)(sizeof(opcsrv->m_config->ClientAccess)/sizeof(opcsrv->m_config->ClientAccess[0])); 
	i++) {
    if ( strcmp( opcsrv->m_config->ClientAccess[i].Address, "") != 0) {
      opcsrv->m_client_access[opcsrv->m_client_access_cnt].address = 
	inet_network( opcsrv->m_config->ClientAccess[i].Address);
      if ( opcsrv->m_client_access[opcsrv->m_client_access_cnt].address != -1) {
	opcsrv->m_client_access[opcsrv->m_client_access_cnt].access = opcsrv->m_config->ClientAccess[i].Access;
	opcsrv->m_client_access_cnt++;
      }
    }      
  }

  // Create a cyclic tread
  pthread_t 	thread;
  sts = pthread_create( &thread, NULL, opcsrv_cyclic, NULL);

  clock_gettime( CLOCK_REALTIME, &opcsrv->m_start_time);

  soap_init( &soap);
  m = soap_bind( &soap, NULL, 18083, 100);
  if ( m < 0)
    soap_print_fault( &soap, stderr);
  else {
    fprintf( stderr, "Socket connection successfull: master socket = %d\n", m);

    for ( int i = 1;; i++) {
      s = soap_accept( &soap);
      if ( s < 0) {
	soap_print_fault( &soap, stderr);
	break;
      }

      fprintf( stderr, "%d: request from IP=%lu.%lu.%lu.%lu socket=%d\n", i,
	       (soap.ip>>24)&0xFF,(soap.ip>>16)&0xFF,(soap.ip>>8)&0xFF,soap.ip&0xFF, s);

      opcsrv->m_client = opcsrv->find_client( soap.ip);
      if ( !opcsrv->m_client)
	opcsrv->m_client = opcsrv->new_client( soap.ip);

      if ( !opcsrv->m_client->m_multi_threaded) {
	if ( soap_serve( &soap) != SOAP_OK)         // Process RPC request
	  soap_print_fault( &soap, stderr);
	soap_destroy( &soap);   // Clean up class instances
	soap_end( &soap);       // Clean up everything and close socket
      }
      else {
	// Create a thread for every request
	struct soap *tsoap;
	pthread_t tid;

	tsoap = soap_copy( &soap);
	if ( !tsoap)
	  break;
	pthread_create( &tid, NULL, opcsrv_process_request, (void *)tsoap);
      }
    }
  }

  soap_done( &soap);     // Close master socket and detach environment
  return SOAP_OK;
}

static void *opcsrv_process_request( void *soap)
{
  pthread_detach( pthread_self());
  soap_serve( (struct soap *) soap);
  soap_destroy( (struct soap *) soap);
  soap_end( (struct soap *) soap);
  soap_done( (struct soap *) soap);
  free( soap);
  return 0;
}

static void *opcsrv_cyclic( void *arg)
{
  pwr_tTime current_time;
  pwr_tDeltaTime diff;

  for (;;) {

    clock_gettime( CLOCK_REALTIME, &current_time);

    // Check if any client can be removed    
    for ( client_iterator it = opcsrv->m_clientlist.begin(); it != opcsrv->m_clientlist.end(); it++) {
      time_Adiff( &diff, &current_time, &it->second.m_last_time);
      if ( diff.tv_sec > 600) {
	fprintf( stderr, "Client erased IP=%d.%d.%d.%d\n",
		 (it->first>>24)&0xFF,(it->first>>16)&0xFF,(it->first>>8)&0xFF,it->first&0xFF);
	opcsrv->m_clientlist.erase( it);
      }
    }

    sleep(1);
  }
}


SOAP_FMAC5 int SOAP_FMAC6 __s0__GetStatus(struct soap *soap, 
					   _s0__GetStatus *s0__GetStatus, 
					   _s0__GetStatusResponse *s0__GetStatusResponse)
{
  pwr_tTime current_time;

  // Check access for connection
  opcsrv_client *client = opcsrv->find_client( soap->ip);
  if ( !client)
    client = opcsrv->new_client( soap->ip);
  opcsrv->m_current_access = client->access;

  switch ( opcsrv->m_current_access) {
  case pwr_eOpc_AccessEnum_None:
    return opcsrv->fault( soap, opc_eResultCode_E_ACCESS_DENIED);
  default: ;
  }

  clock_gettime( CLOCK_REALTIME, &current_time);

  s0__GetStatusResponse->GetStatusResult = new s0__ReplyBase();
  s0__GetStatusResponse->GetStatusResult->RcvTime = opc_datetime(0);
  s0__GetStatusResponse->GetStatusResult->ReplyTime = opc_datetime(0);
  s0__GetStatusResponse->GetStatusResult->RevisedLocaleID = new std::string( "en");
  s0__GetStatusResponse->GetStatusResult->ServerState = s0__serverState__running;
  if ( s0__GetStatus->ClientRequestHandle)
    s0__GetStatusResponse->GetStatusResult->ClientRequestHandle = 
      new std::string( *s0__GetStatus->ClientRequestHandle);

  s0__GetStatusResponse->Status = new s0__ServerStatus();
  s0__GetStatusResponse->Status->VendorInfo = new std::string("Proview - Open Source Process Control");
  s0__GetStatusResponse->Status->SupportedInterfaceVersions.push_back( s0__interfaceVersion__XML_USCOREDA_USCOREVersion_USCORE1_USCORE0);
  s0__GetStatusResponse->Status->SupportedLocaleIDs.push_back( std::string("en"));
  s0__GetStatusResponse->Status->SupportedLocaleIDs.push_back( std::string("en-US"));
  s0__GetStatusResponse->Status->StartTime = opc_datetime( &opcsrv->m_start_time);
  s0__GetStatusResponse->Status->ProductVersion = new std::string( pwrv_cPwrVersionStr);

  return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6 __s0__Read(struct soap *soap, 
				      _s0__Read *s0__Read, 
				      _s0__ReadResponse *s0__ReadResponse)
{
  int ii;
  class s0__ReadRequestItem *item;
  pwr_sAttrRef               ar;
  pwr_tStatus sts;
  pwr_tOName itemname;
  pwr_tOName itempath;
  pwr_tOName path;
  int reqType = -1;
  pwr_tTypeId tid;
  unsigned int elem;
  char  buf[1024];
  unsigned int options = 0;
  
  // Check access for connection
  opcsrv_client *client = opcsrv->find_client( soap->ip);
  if ( !client)
    client = opcsrv->new_client( soap->ip);
  opcsrv->m_current_access = client->access;

  switch ( opcsrv->m_current_access) {
  case pwr_eOpc_AccessEnum_ReadOnly:
  case pwr_eOpc_AccessEnum_ReadWrite:
    break;
  default:
    return opcsrv->fault( soap, opc_eResultCode_E_ACCESS_DENIED);
  }

  s0__ReadResponse->ReadResult = new s0__ReplyBase();
  s0__ReadResponse->ReadResult->RcvTime = opc_datetime(0);
  s0__ReadResponse->ReadResult->ReplyTime = opc_datetime(0);
  if (s0__Read->Options && s0__Read->Options->ClientRequestHandle)
    s0__ReadResponse->ReadResult->ClientRequestHandle = new std::string(*s0__Read->Options->ClientRequestHandle);
  s0__ReadResponse->ReadResult->ServerState = s0__serverState__running;

  if (!s0__Read->ItemList)
    return SOAP_OK;

  if (s0__Read->ItemList->Items.empty())
    return SOAP_OK;
  
  memset(path, 0, sizeof(path));
  
  opc_requestoptions_to_mask(s0__Read->Options, &options);

  if (s0__Read->ItemList->ItemPath && !s0__Read->ItemList->ItemPath->empty())
    strncpy(path, s0__Read->ItemList->ItemPath->c_str(), sizeof(path));
  
  for (ii = 0; ii < (int) s0__Read->ItemList->Items.size(); ii++) {

    s0__ItemValue *iv = new s0__ItemValue();
      
    if (!s0__ReadResponse->RItemList)
      s0__ReadResponse->RItemList = new s0__ReplyItemList();

    item = s0__Read->ItemList->Items[ii];
    
    if (item->ItemPath && !item->ItemPath->empty())
      strncpy(itempath, item->ItemPath->c_str(), sizeof(itempath));
    else
      strncpy(itempath, path, sizeof(itempath));

    strncpy(itemname, itempath, sizeof(itemname));
    strncat(itemname, item->ItemName->c_str(), sizeof(itemname));

    if (options & opc_mRequestOption_ReturnItemPath)
      iv->ItemPath = new std::string(itempath);

    if (options & opc_mRequestOption_ReturnItemName)
      iv->ItemName = new std::string(itemname);

    if (options & opc_mRequestOption_ReturnDiagnosticInfo)
      iv->DiagnosticInfo = new std::string(""); // ToDo !!

    if ( s0__Read->ItemList->Items[ii]->ClientItemHandle)
      iv->ClientItemHandle =
	new std::string(*s0__Read->ItemList->Items[ii]->ClientItemHandle);

    sts = gdh_NameToAttrref(pwr_cNObjid, itemname, &ar);
    
    if (EVEN(sts)) {
      opcsrv_returnerror(s0__ReadResponse->Errors, &iv->ResultID, opc_eResultCode_E_INVALIDITEMNAME, options);
      s0__ReadResponse->RItemList->Items.push_back(iv);
      continue;
    }
    
    if (!item->ReqType || item->ReqType->empty()) {
      if (!s0__Read->ItemList->ReqType || s0__Read->ItemList->ReqType->empty()) {
        reqType = -1;
      } else {
        opc_string_to_opctype(s0__Read->ItemList->ReqType->c_str(), &reqType);
      }
    } else {
      opc_string_to_opctype(item->ReqType->c_str(), &reqType);
    }
    
    gdh_GetAttributeCharAttrref(&ar, &tid, NULL, NULL, &elem);
    
    if (cdh_tidIsCid(tid) || elem > 1) {
      opcsrv_returnerror(s0__ReadResponse->Errors, &iv->ResultID, opc_eResultCode_E_BADTYPE, options);
      s0__ReadResponse->RItemList->Items.push_back(iv);
      continue;
    }
          
    sts = gdh_GetObjectInfoAttrref(&ar, buf, sizeof(buf));  

    if ( ODD(sts)) {
        
      if (reqType < 0) opc_pwrtype_to_opctype(tid, &reqType);
      
      if (opc_convert_pwrtype_to_opctype(buf, 0, sizeof(buf), reqType, tid)) {
      	iv->Value = opc_opctype_to_value(buf, sizeof(buf), reqType);
	if (options & opc_mRequestOption_ReturnItemTime) {
	  // ToDo !!!
	}
        s0__ReadResponse->RItemList->Items.push_back(iv);

      } else {
        opcsrv_returnerror(s0__ReadResponse->Errors, &iv->ResultID, opc_eResultCode_E_BADTYPE, options);
        s0__ReadResponse->RItemList->Items.push_back(iv);
        continue;
      }
    } else {
      opcsrv_returnerror(s0__ReadResponse->Errors, &iv->ResultID, opc_eResultCode_E_BADTYPE, options);
      s0__ReadResponse->RItemList->Items.push_back(iv);
      continue;
    }
  }
  return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6 __s0__Write(struct soap* soap, 
				      _s0__Write *s0__Write, 
				      _s0__WriteResponse *s0__WriteResponse)
{
  int ii;
  s0__ItemValue *item;
  pwr_sAttrRef               ar;
  pwr_tStatus sts;
  pwr_tOName itemname;
  pwr_tOName itempath;
  pwr_tOName path;
  pwr_tTypeId a_type;
  unsigned int a_size, a_elem;
  char  buf[1024];
  unsigned int options = 0;
  
  // Check access for connection
  opcsrv_client *client = opcsrv->find_client( soap->ip);
  if ( !client)
    client = opcsrv->new_client( soap->ip);
  opcsrv->m_current_access = client->access;

  switch ( opcsrv->m_current_access) {
  case pwr_eOpc_AccessEnum_ReadWrite:
    break;
  default:
    return opcsrv->fault( soap, opc_eResultCode_E_ACCESS_DENIED);
  }

  s0__WriteResponse->WriteResult = new s0__ReplyBase();
  s0__WriteResponse->WriteResult->RcvTime = opc_datetime(0);
  s0__WriteResponse->WriteResult->ReplyTime = opc_datetime(0);
  if (s0__Write->Options && s0__Write->Options->ClientRequestHandle)
    s0__WriteResponse->WriteResult->ClientRequestHandle = new std::string(*s0__Write->Options->ClientRequestHandle);
  s0__WriteResponse->WriteResult->ServerState = s0__serverState__running;

  if (!s0__Write->ItemList)
    return SOAP_OK;

  if (s0__Write->ItemList->Items.empty())
    return SOAP_OK;
  
  strcpy(path, "");
  
  opc_requestoptions_to_mask(s0__Write->Options, &options);

  if (s0__Write->ItemList->ItemPath && !s0__Write->ItemList->ItemPath->empty())
    strncpy(path, s0__Write->ItemList->ItemPath->c_str(), sizeof(path));
  
  for (ii = 0; ii < (int) s0__Write->ItemList->Items.size(); ii++) {

    s0__ItemValue *iv = new s0__ItemValue();
      
    if (!s0__WriteResponse->RItemList)
      s0__WriteResponse->RItemList = new s0__ReplyItemList();

    item = s0__Write->ItemList->Items[ii];
    
    if (item->ItemPath && !item->ItemPath->empty())
      strncpy(itempath, item->ItemPath->c_str(), sizeof(itempath));
    else
      strncpy(itempath, path, sizeof(itempath));

    strncpy(itemname, itempath, sizeof(itemname));
    strncat(itemname, item->ItemName->c_str(), sizeof(itemname));

    if (options & opc_mRequestOption_ReturnItemPath)
      iv->ItemPath = new std::string(itempath);

    if (options & opc_mRequestOption_ReturnItemName)
      iv->ItemName = new std::string(itemname);

    if (options & opc_mRequestOption_ReturnDiagnosticInfo)
      iv->DiagnosticInfo = new std::string(""); // ToDo !!

    sts = gdh_NameToAttrref(pwr_cNObjid, itemname, &ar);    
    if (EVEN(sts)) {
      opcsrv_returnerror(s0__WriteResponse->Errors, &iv->ResultID, opc_eResultCode_E_INVALIDITEMNAME, options);
      s0__WriteResponse->RItemList->Items.push_back(iv);
      continue;
    }
    
    gdh_GetAttributeCharAttrref(&ar, &a_type, &a_size, NULL, &a_elem);
    
    if (cdh_tidIsCid(a_type) || a_elem > 1) {
      opcsrv_returnerror(s0__WriteResponse->Errors, &iv->ResultID, opc_eResultCode_E_BADTYPE, options);
      s0__WriteResponse->RItemList->Items.push_back(iv);
      continue;
    }          

    if ( !opc_convert_opctype_to_pwrtype( buf, a_size, item->Value, (pwr_eType) a_type)) {
      opcsrv_returnerror(s0__WriteResponse->Errors, &iv->ResultID, opc_eResultCode_E_BADTYPE, options);
      s0__WriteResponse->RItemList->Items.push_back(iv);
      continue;
    }

    sts = gdh_SetObjectInfoAttrref(&ar, buf, a_size);
    if ( EVEN(sts)) {
      opcsrv_returnerror(s0__WriteResponse->Errors, &iv->ResultID, opc_eResultCode_E_BADTYPE, options);
      s0__WriteResponse->RItemList->Items.push_back(iv);
    }
  }
  return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6 __s0__Subscribe(struct soap* soap, 
					   _s0__Subscribe *s0__Subscribe, 
					   _s0__SubscribeResponse *s0__SubscribeResponse)
{
  pwr_tStatus sts;
  pwr_tTypeId a_tid;
  pwr_tUInt32 a_size, a_offs, a_elem;
  pwr_tAName aname;
  float list_deadband = 0;
  float deadband;

  // Check access for connection
  opcsrv_client *client = opcsrv->find_client( soap->ip);
  if ( !client)
    client = opcsrv->new_client( soap->ip);
  opcsrv->m_current_access = client->access;

  s0__SubscribeResponse->SubscribeResult = new s0__ReplyBase();
  s0__SubscribeResponse->SubscribeResult->RcvTime = opc_datetime(0);
  s0__SubscribeResponse->SubscribeResult->ReplyTime = opc_datetime(0);
  s0__SubscribeResponse->SubscribeResult->ServerState = s0__serverState__running;

  if ( s0__Subscribe->Options && s0__Subscribe->Options->ClientRequestHandle)
    s0__SubscribeResponse->SubscribeResult->ClientRequestHandle = 
      new std::string( *s0__Subscribe->Options->ClientRequestHandle);

  switch ( opcsrv->m_current_access) {
  case pwr_eOpc_AccessEnum_ReadOnly:
  case pwr_eOpc_AccessEnum_ReadWrite:
    break;
  default:
    return opcsrv->fault( soap, opc_eResultCode_E_ACCESS_DENIED);
  }

  if ( s0__Subscribe->ItemList) {

    if ( s0__Subscribe->ItemList->Deadband)
      list_deadband = *s0__Subscribe->ItemList->Deadband;

    for ( int i = 0; i < (int) s0__Subscribe->ItemList->Items.size(); i++) {
      opcsrv_sub sub;
      int resultid = 0;

      while (1) {
      
	strcpy( aname, s0__Subscribe->ItemList->Items[i]->ItemName->c_str());
	sts = gdh_GetAttributeCharacteristics( aname, &a_tid, &a_size, &a_offs, &a_elem);
	if ( EVEN(sts)) {
	  resultid = opc_eResultCode_E_UNKNOWNITEMNAME;
	  break;
	}

	memset( &sub.old_value, 0, sizeof(sub.old_value));
	sub.size = a_size;
	sub.pwr_type = (pwr_eType) a_tid;
	if ( cdh_tidIsCid( a_tid)) {
	  resultid = opc_eResultCode_E_BADTYPE;
	  break;
	}

	if ( !opc_pwrtype_to_opctype( sub.pwr_type, (int *)&sub.opc_type)) {
	  resultid = opc_eResultCode_E_BADTYPE;
	  break;
	}

	sts = gdh_RefObjectInfo( aname, &sub.p, &sub.subid, sub.size);
	if ( EVEN(sts)) {
	  resultid = opc_eResultCode_E_FAIL;
	  break;
	}

	deadband = list_deadband;
	if ( s0__Subscribe->ItemList->Items[i]->Deadband)
	  deadband = *s0__Subscribe->ItemList->Items[i]->Deadband;

	sub.deadband = 0;
	if ( deadband != 0) {
	  // Deadband in percentage of range, get range
	  pwr_tAName oname;
	  pwr_tAttrRef oaref;
	  pwr_tCid cid;
	  char *s;

	  strcpy( oname, aname);
	  if ( (s = strrchr( oname, '.')))
	    *s = 0;

	  sts = gdh_NameToAttrref( pwr_cNOid, oname, &oaref);
	  if ( EVEN(sts)) break;
	  
	  sts = gdh_GetAttrRefTid( &oaref, &cid);
	  if ( EVEN(sts)) break;
	   
	  switch ( cid) {
	  case pwr_cClass_Ai:
	  case pwr_cClass_Ao:
	  case pwr_cClass_Ii:
	  case pwr_cClass_Io: {
	    // Get range from channel
	    pwr_tAttrRef aaref;
	    pwr_tAttrRef sigchancon;
	    pwr_tFloat32 range_high = 0;
	    pwr_tFloat32 range_low = 0;
	    pwr_tStatus sts;

	    // Get Range from channel
	    sts = gdh_ArefANameToAref( &oaref, "SigChanCon", &aaref);
	  
	    if ( ODD(sts))
	      sts = gdh_GetObjectInfoAttrref( &aaref, &sigchancon, sizeof(sigchancon));

	    if ( ODD(sts))
	      if ( cdh_ObjidIsNull( sigchancon.Objid))
		sts = 0;

	    if ( ODD(sts))
	      sts = gdh_ArefANameToAref( &sigchancon, "ActValRangeHigh", &aaref);

	    if ( ODD(sts))
	      sts = gdh_GetObjectInfoAttrref( &aaref, &range_high, sizeof(range_high));

	    if ( ODD(sts))
	      sts = gdh_ArefANameToAref( &sigchancon, "ActValRangeLow", &aaref);

	    if ( ODD(sts))
	      sts = gdh_GetObjectInfoAttrref( &aaref, &range_low, sizeof(range_low));
	    
	    if ( ODD(sts) && !(range_high == 0 && range_low == 0)) {
	      sub.deadband = (range_high - range_low) * deadband / 100;
	      break;
	    }
	    // Else continue and use PresMaxLimit and PresMinLimit
	  }
	  case pwr_cClass_Av:
	  case pwr_cClass_Iv: {
	    // Get range from Min/MaxShow
	    pwr_tAttrRef aaref;
	    pwr_tFloat32 range_high = 0;
	    pwr_tFloat32 range_low = 0;
	    pwr_tStatus sts;

	    sts = gdh_ArefANameToAref( &oaref, "PresMaxLimit", &aaref);

	    if ( ODD(sts))
	      sts = gdh_GetObjectInfoAttrref( &aaref, &range_high, sizeof(range_high));

	    if ( ODD(sts))
	      sts = gdh_ArefANameToAref( &oaref, "PresMinLimit", &aaref);

	    if ( ODD(sts))
	      sts = gdh_GetObjectInfoAttrref( &aaref, &range_low, sizeof(range_low));

	    if ( ODD(sts) && !(range_high == 0 && range_low == 0))
	      sub.deadband = (range_high - range_low) * deadband / 100;

	    break;
	  }
	  default:
	    // No range exist
	    ;
	  }
	}

	if ( s0__Subscribe->ItemList->Items[i]->ClientItemHandle)
	  sub.client_handle = *s0__Subscribe->ItemList->Items[i]->ClientItemHandle;
	else
	  sub.client_handle = std::string("");

	if ( !s0__SubscribeResponse->ServerSubHandle) {
	  vector<opcsrv_sub> subv;
	
	  s0__SubscribeResponse->ServerSubHandle = 
	    new std::string( cdh_SubidToString( 0, sub.subid, 0));
	
	  client->m_sublist[*s0__SubscribeResponse->ServerSubHandle] = subv;
	}
	client->m_sublist[*s0__SubscribeResponse->ServerSubHandle].push_back( sub);
	break;
      }
      if ( resultid || s0__Subscribe->ReturnValuesOnReply) {
	if ( !s0__SubscribeResponse->RItemList)
	  s0__SubscribeResponse->RItemList = new s0__SubscribeReplyItemList();

	s0__SubscribeItemValue *iv = new s0__SubscribeItemValue();
	iv->ItemValue = new s0__ItemValue();

	iv->ItemValue->ItemName = new std::string( *s0__Subscribe->ItemList->Items[i]->ItemName);
	if ( s0__Subscribe->ItemList->Items[i]->ClientItemHandle)
	  iv->ItemValue->ClientItemHandle =
	    new std::string(*s0__Subscribe->ItemList->Items[i]->ClientItemHandle);
	if ( resultid) {
	  opcsrv_returnerror( s0__SubscribeResponse->Errors, 
			      &iv->ItemValue->ResultID, resultid, 0);
	}
	else if ( s0__Subscribe->ReturnValuesOnReply) {
	  int reqType;
	  char  buf[1024];

	  opc_pwrtype_to_opctype( a_tid, &reqType);
	  if (opc_convert_pwrtype_to_opctype( sub.p, buf, sizeof(buf), reqType, a_tid)) {
	    iv->ItemValue->Value = opc_opctype_to_value( buf, sizeof(buf), reqType);
	  }
	  else
	    opcsrv_returnerror( s0__SubscribeResponse->Errors, 
				&iv->ItemValue->ResultID, opc_eResultCode_E_BADTYPE, 0);
	}
	s0__SubscribeResponse->RItemList->Items.push_back( iv);
      }
    }
  }
  return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6 __s0__SubscriptionPolledRefresh(struct soap* soap, 
							   _s0__SubscriptionPolledRefresh *s0__SubscriptionPolledRefresh, 
							   _s0__SubscriptionPolledRefreshResponse *s0__SubscriptionPolledRefreshResponse)
{
  // Check access for the connection
  opcsrv_client *client = opcsrv->find_client( soap->ip);
  if ( !client)
    client = opcsrv->new_client( soap->ip);
  opcsrv->m_current_access = client->access;

  switch ( opcsrv->m_current_access) {
  case pwr_eOpc_AccessEnum_ReadOnly:
  case pwr_eOpc_AccessEnum_ReadWrite:
    break;
  default:
    return opcsrv->fault( soap, opc_eResultCode_E_ACCESS_DENIED);
  }

  s0__SubscriptionPolledRefreshResponse->SubscriptionPolledRefreshResult = 
    new s0__ReplyBase();
  s0__SubscriptionPolledRefreshResponse->SubscriptionPolledRefreshResult->RcvTime = 
    opc_datetime(0);
  s0__SubscriptionPolledRefreshResponse->SubscriptionPolledRefreshResult->ReplyTime = 
    opc_datetime(0);
  s0__SubscriptionPolledRefreshResponse->SubscriptionPolledRefreshResult->ServerState = 
    s0__serverState__running;

  if ( s0__SubscriptionPolledRefresh->Options && 
       s0__SubscriptionPolledRefresh->Options->ClientRequestHandle)
    s0__SubscriptionPolledRefreshResponse->SubscriptionPolledRefreshResult->ClientRequestHandle = 
      new std::string( *s0__SubscriptionPolledRefresh->Options->ClientRequestHandle);

  pwr_tTime hold_time;
  int wait_time;
  bool has_holdtime = false;
  bool has_waittime = false;
  int waited_time = 0;
  pwr_tFloat32 wait_scan = 0.5;
  pwr_tDeltaTime dwait_scan;

  if ( s0__SubscriptionPolledRefresh->HoldTime) {
    if ( !client->m_multi_threaded)
      client->m_multi_threaded = true;
    else {
      has_holdtime = true;
      opc_time_OPCAsciiToA( (char *)s0__SubscriptionPolledRefresh->HoldTime->c_str(), 
			    &hold_time);
      if ( s0__SubscriptionPolledRefresh->WaitTime) {
	has_waittime = true;
	wait_time = *s0__SubscriptionPolledRefresh->WaitTime;
	time_FloatToD( &dwait_scan, wait_scan);
      }
    }
  }

  if ( has_holdtime) {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    pwr_tTime current_time;
    char str1[80], str2[80];

    clock_gettime( CLOCK_REALTIME, &current_time);
    time_AtoAscii( &current_time, time_eFormat_DateAndTime, str1, sizeof(str1));
    time_AtoAscii( &hold_time, time_eFormat_DateAndTime, str2, sizeof(str2));
		   
    printf( "TimedWait %s  %s\n", str1, str2);

    pthread_cond_timedwait( &cond, &mutex, &hold_time);
  }

  if ( has_waittime) {
    // Check all values
    bool change_found = false;

    for ( waited_time = 0; waited_time < wait_time; waited_time += (int)(wait_scan * 1000)) {
      for ( int i = 0; i < (int) s0__SubscriptionPolledRefresh->ServerSubHandles.size(); i++) {
	sublist_iterator it = 
	  client->m_sublist.find( s0__SubscriptionPolledRefresh->ServerSubHandles[i]);
      
	if ( it != client->m_sublist.end()) {
	  for ( int j = 0; j < (int)it->second.size(); j++) {
	    if ( !opc_cmp_pwr( &it->second[j].old_value, it->second[j].p, it->second[j].size,
			      it->second[j].pwr_type, it->second[j].deadband)) {
	      change_found = true;
	      break;
	    }
	  }
	}
	if ( change_found)
	  break;
      }
      
      if ( change_found)
	break;

      pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
      pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
      pwr_tTime current_time, next_time;
      
      clock_gettime( CLOCK_REALTIME, &current_time);
      time_Aadd( &next_time, &current_time, &dwait_scan);
      pthread_cond_timedwait( &cond, &mutex, &next_time);
    }
  }

  for ( int i = 0; i < (int) s0__SubscriptionPolledRefresh->ServerSubHandles.size(); i++) {
    sublist_iterator it = 
      client->m_sublist.find( s0__SubscriptionPolledRefresh->ServerSubHandles[i]);

    if ( it != client->m_sublist.end()) {

      // Test
      for ( int jj = 0; jj < (int) it->second.size(); jj++) {
	printf( "%d sub: p %d size %d opc_type %d pwr_type %d subid %d,%d\n", jj, (int)it->second[jj].p,
		it->second[jj].size, it->second[jj].opc_type, it->second[jj].pwr_type, it->second[jj].subid.nid,
		it->second[jj].subid.rix);
	jj++;
      }


      s0__SubscribePolledRefreshReplyItemList *rlist = new s0__SubscribePolledRefreshReplyItemList();

      rlist->SubscriptionHandle = new std::string(s0__SubscriptionPolledRefresh->ServerSubHandles[i]);
      for ( int j = 0; j < (int)it->second.size(); j++) {
	s0__ItemValue *ritem = new s0__ItemValue();
	
	// TODO

	ritem->Value = opc_opctype_to_value( it->second[j].p, it->second[j].size, 
					     it->second[j].opc_type);
	memcpy( &it->second[j].old_value, it->second[j].p, it->second[j].size);
	ritem->Timestamp = new std::string( opc_datetime(0));
	if ( !it->second[j].client_handle.empty())
	  ritem->ClientItemHandle = new std::string( it->second[j].client_handle);

	rlist->Items.push_back( ritem);
      }
      s0__SubscriptionPolledRefreshResponse->RItemList.push_back( rlist);
    }
    else {
      // Subscription not found
      s0__SubscribePolledRefreshReplyItemList *rlist = new s0__SubscribePolledRefreshReplyItemList();

      rlist->SubscriptionHandle = new std::string(s0__SubscriptionPolledRefresh->ServerSubHandles[i]);

      s0__ItemValue *ritem = new s0__ItemValue();
      opcsrv_returnerror( s0__SubscriptionPolledRefreshResponse->Errors, &ritem->ResultID, 
			  opc_eResultCode_E_NOSUBSCRIPTION, 0);
      rlist->Items.push_back( ritem);
      s0__SubscriptionPolledRefreshResponse->RItemList.push_back( rlist);
    }
  }

  return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6 __s0__SubscriptionCancel(struct soap* soap, 
						    _s0__SubscriptionCancel *s0__SubscriptionCancel, 
						    _s0__SubscriptionCancelResponse *s0__SubscriptionCancelResponse)
{
  opcsrv_client *client = opcsrv->find_client( soap->ip);
  if ( !client)
    return opcsrv->fault( soap, opc_eResultCode_E_FAIL);

  // Check access for the connection
  switch ( client->access) {
  case pwr_eOpc_AccessEnum_ReadOnly:
  case pwr_eOpc_AccessEnum_ReadWrite:
    break;
  default:
    return opcsrv->fault( soap, opc_eResultCode_E_ACCESS_DENIED);
  }

  if ( !s0__SubscriptionCancel->ServerSubHandle)
    return opcsrv->fault( soap, opc_eResultCode_E_FAIL);


  clock_gettime( CLOCK_REALTIME, &client->m_last_time);

  sublist_iterator it = 
    client->m_sublist.find( *s0__SubscriptionCancel->ServerSubHandle);

  if ( it != client->m_sublist.end()) {
    client->m_sublist.erase( it);
  }
  else {
    return opcsrv->fault( soap, opc_eResultCode_E_FAIL);
  }

  if ( s0__SubscriptionCancel->ClientRequestHandle)
    s0__SubscriptionCancelResponse->ClientRequestHandle = 
      new std::string( *s0__SubscriptionCancel->ClientRequestHandle);
  return SOAP_OK;
}

bool opcsrv_get_properties( bool is_item, pwr_tCid pcid, pwr_tAttrRef *parp, 
			    pwr_tAttrRef *arp, unsigned int propmask, gdh_sAttrDef *bd,
			    std::vector<s0__ItemProperty *>& properties)
{
  pwr_tStatus sts;

  if ( !is_item) {
    if ( propmask & opc_mProperty_Description) {
      pwr_tAttrRef aaref;
      pwr_tString80 desc;

      sts = gdh_ArefANameToAref( arp, "Description", &aaref);
      if ( ODD(sts)) {
	sts = gdh_GetObjectInfoAttrref( &aaref, desc, sizeof(desc));
	if ( ODD(sts)) {
	  s0__ItemProperty *ip = new s0__ItemProperty();
	  ip->Name = std::string("description");
	  ip->Value = new xsd__string();
	  ((xsd__string *)ip->Value)->__item = std::string(desc);
	  properties.push_back( ip);
	}
      }
      if ( EVEN(sts)) {
	s0__ItemProperty *ip = new s0__ItemProperty();
	ip->Name = std::string("description");
	ip->Value = new xsd__string();
	properties.push_back( ip);
      }
    }
  }
  else {
    // IsItem

    // Description
    if ( propmask & opc_mProperty_Description) {
      
      switch ( pcid) {
      case pwr_cClass_Di:
      case pwr_cClass_Do:
      case pwr_cClass_Dv:
      case pwr_cClass_Ai:
      case pwr_cClass_Ao:
      case pwr_cClass_Av:
      case pwr_cClass_Ii:
      case pwr_cClass_Io:
      case pwr_cClass_Iv: {
	if ( strcmp( bd->attrName, "ActualValue") != 0)
	  break;

	pwr_tAttrRef aaref;
	pwr_tString80 desc;
	
	// Description from signal object
	sts = gdh_ArefANameToAref( parp, "Description", &aaref);
	if ( EVEN(sts)) break;
	
	sts = gdh_GetObjectInfoAttrref( &aaref, desc, sizeof(desc));
	if ( EVEN(sts)) break;
	
	s0__ItemProperty *ip = new s0__ItemProperty();
	ip->Name = std::string("description");
	ip->Value = new xsd__string();
	((xsd__string *)ip->Value)->__item = std::string( desc);
	properties.push_back( ip);
	break;
      }
      default: {
	s0__ItemProperty *ip = new s0__ItemProperty();
	ip->Name = std::string("description");
	ip->Value = new xsd__string();
	properties.push_back( ip);
      }
      }

    }

    // DataType
    if ( propmask & opc_mProperty_DataType) {
      char *type_p;
      s0__ItemProperty *ip = new s0__ItemProperty();

      if ( opc_pwrtype_to_string( bd->attr->Param.Info.Type, &type_p)) {
	ip->Name = std::string("dataType");
	ip->Value = new xsd__QName_();
	((xsd__QName_ *)ip->Value)->__item = std::string(type_p);
	properties.push_back( ip);
      }
      else {
	// Untranslatable type TODO
      }     
    }

    // Quality
    if ( propmask & opc_mProperty_Quality) {
      char *qual_p;
      s0__ItemProperty *ip = new s0__ItemProperty();

      if ( opc_quality_to_string( s0__qualityBits__good, &qual_p)) {
	ip->Name = std::string("quality");
	ip->Value = new xsd__string();
	((xsd__string *)ip->Value)->__item = std::string(qual_p);
	properties.push_back( ip);
      }
    }

    // Timestamp
    if ( propmask & opc_mProperty_Timestamp) {
      // TODO ...
    }

    // Access Rights
    if ( propmask & opc_mProperty_AccessRights) {
      s0__ItemProperty *ip = new s0__ItemProperty();

      ip->Name = std::string("accessRights");
      ip->Value = new xsd__string();

      switch ( opcsrv->m_current_access) {
      case pwr_eOpc_AccessEnum_ReadOnly:
	((xsd__string *)ip->Value)->__item = std::string("readable");
	break;
      case pwr_eOpc_AccessEnum_ReadWrite:
	if ( bd->attr->Param.Info.Flags & PWR_MASK_RTVIRTUAL ||
	     bd->attr->Param.Info.Flags & PWR_MASK_PRIVATE)
	  ((xsd__string *)ip->Value)->__item = std::string("readable");
	else
	  ((xsd__string *)ip->Value)->__item = std::string("readWritable");
	break;
      default:
	((xsd__string *)ip->Value)->__item = std::string("unknown");
	break;
      }
      properties.push_back( ip);
    }

    // EngineeringUnits
    if ( propmask & opc_mProperty_EngineeringUnits) {
      if ( parp) {

	switch ( pcid) {
	case pwr_cClass_Ai:
	case pwr_cClass_Ao:
	case pwr_cClass_Av: {
	  if ( strcmp( bd->attrName, "ActualValue") != 0)
	    break;

	  pwr_tAttrRef aaref;
	  pwr_tString16 unit;

	  // Get Range from channel
	  sts = gdh_ArefANameToAref( parp, "Unit", &aaref);
	  if ( EVEN(sts)) break;
	  
	  sts = gdh_GetObjectInfoAttrref( &aaref, &unit, sizeof(unit));
	  if ( EVEN(sts)) break;

	  s0__ItemProperty *ip = new s0__ItemProperty();
	  ip->Name = std::string("engineeringUnits");
	  ip->Value = new xsd__string();
	  ((xsd__string *)ip->Value)->__item = std::string(unit);
	  properties.push_back( ip);
	  break;
	}
	default: ;
 	}
      }
    }

    // EuType
    if ( propmask & opc_mProperty_EuType) {
      switch( bd->attr->Param.Info.Type) {
      case pwr_eType_Float32: {
	s0__ItemProperty *ip = new s0__ItemProperty();
	ip->Name = std::string("euType");
	ip->Value = new xsd__string();
	((xsd__string *)ip->Value)->__item = std::string("analog");
	properties.push_back( ip);
	break;
      }
      default: {
	s0__ItemProperty *ip = new s0__ItemProperty();
	ip->Name = std::string("euType");
	ip->Value = new xsd__string();
	((xsd__string *)ip->Value)->__item = std::string("noEnum");
	properties.push_back( ip);
	break;
      }
      }
    }
    
    // HighEU
    if ( propmask & opc_mProperty_HighEU) {
      if ( parp) {
	pwr_tAttrRef aaref;
	pwr_tFloat32 fval;

	switch ( pcid) {
	case pwr_cClass_Av:
	case pwr_cClass_Ai:
	case pwr_cClass_Ao:
	  sts = gdh_ArefANameToAref( parp, "PresMaxLimit", &aaref);
	  if ( ODD(sts)) {
	    sts = gdh_GetObjectInfoAttrref( &aaref, &fval, sizeof(fval));
	    if ( ODD(sts)) {
	      s0__ItemProperty *ip = new s0__ItemProperty();
	      ip->Name = std::string("highEU");
	      ip->Value = new xsd__double();
	      ((xsd__double *)ip->Value)->__item = fval;
	      properties.push_back( ip);
	    }
	  }
	  break;
	default: ;
 	}
      }
    }

    // LowEU
    if ( propmask & opc_mProperty_LowEU) {
      if ( parp) {
	pwr_tAttrRef aaref;
	pwr_tFloat32 fval;

	switch ( pcid) {
	case pwr_cClass_Av:
	case pwr_cClass_Ai:
	case pwr_cClass_Ao:
	  sts = gdh_ArefANameToAref( parp, "PresMinLimit", &aaref);
	  if ( ODD(sts)) {
	    sts = gdh_GetObjectInfoAttrref( &aaref, &fval, sizeof(fval));
	    if ( ODD(sts)) {
	      s0__ItemProperty *ip = new s0__ItemProperty();
	      ip->Name = std::string("lowEU");
	      ip->Value = new xsd__double();
	      ((xsd__double *)ip->Value)->__item = fval;
	      properties.push_back( ip);
	    }
	  }
	  break;
	default: ;
 	}
      }
    }

    // HighIR
    if ( propmask & opc_mProperty_HighIR) {
      if ( parp) {

	switch ( pcid) {
	case pwr_cClass_Ai:
	case pwr_cClass_Ii:
	case pwr_cClass_Ao:
	case pwr_cClass_Io: {
	  if ( strcmp( bd->attrName, "ActualValue") != 0)
	    break;

	  pwr_tAttrRef aaref;
	  pwr_tAttrRef sigchancon;
	  pwr_tFloat32 fval;

	  // Get Range from channel
	  sts = gdh_ArefANameToAref( parp, "SigChanCon", &aaref);
	  if ( EVEN(sts)) break;
	  
	  sts = gdh_GetObjectInfoAttrref( &aaref, &sigchancon, sizeof(sigchancon));
	  if ( EVEN(sts)) break;

	  if ( cdh_ObjidIsNull( sigchancon.Objid))
	    break;

	  sts = gdh_ArefANameToAref( &sigchancon, "ActValRangeHigh", &aaref);
	  if ( EVEN(sts)) break;

	  sts = gdh_GetObjectInfoAttrref( &aaref, &fval, sizeof(fval));
	  if ( EVEN(sts)) break;

	  s0__ItemProperty *ip = new s0__ItemProperty();
	  ip->Name = std::string("highIR");
	  ip->Value = new xsd__double();
	  ((xsd__double *)ip->Value)->__item = fval;
	  properties.push_back( ip);
	  break;
	}
	default: ;
 	}
      }
    }

    // LowIR
    if ( propmask & opc_mProperty_LowIR) {
      if ( parp) {
	
	switch ( pcid) {
	case pwr_cClass_Ai:
	case pwr_cClass_Ii:
	case pwr_cClass_Ao:
	case pwr_cClass_Io: {
	  if ( strcmp( bd->attrName, "ActualValue") != 0)
	    break;

	  pwr_tAttrRef sigchancon;
	  pwr_tAttrRef aaref;
	  pwr_tFloat32 fval;

	  // Get Range from channel
	  sts = gdh_ArefANameToAref( parp, "SigChanCon", &aaref);
	  if ( EVEN(sts)) break;
	  
	  sts = gdh_GetObjectInfoAttrref( &aaref, &sigchancon, sizeof(sigchancon));
	  if ( EVEN(sts)) break;

	  if ( cdh_ObjidIsNull( sigchancon.Objid))
	    break;

	  sts = gdh_ArefANameToAref( &sigchancon, "ActValRangeLow", &aaref);
	  if ( EVEN(sts)) break;

	  sts = gdh_GetObjectInfoAttrref( &aaref, &fval, sizeof(fval));
	  if ( EVEN(sts)) break;

	  s0__ItemProperty *ip = new s0__ItemProperty();
	  ip->Name = std::string("lowIR");
	  ip->Value = new xsd__double();
	  ((xsd__double *)ip->Value)->__item = fval;
	  properties.push_back( ip);
	  break;
	}
	default: ;
 	}
      }
    }

  }
  return true;
}

int opc_server::get_access( int sid)
{
  int access = pwr_eOpc_AccessEnum_None;
  if ( m_grant_all)
    access = pwr_eOpc_AccessEnum_ReadWrite;
  else {
    for ( int i = 0; i < m_client_access_cnt; i++) {
      if ( m_client_access[i].address == (int)sid) {
	access = m_client_access[i].access;
	break;
      }
    }
  }
  return access;
}
  
SOAP_FMAC5 int SOAP_FMAC6 __s0__Browse(struct soap *soap, _s0__Browse *s0__Browse, 
					_s0__BrowseResponse *s0__BrowseResponse)
{
  pwr_tStatus sts;
  pwr_tOid oid, child, ch;
  pwr_tOName name;
  pwr_tCid cid;
  unsigned int property_mask;
  pwr_tTime current_time;
  bool has_max_elem = false;
  int max_elem = 0;

  // Check access for connection
  opcsrv_client *client = opcsrv->find_client( soap->ip);
  if ( !client)
    client = opcsrv->new_client( soap->ip);
  opcsrv->m_current_access = client->access;

  switch ( opcsrv->m_current_access) {
  case pwr_eOpc_AccessEnum_ReadOnly:
  case pwr_eOpc_AccessEnum_ReadWrite:
    break;
  default:
    return opcsrv->fault( soap, opc_eResultCode_E_ACCESS_DENIED);
  }

  clock_gettime( CLOCK_REALTIME, &current_time);

  if ( s0__Browse->MaxElementsReturned) {
    has_max_elem = true;
    max_elem = *s0__Browse->MaxElementsReturned;
  }
  s0__BrowseResponse->BrowseResult = new s0__ReplyBase();
  s0__BrowseResponse->BrowseResult->RcvTime = opc_datetime(0);
  s0__BrowseResponse->BrowseResult->ReplyTime = opc_datetime(0);
  if ( s0__Browse->ClientRequestHandle)
    s0__BrowseResponse->BrowseResult->ClientRequestHandle = 
      new std::string( *s0__Browse->ClientRequestHandle);
  s0__BrowseResponse->BrowseResult->ServerState = s0__serverState__running;
  s0__BrowseResponse->MoreElements = (bool *) malloc( sizeof(bool));
  *s0__BrowseResponse->MoreElements = false;


  if ( s0__Browse->ContinuationPoint) {
    // Continue with next siblings

    pwr_tOName pname;
    pwr_tOName itemname;
    pwr_sAttrRef paref;
    pwr_sAttrRef aref;

    cdh_StringToObjid( s0__Browse->ContinuationPoint->c_str(), &child);

    // Check continuationpoint
    sts = gdh_GetObjectClass( child, &cid);
    if ( EVEN(sts)) {
      return opcsrv->fault( soap, opc_eResultCode_E_INVALIDCONTINUATIONPOINT);
    }

    for ( sts = 1; 
	  ODD(sts); 
	  sts = gdh_GetNextSibling( child, &child)) {

      if ( has_max_elem && (int)s0__BrowseResponse->Elements.size() > max_elem) {
	// Max elements reached, return current oid as continuation point

	s0__BrowseResponse->ContinuationPoint = 
	  new std::string( cdh_ObjidToString( 0, child, 1));
	*s0__BrowseResponse->MoreElements = true;
	break;
      }

      sts = gdh_ObjidToName( child, name, sizeof(name), cdh_mName_object);
      if ( EVEN(sts)) continue;

      sts = gdh_GetObjectClass( child, &cid);
      if ( EVEN(sts)) continue;
      
      s0__BrowseElement *element = new s0__BrowseElement();
	
      element->Name = new std::string( name);
      strcpy( itemname, pname);
      strcat( itemname, "-");
      strcat( itemname, name);
      element->ItemName = new std::string( itemname);
      element->IsItem = false;
      if ( cid == pwr_eClass_PlantHier || cid == pwr_eClass_NodeHier)
	element->HasChildren = ODD( gdh_GetChild( child, &ch)) ? true : false;
      else
	element->HasChildren = true;
      
      if ( s0__Browse->ReturnAllProperties)
	property_mask = ~0;
      else
	opc_propertynames_to_mask( s0__Browse->PropertyNames, &property_mask);

      if ( property_mask) {
	aref = cdh_ObjidToAref( child);
	opcsrv_get_properties( element->IsItem, cid, &paref, &aref, 
			       property_mask, 0,
			       element->Properties);
      }
      s0__BrowseResponse->Elements.push_back( element);	
    }

    return SOAP_OK;
  }


  if ( (!s0__Browse->ItemName || s0__Browse->ItemName->empty()) &&
       (!s0__Browse->ItemPath || s0__Browse->ItemPath->empty())) {
    // Return rootlist
    for ( sts = gdh_GetRootList( &oid); ODD(sts); sts = gdh_GetNextSibling( oid, &oid)) {

      if ( has_max_elem && (int)s0__BrowseResponse->Elements.size() > max_elem) {
	// Max elements reached, return current oid as continuation point

	s0__BrowseResponse->ContinuationPoint = 
	  new std::string( cdh_ObjidToString( 0, oid, 1));
	*s0__BrowseResponse->MoreElements = true;
	break;
      }

      sts = gdh_ObjidToName( oid, name, sizeof(name), cdh_mName_object);
      if ( EVEN(sts))
	return opcsrv->fault( soap, opc_eResultCode_E_FAIL);

      sts = gdh_GetObjectClass( oid, &cid);
      if ( EVEN(sts))
	return opcsrv->fault( soap, opc_eResultCode_E_FAIL);
      
      s0__BrowseElement *element = new s0__BrowseElement();
      element->Name = new std::string( name);
      element->ItemName = element->Name;
      element->IsItem = false;
      if ( cid == pwr_eClass_PlantHier || cid == pwr_eClass_NodeHier)
	element->HasChildren = ODD( gdh_GetChild( child, &ch)) ? true : false;
      else
	element->HasChildren = true;

      if ( s0__Browse->ReturnAllProperties)
	property_mask = ~0;
      else
	opc_propertynames_to_mask( s0__Browse->PropertyNames, &property_mask);

      pwr_tAttrRef aref = cdh_ObjidToAref( oid);
      opcsrv_get_properties( false, cid, 0, &aref, 
			     property_mask, 0,
			     element->Properties);

      s0__BrowseResponse->Elements.push_back( element);
    }
  }
  else {
    // Return attributes and children
    pwr_tOName pname;
    pwr_tOName itemname;
    pwr_tObjName aname;
    gdh_sAttrDef *bd;
    int 	rows;
    pwr_sAttrRef paref;
    pwr_sAttrRef aref;
    
    if ( s0__Browse->ItemPath && !s0__Browse->ItemPath->empty()) {
      strncpy( pname, s0__Browse->ItemPath->c_str(), sizeof( pname));
      if ( s0__Browse->ItemName && !s0__Browse->ItemName->empty()) {
	strcat( pname, s0__Browse->ItemName->c_str());
      }
    }
    else
      strncpy( pname, s0__Browse->ItemName->c_str(), sizeof(pname));

    sts = gdh_NameToAttrref( pwr_cNOid, pname, &paref);
    if ( EVEN(sts))
      return opcsrv->fault( soap, opc_eResultCode_E_UNKNOWNITEMNAME);

    sts = gdh_GetAttrRefTid( &paref, &cid);
    if ( EVEN(sts))
      return opcsrv->fault( soap, opc_eResultCode_E_FAIL);

    if ( paref.Flags.b.Array) {
      // Return all elements
      pwr_tTypeId a_type;
      unsigned int a_size, a_offs, a_dim;
      pwr_tAttrRef oaref;
      char *s;
      char *attrname;


      sts = gdh_AttrArefToObjectAref( &paref, &oaref);
      if ( EVEN(sts))
	return opcsrv->fault( soap, opc_eResultCode_E_FAIL);

      sts = gdh_GetAttrRefTid( &oaref, &cid);
      if ( EVEN(sts))
	return opcsrv->fault( soap, opc_eResultCode_E_FAIL);

      if ( !( attrname = strrchr( pname, '.')))
	return opcsrv->fault( soap, opc_eResultCode_E_FAIL);

      attrname++;

      // Get body definition
      sts = gdh_GetObjectBodyDef( cid, &bd, &rows, pwr_cNOid);
      if ( EVEN(sts))
	return opcsrv->fault( soap, opc_eResultCode_E_FAIL);

      int bd_idx = -1;
      for ( int i = 0; i < rows; i++) {
	if ( cdh_NoCaseStrcmp( attrname, bd[i].attrName) == 0) {
	  bd_idx = i;
	  break;
	}
      }
      if ( bd_idx == -1) {
	// E_INVALIDITEMNAME
	free( (char *)bd);
	return opcsrv->fault( soap, opc_eResultCode_E_FAIL);
      }

      sts = gdh_GetAttributeCharAttrref( &paref, &a_type, &a_size, &a_offs, &a_dim);
      if ( EVEN(sts)) {
	free( (char *)bd);
	return opcsrv->fault( soap, opc_eResultCode_E_FAIL);
      }

      for ( int i = 0; i < (int)a_dim; i++) {
	s0__BrowseElement *element = new s0__BrowseElement();

	sprintf( itemname, "%s[%d]", pname, i);
	s = strrchr( itemname, '.');
	if ( s)
	  strcpy( aname, s + 1);
	else
	  return opcsrv->fault( soap, opc_eResultCode_E_FAIL);

	element->Name = new std::string( aname);
	element->ItemName = new std::string( itemname);
	element->IsItem = true;
	element->HasChildren = false;

	if ( s0__Browse->ReturnAllProperties)
	  property_mask = ~0;
	else
	  opc_propertynames_to_mask( s0__Browse->PropertyNames, &property_mask);

	if ( property_mask) {
	  sts = gdh_NameToAttrref( pwr_cNOid, itemname, &aref);
	  if ( EVEN(sts))
	    return opcsrv->fault( soap, opc_eResultCode_E_FAIL);
	  
	  opcsrv_get_properties( true, cid, &paref, &aref,
				 property_mask, &bd[bd_idx],
				 element->Properties);

	}
	s0__BrowseResponse->Elements.push_back( element);
      }
      free( (char *)bd);

      return SOAP_OK;      
    }

    if ( !cdh_tidIsCid( cid))
      return opcsrv->fault( soap, opc_eResultCode_E_FAIL);

    sts = gdh_GetObjectBodyDef( cid, &bd, &rows, pwr_cNOid);
    if ( ODD(sts)) {

      for ( int i = 0; i < rows; i++) {
	if ( bd[i].flags & gdh_mAttrDef_Shadowed)
	  continue;
	if ( bd[i].attr->Param.Info.Flags & PWR_MASK_RTVIRTUAL || 
	     bd[i].attr->Param.Info.Flags & PWR_MASK_PRIVATE)
	  continue;
	if ( bd[i].attr->Param.Info.Type == pwr_eType_CastId ||
	     bd[i].attr->Param.Info.Type == pwr_eType_DisableAttr)
	  continue;
	if ( bd[i].attr->Param.Info.Flags & PWR_MASK_RTHIDE)
	  continue;
	
	sts = gdh_ArefANameToAref( &paref, bd[i].attrName, &aref);
	if ( EVEN(sts))
	  return opcsrv->fault( soap, opc_eResultCode_E_FAIL);

	if ( bd[i].attr->Param.Info.Flags & PWR_MASK_DISABLEATTR) {
	  pwr_tDisableAttr disabled;

	  sts = gdh_ArefDisabled( &aref, &disabled);
	  if ( EVEN(sts))
	    return opcsrv->fault( soap, opc_eResultCode_E_FAIL);
	    
	  if ( disabled)
	    continue;
	}
	
	if ( bd[i].attr->Param.Info.Flags & PWR_MASK_ARRAY ||
	     bd[i].attr->Param.Info.Flags & PWR_MASK_CLASS ) {
	  s0__BrowseElement *element = new s0__BrowseElement();
	  
	  cdh_SuppressSuper( aname, bd[i].attrName);
	  element->Name = new std::string( aname);
	  strcpy( itemname, pname);
	  strcat( itemname, ".");
	  strcat( itemname, bd[i].attrName);
	  element->ItemName = new std::string( itemname);
	  element->IsItem = false;
	  element->HasChildren = true;

	  if ( s0__Browse->ReturnAllProperties)
	    property_mask = ~0;
	  else
	    opc_propertynames_to_mask( s0__Browse->PropertyNames, &property_mask);

	  if ( property_mask)
	    opcsrv_get_properties( element->IsItem, cid, &paref, &aref, 
				   property_mask, &bd[i],
				   element->Properties);

	  s0__BrowseResponse->Elements.push_back( element);
	}
	else {
	  s0__BrowseElement *element = new s0__BrowseElement();
	  
	  cdh_SuppressSuper( aname, bd[i].attrName);
	  element->Name = new std::string( aname);
	  strcpy( itemname, pname);
	  strcat( itemname, ".");
	  strcat( itemname, bd[i].attrName);
	  element->ItemName = new std::string( itemname);
	  element->IsItem = true;
	  element->HasChildren = false;

	  if ( s0__Browse->ReturnAllProperties)
	    property_mask = ~0;
	  else
	    opc_propertynames_to_mask( s0__Browse->PropertyNames, &property_mask);

	  if ( property_mask)
	    opcsrv_get_properties( element->IsItem, cid, &paref, &aref, 
				   property_mask, &bd[i],
				   element->Properties);

	  s0__BrowseResponse->Elements.push_back( element);
	} 
      }
      free( (char *)bd);      
    }

    if ( paref.Flags.b.Object) {
      for ( sts = gdh_GetChild( paref.Objid, &child); 
	    ODD(sts); 
	    sts = gdh_GetNextSibling( child, &child)) {


	if ( has_max_elem && (int)s0__BrowseResponse->Elements.size() > max_elem) {
	  // Max elements reached, return current oid as continuation point

	  s0__BrowseResponse->ContinuationPoint = 
	    new std::string( cdh_ObjidToString( 0, child, 1));
	  *s0__BrowseResponse->MoreElements = true;
	  break;
	}

	sts = gdh_ObjidToName( child, name, sizeof(name), cdh_mName_object);
	if ( EVEN(sts)) continue;

	sts = gdh_GetObjectClass( child, &cid);
	if ( EVEN(sts)) continue;
      
	s0__BrowseElement *element = new s0__BrowseElement();
	
	element->Name = new std::string( name);
	strcpy( itemname, pname);
	strcat( itemname, "-");
	strcat( itemname, name);
	element->ItemName = new std::string( itemname);
	element->IsItem = false;
	if ( cid == pwr_eClass_PlantHier || cid == pwr_eClass_NodeHier)
	  element->HasChildren = ODD( gdh_GetChild( child, &ch)) ? true : false;
	else
	  element->HasChildren = true;
					       
	if ( s0__Browse->ReturnAllProperties)
	  property_mask = ~0;
	else
	  opc_propertynames_to_mask( s0__Browse->PropertyNames, &property_mask);

	if ( property_mask) {
	  aref = cdh_ObjidToAref( child);
	  opcsrv_get_properties( element->IsItem, cid, &paref, &aref, 
				 property_mask, 0,
				 element->Properties);
	}
	s0__BrowseResponse->Elements.push_back( element);	
      }
    }
  }
  return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6 __s0__GetProperties(struct soap *soap, 
					       _s0__GetProperties *s0__GetProperties, 
					       _s0__GetPropertiesResponse *s0__GetPropertiesResponse)
{
  unsigned int property_mask;
  pwr_tCid cid;
  pwr_tAName iname;
  char *aname;
  pwr_tStatus sts;
  pwr_tAttrRef aref;
  pwr_tAttrRef paref;
  gdh_sAttrDef *bd;
  int 	rows;

  // Check access for connection
  opcsrv_client *client = opcsrv->find_client( soap->ip);
  if ( !client)
    client = opcsrv->new_client( soap->ip);
  opcsrv->m_current_access = client->access;

  switch ( opcsrv->m_current_access) {
  case pwr_eOpc_AccessEnum_ReadOnly:
  case pwr_eOpc_AccessEnum_ReadWrite:
    break;
  default:
    return opcsrv->fault( soap, opc_eResultCode_E_ACCESS_DENIED);
  }

  s0__GetPropertiesResponse->GetPropertiesResult = new s0__ReplyBase();
  s0__GetPropertiesResponse->GetPropertiesResult = new s0__ReplyBase();
  s0__GetPropertiesResponse->GetPropertiesResult->RcvTime = opc_datetime(0);
  s0__GetPropertiesResponse->GetPropertiesResult->ReplyTime = opc_datetime(0);
  if ( s0__GetProperties->ClientRequestHandle)
    s0__GetPropertiesResponse->GetPropertiesResult->ClientRequestHandle = 
      new std::string( *s0__GetProperties->ClientRequestHandle);
  s0__GetPropertiesResponse->GetPropertiesResult->ServerState = s0__serverState__running;

  if ( s0__GetProperties->ReturnAllProperties && *s0__GetProperties->ReturnAllProperties)
    property_mask = ~0;
  else
    opc_propertynames_to_mask( s0__GetProperties->PropertyNames, &property_mask);

  for ( int i = 0; i < (int)s0__GetProperties->ItemIDs.size(); i++) {
    s0__PropertyReplyList *plist = new s0__PropertyReplyList();
    std::string *path;

    if ( s0__GetProperties->ItemIDs[i]->ItemPath)
      path = s0__GetProperties->ItemIDs[i]->ItemPath;
    else
      path = s0__GetProperties->ItemPath;

    plist->ItemPath = path;
    plist->ItemName = new std::string(*s0__GetProperties->ItemIDs[i]->ItemName);

    if ( path) {
      strcpy( iname, path->c_str());
      strcat( iname, plist->ItemName->c_str());
    }
    else
      strcpy( iname, plist->ItemName->c_str());

    sts = gdh_NameToAttrref( pwr_cNOid, iname, &aref);
    if ( EVEN(sts)) {
      opcsrv_returnerror( s0__GetPropertiesResponse->Errors, &plist->ResultID, 
			  opc_eResultCode_E_UNKNOWNITEMNAME, 0);
      s0__GetPropertiesResponse->PropertyLists.push_back( plist);
      continue;
    }

    if ( aref.Flags.b.Object || aref.Flags.b.ObjectAttr) {
      // This is an object
      sts = gdh_GetAttrRefTid( &aref, &cid);
      if ( EVEN(sts)) {
	opcsrv_returnerror( s0__GetPropertiesResponse->Errors, &plist->ResultID, 
			  opc_eResultCode_E_FAIL, 0);
	s0__GetPropertiesResponse->PropertyLists.push_back( plist);
	continue;
      }
      if ( !cdh_tidIsCid( cid)) {
	opcsrv_returnerror( s0__GetPropertiesResponse->Errors, &plist->ResultID, 
			  opc_eResultCode_E_FAIL, 0);
	s0__GetPropertiesResponse->PropertyLists.push_back( plist);
	continue;
      }

      opcsrv_get_properties( false, cid, 0, &aref, 
			     property_mask, 0,
			     plist->Properties);
    }
    else {
      // Get the object attrref and class for this attribute
      if ( !( aname = strrchr( iname, '.'))) {
	opcsrv_returnerror( s0__GetPropertiesResponse->Errors, &plist->ResultID, 
			  opc_eResultCode_E_INVALIDITEMNAME, 0);
	s0__GetPropertiesResponse->PropertyLists.push_back( plist);
	continue;
      }
      aname++;
      
      sts = gdh_AttrArefToObjectAref( &aref, &paref);
      if ( EVEN(sts)) {
	opcsrv_returnerror( s0__GetPropertiesResponse->Errors, &plist->ResultID, 
			  opc_eResultCode_E_FAIL, 0);
	s0__GetPropertiesResponse->PropertyLists.push_back( plist);
	continue;
      }

      sts = gdh_GetAttrRefTid( &paref, &cid);
      if ( EVEN(sts)) {
	opcsrv_returnerror( s0__GetPropertiesResponse->Errors, &plist->ResultID, 
			  opc_eResultCode_E_FAIL, 0);
	s0__GetPropertiesResponse->PropertyLists.push_back( plist);
	continue;
      }

      // Get body definition
      sts = gdh_GetObjectBodyDef( cid, &bd, &rows, pwr_cNOid);
      if ( EVEN(sts)) {
	opcsrv_returnerror( s0__GetPropertiesResponse->Errors, &plist->ResultID, 
			  opc_eResultCode_E_FAIL, 0);
	s0__GetPropertiesResponse->PropertyLists.push_back( plist);
	continue;
      }
      int bd_idx = -1;
      for ( int i = 0; i < rows; i++) {
	if ( cdh_NoCaseStrcmp( aname, bd[i].attrName) == 0) {
	  bd_idx = i;
	  break;
	}
      }
      if ( bd_idx == -1) {
	free( (char *)bd);
	opcsrv_returnerror( s0__GetPropertiesResponse->Errors, &plist->ResultID, 
			  opc_eResultCode_E_FAIL, 0);
	s0__GetPropertiesResponse->PropertyLists.push_back( plist);
	continue;
      }

      opcsrv_get_properties( true, cid, &paref, &aref,
			     property_mask, &bd[bd_idx],
			     plist->Properties);
      free( (char *)bd);
    }

    s0__GetPropertiesResponse->PropertyLists.push_back( plist);
  }

  return SOAP_OK;
}

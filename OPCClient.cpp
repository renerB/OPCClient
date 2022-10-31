// Simple OPC Client
//
// This is a modified version of the "Simple OPC Client" originally
// developed by Philippe Gras (CERN) for demonstrating the basic techniques
// involved in the development of an OPC DA client.
//
// The modifications are the introduction of two C++ classes to allow the
// the client to ask for callback notifications from the OPC server, and
// the corresponding introduction of a message comsumption loop in the
// main program to allow the client to process those notifications. The
// C++ classes implement the OPC DA 1.0 IAdviseSink and the OPC DA 2.0
// IOPCDataCallback client interfaces, and in turn were adapted from the
// KEPWARE´s  OPC client sample code. A few wrapper functions to initiate
// and to cancel the notifications were also developed.
//
// The original Simple OPC Client code can still be found (as of this date)
// in
//        http://pgras.home.cern.ch/pgras/OPCClientTutorial/
//
//
// Luiz T. S. Mendes - DELT/UFMG - 15 Sept 2011
// luizt at cpdee.ufmg.br
//

#include <atlbase.h>    // required for using the "_T" macro
#include <iostream>
#include <thread>
#include <conio.h>
#include <variant>
#include <comdef.h>

#include "opcda.h"
#include "opcerror.h"
#include "OPCClient.h"
#include "SOCAdviseSink.h"
#include "SOCDataCallback.h"
#include "SOCWrapperFunctions.h"

using namespace std;

#define OPC_SERVER_NAME L"Matrikon.OPC.Simulation.1"
#define VT VT_R4


//#define REMOTE_SERVER_NAME L"your_path"

// Global variables

// The OPC DA Spec requires that some constants be registered in order to use
// them. The one below refers to the OPC DA 1.0 IDataObject interface.
UINT OPC_DATA_TIME = RegisterClipboardFormat (_T("OPCSTMFORMATDATATIME"));

struct OPCItem {
	char* PV;
	char* Tag;
	wchar_t* ID;
	int hClient;
	OPCHANDLE hServerItem;
};

OPCItem PRE_HEATING = { "Temperatura pré aquecimento", "Random.Real4", L"Random.Real4", 0, 0 };
OPCItem HEATING = { "Temperatura aquecimento", "Saw-toothed Waves.Real4", L"Saw-toothed Waves.Real4", 1, 0 };
OPCItem SOAK = { "Temperatura encharque", "Triangle Waves.Real4", L"Triangle Waves.Real4", 2, 0 };
OPCItem FLOW = { "Vazão", "Square Waves.Real4", L"Square Waves.Real4", 3, 0 };
OPCItem PRE_HEATING_SETPOINT = { "Setpoint pré aquecimento", "Bucket Brigade.Real8", L"Bucket Brigade.Real8", 4, 0 };
OPCItem HEATING_SETPOINT = { "Setpoint aquecimento", "Bucket Brigade.Real4", L"Bucket Brigade.Real4", 5, 0 };
OPCItem SOAK_SETPOINT = { "Setpoint encharque", "Bucket Brigade.Int4", L"Bucket Brigade.Int4", 6, 0 };
wchar_t PRE_HEATING_ID[]=L"Random.Real4";
wchar_t HEATING_ID[] = L"Saw-toothed Waves.Real4";
wchar_t SOAK_ID[] = L"Triangle Waves.Real4";
wchar_t FLOW_ID[] = L"Square Waves.Real4";
wchar_t PRE_HEATING_SETPOINT_ID[] = L"Bucket Brigade.Real8";
wchar_t HEATING_SETPOINT_ID[] = L"Bucket Brigade.Real4";
wchar_t SOAK_SETPOINT_ID[] = L"Bucket Brigade.Int4";
extern wchar_t* ITEM_IDS[7] = {PRE_HEATING_ID, HEATING_ID, SOAK_ID, FLOW_ID, PRE_HEATING_SETPOINT_ID, HEATING_SETPOINT_ID, SOAK_SETPOINT_ID };

//////////////////////////////////////////////////////////////////////
// Read the value of an item on an OPC server. 
//
void main(void)
{
	IOPCServer* pIOPCServer = NULL;   //pointer to IOPServer interface
	IOPCItemMgt* pIOPCItemMgt = NULL; //pointer to IOPCItemMgt interface

	OPCHANDLE hServerGroup; // server handle to the group
	OPCHANDLE hServerItem;  // server handle to the item

	int i;
	char buf[100];

	// Have to be done before using microsoft COM library:
	printf("Initializing the COM environment...\n");
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	// Let's instantiante the IOPCServer interface and get a pointer of it:
	printf("Instantiating the MATRIKON OPC Server for Simulation...\n");
	pIOPCServer = InstantiateServer(OPC_SERVER_NAME);
	
	// Add the OPC group the OPC server and get an handle to the IOPCItemMgt
	//interface:
	printf("Adding a group in the INACTIVE state for the moment...\n");
	AddTheGroup(pIOPCServer, pIOPCItemMgt, hServerGroup);

	// Add the OPC item. First we have to convert from wchar_t* to char*
	// in order to print the item name in the console.
    size_t m;
	wcstombs_s(&m, buf, 100, HEATING_ID, _TRUNCATE);
	printf("Adding the item %s to the group...\n", buf);
	AddTheItem(pIOPCItemMgt, hServerItem);

	//Synchronous read of the device´s item value.
	VARIANT varValue[3]; //to store the read value
	VariantInit(varValue);
	varValue[0].vt = VT_R4;
	varValue[1].vt = VT_R4;
	varValue[2].vt = VT_R4;
	varValue[0].fltVal = 0.0;
	varValue[1].fltVal = 0.0;
	varValue[2].fltVal = 0.0;
	VarToStr(varValue[0], buf);
	
		// Enters a message pump in order to process the server´s callback
	// notifications. This is needed because the CoInitialize() function
	// forces the COM threading model to STA (Single Threaded Apartment),
	// in which, according to the MSDN, "all method calls to a COM object
	// (...) are synchronized with the windows message queue for the
	// single-threaded apartment's thread." So, even being a console
	// application, the OPC client must process messages (which in this case
	// are only useless WM_USER [0x0400] messages) in order to process
	// incoming callbacks from a OPC server.
	//
	// A better alternative could be to use the CoInitializeEx() function,
	// which allows one to  specifiy the desired COM threading model;
	// in particular, calling
	//        CoInitializeEx(NULL, COINIT_MULTITHREADED)
	// sets the model to MTA (MultiThreaded Apartments) in which a message
	// loop is __not required__ since objects in this model are able to
	// receive method calls from other threads at any time. However, in the
	// MTA model the user is required to handle any aspects regarding
	// concurrency, since asynchronous, multiple calls to the object methods
	// can occur.
	//
	int bRet;
	MSG msg;
	DWORD ticks1, ticks2;
	    
	// Establish a callback asynchronous read by means of the IOPCDaraCallback
	// (OPC DA 2.0) method. We first instantiate a new SOCDataCallback object and
	// adjusts its reference count, and then call a wrapper function to
	// setup the callback.
	IConnectionPoint* pIConnectionPoint = NULL; //pointer to IConnectionPoint Interface
	DWORD dwCookie = 0;
	SOCDataCallback* pSOCDataCallback = new SOCDataCallback();
	pSOCDataCallback->AddRef();

	printf("Setting up the IConnectionPoint callback connection...\n");
	SetDataCallback(pIOPCItemMgt, pSOCDataCallback, pIConnectionPoint, &dwCookie);

	// Change the group to the ACTIVE state so that we can receive the
	// server´s callback notification
	printf("Changing the group state to ACTIVE...\n");
    SetGroupActive(pIOPCItemMgt); 

	// Enter again a message pump in order to process the server´s callback
	// notifications, for the same reason explained before.
		
	printf("Waiting for IOPCDataCallback notifications...\n");
	
	std::thread t1(ManageNotifications, bRet, msg);
	
	char KeyboardEntry = NULL;
	while (true) {
		KeyboardEntry = _getch();
		if (KeyboardEntry == 's' || KeyboardEntry == 'S') {
			printf("Teclado \n \n");
			WriteItem(pIOPCItemMgt, varValue);
			Sleep(1000);
		}
	}
	t1.join();
	// Cancel the callback and release its reference
	printf("Cancelling the IOPCDataCallback notifications...\n");
    CancelDataCallback(pIConnectionPoint, dwCookie);
	//pIConnectionPoint->Release();
	pSOCDataCallback->Release();

	// Remove the OPC item:
	printf("Removing the OPC item...\n");
	RemoveItem(pIOPCItemMgt, hServerItem);

	// Remove the OPC group:
	printf("Removing the OPC group object...\n");
    pIOPCItemMgt->Release();
	RemoveGroup(pIOPCServer, hServerGroup);

	// release the interface references:
	printf("Removing the OPC server object...\n");
	pIOPCServer->Release();

	//close the COM library:
	printf ("Releasing the COM environment...\n");
	CoUninitialize();
}

////////////////////////////////////////////////////////////////////
// Instantiate the IOPCServer interface of the OPCServer
// having the name ServerName. Return a pointer to this interface
//
IOPCServer* InstantiateServer(wchar_t ServerName[])
{
	CLSID CLSID_OPCServer;
	HRESULT hr;

	// get the CLSID from the OPC Server Name:
	hr = CLSIDFromString(ServerName, &CLSID_OPCServer);
	_ASSERT(!FAILED(hr));


	//queue of the class instances to create
	LONG cmq = 1; // nbr of class instance to create.
	MULTI_QI queue[1] =
		{{&IID_IOPCServer,
		NULL,
		0}};

	//Server info:
	//COSERVERINFO CoServerInfo =
    //{
	//	/*dwReserved1*/ 0,
	//	/*pwszName*/ REMOTE_SERVER_NAME,
	//	/*COAUTHINFO*/  NULL,
	//	/*dwReserved2*/ 0
    //}; 

	// create an instance of the IOPCServer
	hr = CoCreateInstanceEx(CLSID_OPCServer, NULL, CLSCTX_SERVER,
		/*&CoServerInfo*/NULL, cmq, queue);
	_ASSERT(!hr);

	// return a pointer to the IOPCServer interface:
	return(IOPCServer*) queue[0].pItf;
}


/////////////////////////////////////////////////////////////////////
// Add group "Group1" to the Server whose IOPCServer interface
// is pointed by pIOPCServer. 
// Returns a pointer to the IOPCItemMgt interface of the added group
// and a server opc handle to the added group.
//
void AddTheGroup(IOPCServer* pIOPCServer, IOPCItemMgt* &pIOPCItemMgt, 
				 OPCHANDLE& hServerGroup)
{
	DWORD dwUpdateRate = 0;
	OPCHANDLE hClientGroup = 0;

	// Add an OPC group and get a pointer to the IUnknown I/F:
    HRESULT hr = pIOPCServer->AddGroup(/*szName*/ L"Group1",
		/*bActive*/ FALSE,
		/*dwRequestedUpdateRate*/ 1000,
		/*hClientGroup*/ hClientGroup,
		/*pTimeBias*/ 0,
		/*pPercentDeadband*/ 0,
		/*dwLCID*/0,
		/*phServerGroup*/&hServerGroup,
		&dwUpdateRate,
		/*riid*/ IID_IOPCItemMgt,
		/*ppUnk*/ (IUnknown**) &pIOPCItemMgt);
	_ASSERT(!FAILED(hr));
}



//////////////////////////////////////////////////////////////////
// Add the Item HEATING_ID to the group whose IOPCItemMgt interface
// is pointed by pIOPCItemMgt pointer. Return a server opc handle
// to the item.
 
void AddTheItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE& hServerItem)
{
	HRESULT hr;

	// Array of items to add:
	OPCITEMDEF ItemArray[7] =
	{{
		/*szAccessPath*/ L"",
		/*szItemID*/ PRE_HEATING.ID,
		/*bActive*/ TRUE,
		/*hClient*/ PRE_HEATING.hClient,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},
	{
		/*szAccessPath*/ L"",
		/*szItemID*/ HEATING.ID,
		/*bActive*/ TRUE,
		/*hClient*/ HEATING.hClient,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},
	{
		/*szAccessPath*/ L"",
		/*szItemID*/ SOAK.ID,
		/*bActive*/ TRUE,
		/*hClient*/ SOAK.hClient,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},
	{
		/*szAccessPath*/ L"",
		/*szItemID*/ FLOW.ID,
		/*bActive*/ TRUE,
		/*hClient*/ FLOW.hClient,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},
	{
		/*szAccessPath*/ L"",
		/*szItemID*/ PRE_HEATING_SETPOINT.ID,
		/*bActive*/ TRUE,
		/*hClient*/ PRE_HEATING_SETPOINT.hClient,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},
	{
		/*szAccessPath*/ L"",
		/*szItemID*/ HEATING_SETPOINT.ID,
		/*bActive*/ TRUE,
		/*hClient*/ HEATING_SETPOINT.hClient,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},
	{
		/*szAccessPath*/ L"",
		/*szItemID*/ SOAK_SETPOINT.ID,
		/*bActive*/ TRUE,
		/*hClient*/ SOAK_SETPOINT.hClient,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	}
	};

	//Add Result:
	OPCITEMRESULT* pAddResult=NULL;
	HRESULT* pErrors = NULL;

	// Add an Item to the previous Group:
	hr = pIOPCItemMgt->AddItems(6, ItemArray, &pAddResult, &pErrors);
	if (hr != S_OK){
		printf("Failed call to AddItems function. Error code = %x\n", hr);
		exit(0);
	}

	// Server handle for the added item:
	hServerItem = pAddResult[0].hServer;
	PRE_HEATING.hServerItem = pAddResult[0].hServer;
	HEATING.hServerItem = pAddResult[0].hServer;
	SOAK.hServerItem = pAddResult[0].hServer;
	FLOW.hServerItem = pAddResult[0].hServer;
	PRE_HEATING_SETPOINT.hServerItem = pAddResult[0].hServer;
	HEATING_SETPOINT.hServerItem = pAddResult[0].hServer;
	SOAK_SETPOINT.hServerItem = pAddResult[0].hServer;


	// release memory allocated by the server:
	CoTaskMemFree(pAddResult->pBlob);

	CoTaskMemFree(pAddResult);
	pAddResult = NULL;

	CoTaskMemFree(pErrors);
	pErrors = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Read from device the value of the item having the "hServerItem" server 
// handle and belonging to the group whose one interface is pointed by
// pGroupIUnknown. The value is put in varValue. 
//
void ReadItem(IUnknown* pGroupIUnknown, OPCHANDLE hServerItem, VARIANT& varValue)
{
	// value of the item:
	OPCITEMSTATE* pValue = NULL;

	//get a pointer to the IOPCSyncIOInterface:
	IOPCSyncIO* pIOPCSyncIO;
	pGroupIUnknown->QueryInterface(__uuidof(pIOPCSyncIO), (void**) &pIOPCSyncIO);

	// read the item value from the device:
	HRESULT* pErrors = NULL; //to store error code(s)
	HRESULT hr = pIOPCSyncIO->Read(OPC_DS_DEVICE, 1, &hServerItem, &pValue, &pErrors);
	_ASSERT(!hr);
	_ASSERT(pValue!=NULL);

	varValue = pValue[0].vDataValue;

	//Release memeory allocated by the OPC server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;

	CoTaskMemFree(pValue);
	pValue = NULL;

	// release the reference to the IOPCSyncIO interface:
	pIOPCSyncIO->Release();
}

void WriteItem(IUnknown* pGroupIUnknown, VARIANT* varValue)
{
	// value of the item:
	OPCITEMSTATE* pValue = NULL;

	//get a pointer to the IOPCSyncIOInterface:
	IOPCSyncIO* pIOPCSyncIO;
	pGroupIUnknown->QueryInterface(__uuidof(pIOPCSyncIO), (void**)&pIOPCSyncIO);

	// read the item value from the device:
	OPCHANDLE hServerItems[] = { PRE_HEATING_SETPOINT.hServerItem, HEATING_SETPOINT.hServerItem, SOAK_SETPOINT.hServerItem };
	HRESULT* pErrors = NULL; //to store error code(s)
	HRESULT hr = pIOPCSyncIO->Write(3, hServerItems, varValue, &pErrors);
	_com_error err(hr);
	LPCTSTR errMsg = err.ErrorMessage();
	_ASSERT(!hr);
	_ASSERT(pValue != NULL);


	//Release memeory allocated by the OPC server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;

	CoTaskMemFree(pValue);
	pValue = NULL;

	// release the reference to the IOPCSyncIO interface:
	pIOPCSyncIO->Release();
}

///////////////////////////////////////////////////////////////////////////
// Remove the item whose server handle is hServerItem from the group
// whose IOPCItemMgt interface is pointed by pIOPCItemMgt
//
void RemoveItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE hServerItem)
{
	// server handle of items to remove:
	OPCHANDLE hServerArray[1];
	hServerArray[0] = hServerItem;
	
	//Remove the item:
	HRESULT* pErrors; // to store error code(s)
	HRESULT hr = pIOPCItemMgt->RemoveItems(1, hServerArray, &pErrors);
	_ASSERT(!hr);

	//release memory allocated by the server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;
}

////////////////////////////////////////////////////////////////////////
// Remove the Group whose server handle is hServerGroup from the server
// whose IOPCServer interface is pointed by pIOPCServer
//
void RemoveGroup (IOPCServer* pIOPCServer, OPCHANDLE hServerGroup)
{
	// Remove the group:
	HRESULT hr = pIOPCServer->RemoveGroup(hServerGroup, FALSE);
	if (hr != S_OK){
		if (hr == OPC_S_INUSE)
			printf ("Failed to remove OPC group: object still has references to it.\n");
		else printf ("Failed to remove OPC group. Error code = %x\n", hr);
		exit(0);
	}
}

void ManageNotifications(int bRet, MSG msg) {
	bRet = GetMessage(&msg, NULL, 0, 0);
	if (!bRet) {
		printf("Failed to get windows message! Error code = %d\n", GetLastError());
		exit(0);
	}
	//TranslateMessage(&msg); // This call is not really needed ...
	//DispatchMessage(&msg);  // ... but this one is!
}

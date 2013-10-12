#ifdef _WIN32
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#include <Windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <process.h>
#include <queue>
#include <string>
#include <qhash.h>
#include <sstream>
#include <qstring.h>
#include <qstringlist.h>
#include <iostream>
#include "include/public_errors.h"
#include "include/public_errors_rare.h"
#include "include/public_definitions.h"
#include "include/public_rare_definitions.h"
#include "include/ts3_functions.h"
#include "include/ts3plugin.h"
#include "include/parser.h"
#include "include/relative_pos.h"

#include <qmatrix4x4.h>
#include <qvector3d.h>
using namespace std;

static struct TS3Functions ts3Functions;

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); (dest)[destSize-1] = '\0'; }
#endif

#define PLUGIN_API_VERSION	19

#define PATH_BUFSIZE		512
#define COMMAND_BUFSIZE		128
#define INFODATA_BUFSIZE	512
#define SERVERINFO_BUFSIZE	256
#define CHANNELINFO_BUFSIZE	512
#define RETURNCODE_BUFSIZE	128
#define SERVER_NAME			L"."
#define PIPE_NAME			L"AXTS"
#define FULL_PIPE_NAME		L"\\\\" SERVER_NAME L"\\pipe\\" PIPE_NAME
#define BUFFER_SIZE			512
#define MAX_VOICE_RANGE		80
#define MAX_LW_RANGE		5000
#define MAX_SW_RANGE		1000

#define SILENT	0
#define VOICE	1
#define SW		2
#define CrossLW	3
#define LW		4
#define CrossSW	5

// Plugin variables
static char* pluginID = NULL;
uint64 connectionHandlerID = 0;
anyID myId = 0;
char* chnameArray[] = {"RT_OFP", "RT_AA", "RT_A2", "RT_A3"};
char* chPasswordArray[] = {"1234", "1234", "4321", "372polk_bolk24"};
char* logchannel[] = {"KGB",""};

// System variables
DWORD dwError = ERROR_SUCCESS;
HANDLE clientPipe;
HANDLE receiverThreadHndl	= NULL;
HANDLE handlingThreadHndl	= NULL;
HANDLE senderThreadHndl		= NULL;
HANDLE timerThreadHndl      = NULL;
queue<string> incomingMessages;
queue<string> outgoingMessages;
QHash<anyID, relativePOS_Struct> relativePosHash;
QHash<anyID,argsComOTH> players;
relativePOS_Struct *relativePos;
uint64 newcid = 9999;
uint64 oldcid = 9999;
argsComPOS *self; 
argsComPOS *oldSelf;
argsComOTH *other; 
argsComMIN *miniOther;
argsGameType *gameType;
QMatrix4x4 trans_matrix;

// Changeable variables
BOOL inRt = 0;
BOOL stopRequested = FALSE;
BOOL connected = 0;
BOOL vadEnabled = FALSE;
BOOL timerReset = FALSE;
BOOL recalcRequired = FALSE;
BOOL pipeOpen = FALSE;

// Sound resources
static char* beepin_sw = "plugins\\AxTS_Rebuild\\sounds\\beep_in_long_stereo.wav";
static char* beepin_lw = "plugins\\AxTS_Rebuild\\sounds\\beep_in_long_stereo.wav";
static char* beepout_lw = "plugins\\AxTS_Rebuild\\sounds\\rbt_long_stereo.wav";
static char* beepout_sw = "plugins\\AxTS_Rebuild\\sounds\\rbt_short_stereo.wav";

/* 
 * ALL CUSTOM FUNCTIONS HAVE SPECIFIC PREFIXES INDICATING WHAT DO THEY DO *
 * msg_				- Messaging;
 * ipc_				- Inter-process communication handling;
 * pos_				- Positioning;
 * vPos_			- Voice positioning;
 * rPos_			- Radio positioning;
 * ts3plugin_		- TS3 Callbacks and Required functions
 * ts3Functions.	- TS3 Client Library functions
 * hlp_				- Helper functions
 * chnl_            - Channel movement functions
 * prs_				- Parsing & Processing of received data
 * vce_				- Voice & Volume calculation and alterations
 */

void ipc_pipeConnect();
void ipc_receiveCommand(void* pArguments);
void ipc_sendCommand(void* pArguments);
void ipc_handlingReceivedCommands(void* pArguments);

void chnl_moveToRt();
void chnl_moveFromRt();

void msg_generateOTH(string &result);
void msg_generateREQ(string &result, anyID &targetId);
void msg_generateMIN(string &result);
void msg_generateSTT(string &result);
void msg_generateVER(string &result);

void pos_client(anyID idClient);
void pos_self();
void vPos_logic(anyID &idClient);
void vPos_infantry(anyID &idClient);
void vPos_vehicle(anyID &idClient);
void rPos_swLogic(anyID &idClient);
void rPos_lwLogic(anyID &idClient);
void rPos_clientSW(anyID &idClient);
void rPos_clientLW(anyID &idClient);

double hlp_getDistance(float x1, float y1, float z1, float x2, float y2, float z2);
double hlp_getDistanceToPlayer(anyID idClient);

void hlp_muteClient(anyID &idClient);
void hlp_unmuteClient(anyID &idClient);

BOOL hlp_checkVad();
void hlp_enableVad();
void hlp_disableVad();

BOOL hlp_majorSelfDataChange();
void hlp_sendPluginCommand(string &commandText, anyID idClient, BOOL isBroadcast);

void hlp_setMetaData(string data);
string hlp_generateMetaData();

int hlp_getChannel(anyID idClient, BOOL isSw);
int hlp_getCrossChannel(anyID idClient, BOOL isSw);

void hlp_getRadioTalkStateAll();
int hlp_getRadioTalkState(anyID idClient);

void hlp_getClientCalculations(anyID idClient);
void hlp_getClientCalculationsAll();

void hlp_timerThread(void* pArguments);
void hlp_playPositionedWave(anyID idClient, const char* path);

void prs_commandText(string &commandText, anyID &idClient, anyID &targetId);
void prs_parseREQ(anyID &idClient, anyID &targetId);
void prs_parseMIN(anyID &idClient);
void prs_parseOTH(anyID &idClient);
void prs_parsePOS();
void prs_parseVER();

void RadioNoiseDSP(float slevel, short * samples, int sampleCount);

/*********************************** Required functions START ************************************/

const char* ts3plugin_name() {
	return "AxTS Rebuild";
}

const char* ts3plugin_version() {
    return "v0.8.2.6d";
}

int ts3plugin_apiVersion() {
	return PLUGIN_API_VERSION;
}

const char* ts3plugin_author() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "MSC && Kolun && Swapp";
}

const char* ts3plugin_description() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "AxTS Rebuild: This plugin will (not) work.";
}

const char* ts3plugin_infoTitle() 
{
	return "Plugin info";
}

void ts3plugin_registerPluginID(const char* id) {
	const size_t sz = strlen(id) + 1;
	pluginID = (char*)malloc(sz * sizeof(char));
	_strcpy(pluginID, sz, id);  /* The id buffer will invalidate after exiting this function */
	//printf("PLUGIN: registerPluginID: %s\n", pluginID);
}

void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
}

void ts3plugin_freeMemory(void* data)
{
	free(data);
}

int ts3plugin_init()
{
	/* Plugin init code goes here */
	printf("PLUGIN: Plug-in startup\n");
	int errorCode = 0;

	self = new argsComPOS();
	oldSelf = new argsComPOS();
	other = new argsComOTH();
	miniOther = new argsComMIN();
	relativePos = new relativePOS_Struct();
	gameType = new argsGameType();

	errorCode = connectionHandlerID = ts3Functions.getCurrentServerConnectionHandlerID();
	if(connectionHandlerID != 0)
	{
		int connectionState;
		errorCode = ts3Functions.getConnectionStatus(connectionHandlerID, &connectionState);
		if(errorCode == ERROR_ok)
		{
			if(connectionState == STATUS_CONNECTION_ESTABLISHED)
			{
				if(ts3Functions.getClientID(connectionHandlerID, &myId) != ERROR_ok)
				{
					//printf("PLUGIN: Failed to receive client ID.\n"); 
				}
				else
				{
					//printf("PLUGIN: Current client ID: %d\n", myId);
				}

				connected = TRUE;

				// Set client metadata, publically declaring that we are using this plug-in.
				hlp_setMetaData(hlp_generateMetaData());
			}
		}
	}
	else
	{
		//printf("PLUGIN: Not connected to a server. No server connection handler or client ID available.\n");
		connected = FALSE;
		inRt = FALSE;
	}
	
	// Initialiaze a named-pipe listener thread.
	if(receiverThreadHndl == NULL)
	{
		//printf("PLUGIN: Receiver handle is unassigned. Assigning..\n");
		receiverThreadHndl = (HANDLE)_beginthread(ipc_receiveCommand, 0, NULL);
	}
	else
	{
		//printf("PLUGIN: Receiver handle already assigned. \n");
		if(GetThreadId(receiverThreadHndl) != NULL)
		{
			//printf("PLUGIN: Thread id: %d\n", GetThreadId(receiverThreadHndl));
		}
		else
		{
			//printf("PLUGIN: Couldn't start receiver thread.\n");
			return 1;
		}
	}

	//Initialize a named-pipe sender thread.
	if(senderThreadHndl == NULL)
	{
		//printf("PLUGIN: Sender handle is unassigned. Assigning..\n");
		senderThreadHndl = (HANDLE)_beginthread(ipc_sendCommand, 0, NULL);
	}
	else
	{
		//printf("PLUGIN: Sender handle already assigned. \n");
		if(GetThreadId(senderThreadHndl) != NULL)
		{
			//printf("PLUGIN: Thread id: %d\n", GetThreadId(senderThreadHndl));
		}
		else
		{
			//printf("PLUGIN: Couldn't start sender thread.\n");
			return 1;
		}
	}

	// Initialize an timer thread
	if(timerThreadHndl == NULL)
	{
		//printf("PLUGIN: Timer handle is unassigned. Assigning..\n");
		timerThreadHndl = (HANDLE)_beginthread(hlp_timerThread, 0, NULL);
	}
	else
	{
		//printf("PLUGIN: Timer handle already assigned. \n");
		if(GetThreadId(timerThreadHndl) != NULL)
		{
			//printf("PLUGIN: Thread id: %d\n", GetThreadId(timerThreadHndl));
		}
		else
		{
			//printf("PLUGIN: Couldn't start timer thread.\n");
			return 1;
		}
	}

	// Initialize an incoming message handling thread
	if(handlingThreadHndl == NULL)
	{
		//printf("PLUGIN: Handling handle is unassigned. Assigning..\n");
		handlingThreadHndl = (HANDLE)_beginthread(ipc_handlingReceivedCommands, 0, NULL);
	}
	else
	{
		//printf("PLUGIN: Handling handle already assigned. \n");
		if(GetThreadId(handlingThreadHndl) != NULL)
		{
			//printf("PLUGIN: Thread id: %d\n", GetThreadId(handlingThreadHndl));
		}
		else
		{
			//printf("PLUGIN: Couldn't start handling thread.\n");
			return 1;
		}
	}
	printf("PLUGIN: Start-up cpmplete\n");
    return 0;  /* 0 = success, 1 = failure */
}

void ts3plugin_shutdown()
{
	printf("PLUGIN: Plug-in hutdown called.\n");

	// Remove the player from RT
	if(inRt == TRUE)
	{
		chnl_moveFromRt();
	}

	// Request thread stop.
	stopRequested = TRUE;

	// Await timer thread stop
	while(timerThreadHndl != INVALID_HANDLE_VALUE)
	{
		//printf("PLUGIN: Awaiting timer thread shutdown.\n");
		Sleep(100);
	}

	//printf("PLUGIN: Timer thread shutdown confirmed.\n");

	// Await handling thread stop
	CancelSynchronousIo(handlingThreadHndl);
	while(handlingThreadHndl != INVALID_HANDLE_VALUE)
	{
		//printf("PLUGIN: Awaiting handling thread shutdown.\n");
		Sleep(100);
	}

	//printf("PLUGIN: Handling thread shutdown confirmed.\n");

	// Await sender thread stop
	CancelSynchronousIo(senderThreadHndl);
	while(senderThreadHndl != INVALID_HANDLE_VALUE)
	{
		//printf("PLUGIN: Awaiting sender thread shutdown.\n");
		Sleep(100);
	}
	//printf("PLUGIN: Sender thread shutdown confirmed.\n");

	// Await receiver thread stop
	CancelSynchronousIo(receiverThreadHndl);
	while(receiverThreadHndl != INVALID_HANDLE_VALUE)
	{
		//printf("PLUGIN: Awaiting receiver thread shutdown.\n");
		Sleep(100);
	}
	//printf("PLUGIN: Receiver thread shutdown confirmed.\n");

	// Await pipe closure
	if(clientPipe != INVALID_HANDLE_VALUE && clientPipe != NULL)
	{
		CloseHandle(clientPipe);
	}

	// Clear client metadata.
	hlp_setMetaData("");

	/* Free pluginID if we registered it */
	if(pluginID) {
		free(pluginID);
		pluginID = NULL;
	}

	printf("PLUGIN: Plug-in shutdown complete\n");
}

/*********************************** Required functions END ************************************/

/********************************** TS3 Callbacks **********************************************/

void ts3plugin_onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber)
{
	// This is called when a user joins a server (if the plug-in was launched while he was not connected) or rejoins/joins another server.
	if(newStatus == STATUS_CONNECTION_ESTABLISHED)
	{
		connectionHandlerID = ts3Functions.getCurrentServerConnectionHandlerID();
		if(connectionHandlerID)
		{
			if(ts3Functions.getClientID(connectionHandlerID, &myId) != ERROR_ok)
			{
				//printf("PLUGIN: Failed to receive client ID.\n"); 
			}
			else
			{
				//printf("PLUGIN: Connected to a server.\n");
				//printf("PLUGIN: Current client ID: %d\n", myId);

				// Set 3D settings for the server

				int errorCode = ts3Functions.systemset3DSettings(connectionHandlerID, 1.0f, 10.0f);
				if(errorCode != ERROR_ok)
				{
					//printf("PLUGIN: Failed to set system 3D settings. Error code %d\n", errorCode);
				}
				else
				{
					//printf("PLUGIN: System 3D settings set.\n");
				}
			}
			// Clear any awaiting messages. (If we for example disconnect and reconnect during game).
			while(!incomingMessages.empty())
			{
				incomingMessages.pop();
			}
			connected = TRUE;

			// Set client metadata, publically declaring that we are using this plug-in.
			hlp_setMetaData(hlp_generateMetaData());
		}
		else
		{
			//printf("PLUGIN: Failure to receive new server connection handler.\n");
			// Unload plug-in.
			ts3plugin_shutdown();
		}
	}
	else
	{
		connected = FALSE;
		inRt = FALSE;
	}
}

void ts3plugin_onPluginCommandEvent(uint64 serverConnectionHandlerID, const char* pluginName, const char* pluginCommand)
{
	if(connected == TRUE && inRt == TRUE && serverConnectionHandlerID == connectionHandlerID)
	{
		if(strcmp(pluginName, "axts_rebuild") != 0)
		{
			//printf("PLUGIN: Plugin command event failure.\n");
		}
		else
		{
			string commandText(pluginCommand);
			string tokenizedCommandText[3];
			size_t current;
			size_t next = -1;
			int iterator = 0;

			do
			{
			  current = next + 1;
			  next = commandText.find_first_of("@", current );
			  tokenizedCommandText[iterator] = commandText.substr( current, next - current );
			  iterator++;
			}
			while (next != string::npos);

			anyID idClient = (unsigned short) strtoul(tokenizedCommandText[0].c_str(), NULL, 0);
			anyID targetId = (unsigned short) strtoul(tokenizedCommandText[1].c_str(), NULL, 0);

			if(myId != idClient)
			{
				prs_commandText(tokenizedCommandText[2], idClient, targetId);
			}
		}
	}
}

void ts3plugin_onClientMoveEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* moveMessage)
{
	if(serverConnectionHandlerID == connectionHandlerID)
		{
		if(clientID == myId)
		{
			char* result;
			if(ts3Functions.getChannelVariableAsString(connectionHandlerID, newChannelID, CHANNEL_NAME, &result) == ERROR_ok)
			{
				if(strcmp(result, chnameArray[gameType->game]) != 0)
				{
					printf("PLUGIN: Switched to a non-RT channel.\n");
					inRt = FALSE;
				}
				else
				{
					printf("PLUGIN: Switched to RT channel.\n");
					newcid = newChannelID;
					oldcid = oldChannelID;
					inRt = TRUE;
				}
				ts3Functions.freeMemory(result);
			}
			else
			{
				printf("PLUGIN: Channel name query failure..\n");
			}
		}
		else
		{
			if(inRt == TRUE)
			{
				if(newChannelID == newcid)
				{
					hlp_muteClient(clientID);
				}
				else
				{
					hlp_unmuteClient(clientID);
					if(players.contains(clientID))
					{
						players.remove(clientID);
					}
				}
			}
		}
	}
}

void ts3plugin_onTalkStatusChangeEvent(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID)
{
	if(inRt == TRUE && connected == TRUE && serverConnectionHandlerID == connectionHandlerID)
	{
		if(clientID == myId)
		{
			// Me
			// Broadcast my state to everyone.
			string minMessage;
			msg_generateMIN(minMessage);
			hlp_sendPluginCommand(minMessage, clientID, TRUE);

			// Update my talking status
			if(status == STATUS_TALKING || status == STATUS_TALKING_WHILE_DISABLED)
			{
				self->talking = self->TAN + 1;
				hlp_getClientCalculationsAll();
			}
			else
			{
				self->talking = 0;
				hlp_getClientCalculationsAll();
			}
		}
		else
		{
			// Somebody else
			// Check whether we already know about him
			if(players.contains(clientID))
			{
				if(status == STATUS_TALKING || status == STATUS_TALKING_WHILE_DISABLED)
				{
					players[clientID].talking = hlp_getRadioTalkState(clientID);
					players[clientID].endedTalking = FALSE;

					if((players[clientID].talking == SW || players[clientID].talking == CrossSW))
					{
						// SW Beep-in.
						printf("DEBUG: Beep-in SW\n");
						hlp_playPositionedWave(clientID, beepin_sw);
					}
					else if ((players[clientID].talking == CrossLW || players[clientID].talking == LW))
					{
						// LW Beep-in.
						printf("DEBUG: Beep-in LW\n");
						hlp_playPositionedWave(clientID, beepin_lw);
					}	
				}
				else
				{
					// Indicate that the players has ended talking.
					players[clientID].endedTalking = TRUE;

					if((players[clientID].oldTalking == SW || players[clientID].oldTalking == CrossSW))
					{
						// SW Beep-out.
						printf("DEBUG: Beep-out SW\n");
						hlp_playPositionedWave(clientID, beepout_sw);
					}
					else if ((players[clientID].oldTalking == CrossLW || players[clientID].oldTalking == LW))
					{
						// LW Beep-in.
						printf("DEBUG: Beep-out LW\n");
						hlp_playPositionedWave(clientID, beepout_lw);
					}	
					players[clientID].talking = 0;
				}
			}
			else
			{
				//printf("PLUGIN: Unknown client. Ignoring talk state updates.\n");
			}
		}
	}
}

void ts3plugin_onCustom3dRolloffCalculationWaveEvent(uint64 serverConnectionHandlerID, uint64 waveHandle, float distance, float* volume)
{
	if(serverConnectionHandlerID == connectionHandlerID && connected == TRUE && inRt == TRUE && players.contains((anyID)distance))
	{
		float radioVolume;
		anyID idClient = (anyID)distance;

		if(players[idClient].endedTalking == FALSE)
		{
			// Use current data frame to play sounds.
			switch(players[idClient].talking)
			{
			case SW:
				radioVolume = self->kvVolArray[players[idClient].hearableKV];
				break;
			case CrossLW:
				radioVolume = self->dvVolArray[players[idClient].hearableCrossDV];
				break;
			case LW:
				radioVolume = self->dvVolArray[players[idClient].hearableDV];
				break;
			case CrossSW:
				radioVolume = self->kvVolArray[players[idClient].hearableCrossKV];
				break;
			default:
				printf("PLUGIN: Wait.. WHAT?\n");
				break;
			}
		}
		else
		{
			// Use old data frame to play sounds.
			switch(players[idClient].oldTalking)
			{
			case SW:
				radioVolume = self->kvVolArray[players[idClient].oldHearableKV];
				break;
			case CrossLW:
				radioVolume = self->dvVolArray[players[idClient].oldHearableCrossDV];
				break;
			case LW:
				radioVolume = self->dvVolArray[players[idClient].oldHearableDV];
				break;
			case CrossSW:
				radioVolume = self->kvVolArray[players[idClient].oldHearableCrossKV];
				break;
			default:
				printf("OLD PLUGIN: Wait.. WHAT?\n");
				break;
			}
		}
		*volume = radioVolume / 50;
	}	
}

void ts3plugin_onCustom3dRolloffCalculationClientEvent(uint64 serverConnectionHandlerID, anyID clientID, float distance , float* volume )
{
	if(inRt == TRUE && players.contains(clientID) && serverConnectionHandlerID == connectionHandlerID && connected == TRUE)
	{
		float calculatedVolume;

		if(players[clientID].talking == VOICE)
		{
			switch(players[clientID].Mode)
			{
			case 0:
				// Whisper
				calculatedVolume = 0.7f - distance * 0.2f; // ~4,5 meters of hearing range.
				break;
			case 1:
				// Normal
				calculatedVolume = 1.0f - distance * 0.05f; // ~ 20 meters of hearing range.
				break;
			case 2:
				// Screaming
				calculatedVolume = 1.5f - distance * 0.02f; // ~ 75 meters of hearing range.
				break;
			}
			calculatedVolume < 0 ? *volume = 0.0f : *volume = calculatedVolume;
		}
		else
		{
			switch(players[clientID].talking)
			{
			case SW:
				self->talking == 2 ? *volume = 0.0f : *volume = self->kvVolArray[players[clientID].hearableKV] / 50;
				break;
			case CrossLW:
				self->talking == 3 ? *volume = 0.0f : *volume = self->dvVolArray[players[clientID].hearableCrossDV] / 50;
				break;
			case LW:
				self->talking == 3 ? *volume = 0.0f : *volume = self->dvVolArray[players[clientID].hearableDV] / 50;
				break;
			case CrossSW:
				self->talking == 2 ? *volume = 0.0f : *volume = self->kvVolArray[players[clientID].hearableCrossKV] / 50;
				break;
			}
		}
	}
}

void ts3plugin_onEditPlaybackVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels)
{
	// Any voice alterations are to be done here, as this runs before onCustom3dRolloffCalculationEvent (which sets the volume).

	if(inRt == TRUE && players.contains(clientID) && serverConnectionHandlerID == connectionHandlerID)
	{
		if(players[clientID].talking == LW || players[clientID].talking == CrossSW)
		{
			// He is talking via his LW radio.
			RadioNoiseDSP(1 + hlp_getDistanceToPlayer(clientID) / MAX_LW_RANGE, samples, sampleCount);
		}
		else if(players[clientID].talking == SW || players[clientID].talking == CrossLW)
		{
			// He is talking via his SW radio.
			RadioNoiseDSP(1 + hlp_getDistanceToPlayer(clientID) / MAX_SW_RANGE, samples, sampleCount);
		}
	}
}

void ts3plugin_onEditPostProcessVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask)
{
	// Sound positioning (left\right ear\both) should be done here. As it runs after alterations to sound and volume were applied.

	if(inRt == TRUE && players.contains(clientID) && serverConnectionHandlerID == connectionHandlerID && connected == TRUE)
	{
		int fillMaskArray[] = {SPEAKER_FRONT_RIGHT, SPEAKER_FRONT_RIGHT + SPEAKER_FRONT_LEFT, SPEAKER_FRONT_LEFT};

		switch(players[clientID].talking) //TODO Crashes if hearable kv or else is -1;
		{
		case SW:
			*channelFillMask = fillMaskArray[self->kvPosArray[players[clientID].hearableKV]];
			break;
		case CrossLW:
			*channelFillMask = fillMaskArray[self->dvPosArray[players[clientID].hearableCrossDV]];
			break;
		case LW:
			*channelFillMask = fillMaskArray[self->dvPosArray[players[clientID].hearableDV]];
			break;
		case CrossSW:
			*channelFillMask = fillMaskArray[self->kvPosArray[players[clientID].hearableCrossKV]];
			break;
		}
	}
}

void ts3plugin_infoData(uint64 serverConnectionHandlerID, uint64 id, enum PluginItemType type, char** data)
{
	if(connected == TRUE)
	{
		char* metaData;

		switch(type) 
		{
			case PLUGIN_CLIENT:
				if(ts3Functions.getClientVariableAsString(serverConnectionHandlerID, (anyID)id, CLIENT_META_DATA, &metaData) != ERROR_ok)
				{
					//printf("PLUGIN: Failure querying metadata.\n");
					return;
				}
				break;
			default:
				data = NULL;  /* Ignore */
				return;
		}
		
		*data = (char*)malloc(INFODATA_BUFSIZE * sizeof(char));

		if(*metaData == '\0')
		{
			snprintf(*data, INFODATA_BUFSIZE, "[I]\%s\[/I]", "No plug-in detected.");
		}
		else
		{
			snprintf(*data, INFODATA_BUFSIZE, "[I]\%s\[/I]", metaData);
		}

		ts3Functions.freeMemory(metaData);
	}
}

/********************************** TS3 Callback END *******************************************/

/********************************** IPC Implementation *****************************************/
void ipc_pipeConnect()
{
	// Try to open the named pipe identified by the pipe name.
	while (stopRequested != TRUE) 
    {
        clientPipe = CreateFile( 
            FULL_PIPE_NAME,                 // Pipe name 
            GENERIC_READ | GENERIC_WRITE,   // Read and write access
            0,                              // No sharing 
            NULL,                           // Default security attributes
            OPEN_EXISTING,                  // Opens existing pipe
            0,                              // Default attributes
            NULL                            // No template file
            );

		if(clientPipe != INVALID_HANDLE_VALUE)
		{
			//printf("PLUGIN: Connected to a server pipe.\n");
			// Update metadata to indicate that we are connected.
			pipeOpen = TRUE;
			hlp_setMetaData(hlp_generateMetaData());
			// Set the read mode and the blocking mode of the named pipe.
			DWORD dwMode = PIPE_READMODE_MESSAGE | PIPE_NOWAIT;
			if (!SetNamedPipeHandleState(clientPipe, &dwMode, NULL, NULL))
			{
				dwError = GetLastError();
				//printf("PLUGIN: SetNamedPipeHandleState failed w/err 0x%08lx\n", dwError);
			}
			break;
		}
		else
		{
			// Update metadata to indicate that we are disconnected
			// Condition to prevent spamming updates of metadata.
			if(pipeOpen != FALSE)
			{
				pipeOpen = FALSE;
				hlp_setMetaData(hlp_generateMetaData());
			}
			// Wait before re-trying.
			Sleep(500);
		}
    }
}

void ipc_receiveCommand(void* pArguments)
{
	// Call pipe connection function.

		ipc_pipeConnect();

		BOOL fFinishRead = FALSE;
		char chResponse[BUFFER_SIZE];
		DWORD cbResponse, cbRead;
		cbResponse = sizeof(chResponse) -1;
		int errCode;

		while(stopRequested != TRUE)
		{
			if(connected == TRUE)
			{
				fFinishRead = ReadFile(
					clientPipe,             // Handle of the pipe
					chResponse,             // Buffer to receive the reply
					cbResponse,             // Size of buffer in bytes
					&cbRead,                // Number of bytes read 
					NULL                    // Not overlapped 
					);

				if (fFinishRead)
				{
					chResponse[cbRead] = '\0';
					incomingMessages.push(chResponse);	
				}
				else
				{
					errCode = GetLastError();
					if(errCode == 109)
					{
						CloseHandle(clientPipe);
						ipc_pipeConnect();
					}
					else if(errCode == 995)
					{
						//printf("PLUGIN: IO Operation aborted.\n");
					}
					else if(errCode == 232)
					{
						// No data to read.
						Sleep(100);
					}
					else
					{
						//printf("PLUGIN: Read failed. Error code: %d\n",GetLastError());
					}
				}
			}
		}
		CloseHandle(receiverThreadHndl);
		receiverThreadHndl = INVALID_HANDLE_VALUE;
}

void ipc_sendCommand(void* pArguments)
{
	while(clientPipe == INVALID_HANDLE_VALUE)
	{
		if(stopRequested == TRUE)
		{
			break;
		}
		else
		{
			Sleep(500);
		}
	}

	//printf("PLUGIN: Connected to a server pipe.\n");

	while(stopRequested != TRUE)
	{
		if(!outgoingMessages.empty())
		{	
			DWORD cbRequest, cbWritten;
			cbRequest = outgoingMessages.front().size() + 1;

			if (!WriteFile(
				clientPipe,							// Handle of the pipe
				outgoingMessages.front().c_str(),   // Message to be written
				cbRequest,							// Number of bytes to write
				&cbWritten,							// Number of bytes written
				NULL								// Not overlapped
				))
			{
				dwError = GetLastError();
				wprintf(L"Write to pipe failed w/err 0x%08lx\n", dwError);
				// Clear queue in case of problems.
				while(!outgoingMessages.empty())
					outgoingMessages.pop();
			}
			else
			{
				outgoingMessages.pop();
			}
		}
		else
		{
			Sleep(100);
		}
	}

	CloseHandle(senderThreadHndl);
	senderThreadHndl = INVALID_HANDLE_VALUE;
}

void ipc_handlingReceivedCommands(void* pArguments)
{
	while(stopRequested != TRUE)
	{
		if(connected == TRUE && !incomingMessages.empty())
		{
			// Get a command from the queue.
			string commandText = incomingMessages.front();
			incomingMessages.pop();
			anyID targetId = (anyID)0;

			// Pass it on for parsing.
			prs_commandText(commandText, myId, targetId);
		}
		else
		{
			Sleep(100);
		}
	}

	CloseHandle(handlingThreadHndl);
	handlingThreadHndl = INVALID_HANDLE_VALUE;
} 
/********************************** IPC Implementation END *************************************/

/********************************** Channel Movement *******************************************/
void chnl_moveToRt()
{
	// Move user to RT channel.
	int errorCode;
	char* rtChannelPath[] = {"PvP_WOG", chnameArray[gameType->game], ""};
	errorCode = ts3Functions.getChannelIDFromChannelNames(connectionHandlerID, rtChannelPath, &newcid);
	if(errorCode == ERROR_ok)
	{
		if(newcid != NULL)
		{
			anyID *clientList;
			errorCode = ts3Functions.getChannelOfClient(connectionHandlerID, myId, &oldcid);
			if(errorCode != ERROR_ok)
			{
				//printf("PLUGIN: Failed to get channel of client.\n");
			}
			
			errorCode = ts3Functions.requestClientMove(connectionHandlerID, myId, newcid, chPasswordArray[gameType->game], NULL);
			if(errorCode != ERROR_ok)
			{
				//printf("PLUGIN: Failed to requet client move.\n");
			}
			
			errorCode = ts3Functions.getChannelClientList(connectionHandlerID, newcid, &clientList);
			if(errorCode != ERROR_ok)
			{
				//printf("PLUGIN: Failed to get channel client list.\n");
			}
			else
			{
				errorCode = ts3Functions.requestMuteClients(connectionHandlerID, clientList, NULL);
				ts3Functions.freeMemory(clientList);
				if(errorCode != ERROR_ok)
				{
					//printf("PLUGIN: Failed to mute clients in channel.\n");
				}
			}
			inRt = TRUE;
		}
		else
		{
			//printf("PLUGIN: No RT channel found.\n");
		}
	}
	else
	{
		//printf("PLUGIN: Failed to get RT channel.\n");
	}
}

void chnl_moveFromRt()
{
	if(inRt == TRUE)
	{
		int errorCode;
		anyID *clientList;

		// Unmute all clients in RT
		errorCode = ts3Functions.getChannelClientList(connectionHandlerID, newcid, &clientList);
		if(errorCode == ERROR_ok)
		{
			errorCode = ts3Functions.requestUnmuteClients(connectionHandlerID, clientList, NULL);
			if(errorCode == ERROR_ok)
			{
				//printf("PLUGIN: Unmuted all clients in RT channel.\n");
			}
			else
			{
				//printf("PLUGIN: Failed unmuting all clients in RT.\n");
			}
			ts3Functions.freeMemory(clientList);
		}
		else
		{
			//printf("PLUGIN: Failed to get all clients in RT.\n");
		}

		// Move the player from RT
		errorCode = ts3Functions.requestClientMove(connectionHandlerID, myId, oldcid, "", NULL);
		if(errorCode == ERROR_ok)
		{
			//printf("PLUGIN: Moved user back to old channel.\n");
			inRt = FALSE;

			// Check if VAD reactivation is required.
			if(vadEnabled == TRUE)
			{
				hlp_enableVad();
			}
		}
		else
		{
			//printf("PLUGIN: Failed to move user back to old channel. Trying to move to default channel.\n");

			uint64* allChannels;

			if(ts3Functions.getChannelList(connectionHandlerID, &allChannels) == ERROR_ok)
			{
				int isDefault = 0,
				i = 0;
				for(; (allChannels[i] != NULL) && (isDefault == 0); i++)
				if(ts3Functions.getChannelVariableAsInt(connectionHandlerID, allChannels[i], CHANNEL_FLAG_DEFAULT, &isDefault) != ERROR_ok )
				{
					//printf("PLUGIN: Failed to check default flag of channel: %lld\n", allChannels[i]);
				}
				if(ts3Functions.requestClientMove(connectionHandlerID, myId, oldcid, "", 0) == ERROR_ok)
				{
					//printf("PLUGIN: Moved user to default channel.\n");
					inRt = FALSE;
				}
				else
				{
					//printf("PLUGIN: Failed to move user to default channel.\n");
					// Shutdown plug-in. Let the user sort it out himself.
					ts3plugin_shutdown();
				}
				ts3Functions.freeMemory(allChannels);
			}
			else
			{
				//printf("PLUGIN: Failed to get channel list.\n");
			}
		}
	}
	else
		//printf("PLUGIN: Client already not in RT.\n");

	// In any case, forget the ID of the rt channel.
	gameType->game = 0;
}

/********************************** Channel Movement END ***************************************/

/********************************** Message Generation *****************************************/
void msg_generateOTH(string &result)
{
	stringstream othStream;
	othStream << myId << "@0@[AxTS_CMD]OTH[/AxTS_CMD][AxTS_ARG]" 
		<< self->posX									<< ";" 
		<< self->posY									<< ";" 
		<< self->posZ									<< ";" 
		<< self->Dir									<< ";" 
		<< self->Mode									<< ";" 
		<< self->vehId.toLocal8Bit().constData()		<< ";" 
		<< self->isOut									<< ";" 
		<< self->kvChanArray[0] 						<< ";" 
		<< self->kvChanArray[1]							<< ";" 
		<< self->kvChanArray[2]							<< ";" 
		<< self->kvChanArray[3]							<< ";" 
		<< self->kvActive 								<< ";" 
		<< self->kvSide 								<< ";" 
		<< self->dvChanArray[0]							<< ";" 
		<< self->dvChanArray[1]							<< ";" 
		<< self->dvChanArray[2]	 						<< ";" 
		<< self->dvChanArray[3]							<< ";" 
		<< self->dvActive 								<< ";" 
		<< self->dvSide 								<< ";" 
		<< self->TAN						<< ";[/AxTS_ARG]";
	//printf("GENERATION: Generated OTH message.\n");
	result = othStream.str();
}

void msg_generateREQ(string &result, anyID &targetId)
{
	stringstream reqStream;
	reqStream << myId << "@" << targetId << "@[AxTS_CMD]REQ[/AxTS_CMD]";
	//printf("GENERATION: Generated REQ message.\n");
	result = reqStream.str();
}

void msg_generateMIN(string &result)
{
	stringstream minStream;
	minStream << myId << "@0@[AxTS_CMD]MIN[/AxTS_CMD][AxTS_ARG]"
		<< self->posX		<< ";"
		<< self->posY		<< ";"
		<< self->posZ		<< ";"
		<< self->Dir		<< ";"
		<< self->Mode		<< ";"
		<< self->TAN 		<< ";[/AxTS_ARG]";
	//printf("GENERATION: Generated MIN message.\n");
	result = minStream.str();
}

void msg_generateSTT(string &result)
{
	int kvArr[4] = {0};
	int dvArr[4] = {0};

	QHash<anyID, argsComOTH>::iterator i = players.begin();
	while(i != players.end())
	{
		if(players[i.key()].talking > 1)
		{
			if(players[i.key()].hearableKV != -1)
			{
				kvArr[players[i.key()].hearableKV]++;
			}

			if(players[i.key()].hearableCrossKV != -1)
			{
				kvArr[players[i.key()].hearableCrossKV]++;
			}
			
			if(players[i.key()].hearableDV != -1)
			{
				dvArr[players[i.key()].hearableDV]++;
			}

			if(players[i.key()].hearableCrossDV != -1)
			{
				kvArr[players[i.key()].hearableCrossDV]++;
			}
		}
		i++;
	}

	stringstream sttStream;
	sttStream << "[" << self->talking
		<< "," << kvArr[0] << ","<< kvArr[1]
		<< "," << kvArr[2] << "," << kvArr[3]
		<< "," << dvArr[0] << "," << dvArr[1]
		<< "," << dvArr[2] << "," << dvArr[3]
		<< "," << inRt << "]";
	//printf("GENERATION: Generated STT message.\n");
	result = sttStream.str();
}

void msg_generateVER(string &result)
{
	//printf("GENERATION: Generated VER request.\n");
	result = "[AxTS_CMD]VER[/AxTS_CMD]";
}

/********************************** Message Generation END *************************************/

/********************************** 3D Positioning *********************************************/

void pos_client(anyID idClient)
{
	if(players.contains(idClient))
	{
		switch(players[idClient].TAN)
		{
		case 0:
			// Voice
			vPos_logic(idClient);
			break;
		case 1:
			rPos_swLogic(idClient);
			// ShortWave
			break;
		case 2:
			rPos_lwLogic(idClient);
			// LongWave			
			break;
		}
	}
	else
	{
		//printf("POSITIONING: UNKNOWN CLIENT!\n");
	}
}

void vPos_logic(anyID &idClient)
{
	// Voice
	if(hlp_getDistanceToPlayer(idClient) < MAX_VOICE_RANGE)
	{
		if(self->vehId != "0")
		{
			if(self->isOut == 1)
			{
				if(players[idClient].vehId != "0")
				{
					if(self->vehId == players[idClient].vehId)
					{
						hlp_unmuteClient(idClient);
						vPos_vehicle(idClient);
					}
					else
					{
						if(players[idClient].isOut == 1)
						{
							hlp_unmuteClient(idClient);
							vPos_infantry(idClient);
						}
						else
						{
							hlp_muteClient(idClient);
						}
					}
				}
				else
				{
					hlp_unmuteClient(idClient);
					vPos_infantry(idClient);
				}
			}
			else
			{
				if(players[idClient].vehId != "0")
				{
					if(self->vehId == players[idClient].vehId)
					{
						hlp_unmuteClient(idClient);
						vPos_vehicle(idClient);
					}
					else
					{
						hlp_muteClient(idClient);
					}
				}
				else
				{
					hlp_muteClient(idClient);
				}
			}
		}
		else
		{
			if(players[idClient].vehId != "0")
			{
				if(players[idClient].isOut != 0)
				{
					hlp_unmuteClient(idClient);
					vPos_infantry(idClient);
				}
				else
				{
					hlp_muteClient(idClient);
				}
			}
			else
			{
				hlp_unmuteClient(idClient);
				vPos_infantry(idClient);
			}
		}
	}
	else
	{
		hlp_muteClient(idClient);
	}
}

void rPos_swLogic(anyID &idClient)
{
	if(players[idClient].hearableKV != -1)
	{
		if(hlp_getDistanceToPlayer(idClient) < MAX_SW_RANGE)
		{
			hlp_unmuteClient(idClient);
			rPos_clientSW(idClient);
		}
		else
		{
			hlp_muteClient(idClient);
		}
	}
	else if(players[idClient].hearableCrossDV != -1)
	{
		if(hlp_getDistanceToPlayer(idClient) < MAX_SW_RANGE)
		{
			hlp_unmuteClient(idClient);
			rPos_clientLW(idClient);
		}
		else
		{
			hlp_muteClient(idClient);
		}
	}
	else
	{
		vPos_logic(idClient);
	}
}

void rPos_lwLogic(anyID &idClient)
{
	if(players[idClient].hearableDV != -1)
	{
		if(hlp_getDistanceToPlayer(idClient) < MAX_LW_RANGE)
		{
			hlp_unmuteClient(idClient);
			rPos_clientLW(idClient);
		}
		else
		{
			hlp_muteClient(idClient);
		}
	}
	else if(players[idClient].hearableCrossKV != -1)
	{
		if(hlp_getDistanceToPlayer(idClient) < MAX_LW_RANGE)
		{
			hlp_unmuteClient(idClient);
			rPos_clientSW(idClient);
		}
		else
		{
			hlp_muteClient(idClient);
		}
	}
	else
	{
		vPos_logic(idClient);
	}
}

void pos_self()
{
	// Refresh own position, position all clients accordingly
	QVector3D pos = QVector3D(self->posX, self->posY, self->posZ);
	QMatrix4x4 tm;

	tm.translate(pos);
	tm.rotate(360 - self->Dir, 0, 0, 1);
	trans_matrix = tm.inverted();

	QHash<anyID, argsComOTH>::iterator i = players.begin();
	while(i != players.end())
	{
		pos_client(i.key());
		i++;
	}
}

void vPos_infantry(anyID &idClient)
{
	TS3_VECTOR clientPos;
	QVector3D rel_pos = trans_matrix.map(QVector3D(players[idClient].posX, players[idClient].posY, players[idClient].posZ));
	clientPos.x = rel_pos.x();
	clientPos.y = rel_pos.y();
	clientPos.z = rel_pos.z();

	int errorCode = ts3Functions.channelset3DAttributes(connectionHandlerID, idClient, &clientPos);
	if(errorCode != ERROR_ok)
	{
		if(errorCode == ERROR_client_invalid_id)
		{
			//printf("POSITIONING: Client %d does not exist, removing..\n", idClient);
			players.remove(idClient);
		}
		else
		{
			//printf("POSITIONING: Failed to position client %d. Error code %d\n", idClient, errorCode);
		}
	}
	else
	{
		hlp_unmuteClient(idClient);
	}
}

void vPos_vehicle(anyID &idClient)
{
	int errorCode = 0;
	TS3_VECTOR othPos;

	if(!relativePosHash.contains(idClient))
	{
		// Means we know of no such guy.
		// Calculate the coordinate drift.
		relativePos->xDrift = self->posX - players[idClient].posX;
		relativePos->yDrift = self->posY - players[idClient].posY;
		relativePos->zDrift = self->posZ - players[idClient].posZ;

		// Add the guy to the relative position hash.
		relativePosHash.insert(idClient, *relativePos);
	}

	// Set him to a required relative position.
	float x = self->posX + relativePosHash[idClient].xDrift;
	float y = self->posY + relativePosHash[idClient].yDrift;
	float z = self->posZ + relativePosHash[idClient].zDrift;
	
	QVector3D rel_pos = trans_matrix.map(QVector3D(x, y, z));
	othPos.x = rel_pos.x();
	othPos.y = rel_pos.y();
	othPos.z = rel_pos.z();

	errorCode = ts3Functions.channelset3DAttributes(connectionHandlerID, idClient, &othPos);
	if(errorCode != ERROR_ok)
	{
		if(errorCode == ERROR_client_invalid_id)
		{
			//printf("POSITIONING: Client %d does not exist, removing..\n", idClient);
			players.remove(idClient);
			relativePosHash.remove(idClient);
		}
		else
		{
			//printf("POSITIONING: Failed to position client %d. Error code %d\n", idClient, errorCode);
		}
	}
	else
	{
		//printf("POSITIONING: Remote client with ID %d positioned in 3D space.\n",idClient);
	}

	// Un-mute him.
	hlp_unmuteClient(idClient);
}
	
void rPos_clientSW(anyID &idClient)
{
	int errorCode = 0;
	TS3_VECTOR othPos;

	othPos.x = 0;
	othPos.y = 0;
	othPos.z = 1;

	errorCode = ts3Functions.channelset3DAttributes(connectionHandlerID, idClient, &othPos);
	if(errorCode != ERROR_ok)
	{
		//printf("POSITIONING: Failed to set 3D position of remote client with ID %d . Error code: %d\n", idClient,errorCode);
	}
	else
	{
		//printf("POSITIONING: Remote client with ID %d positioned in 3D space.\n",idClient);
	}
}

void rPos_clientLW(anyID &idClient)
{
	int errorCode = 0;
	TS3_VECTOR othPos;

	othPos.x = 0;
	othPos.y = 0;
	othPos.z = 1;

	errorCode = ts3Functions.channelset3DAttributes(connectionHandlerID, idClient, &othPos);
	if(errorCode != ERROR_ok)
	{
		//printf("POSITIONING: Failed to set 3D position of remote client with ID %d . Error code: %d\n", idClient,errorCode);
	}
	else
	{
		//printf("POSITIONING: Remote client with ID %d positioned in 3D space.\n",idClient);
	}
}

/********************************** 3D Positioning END ****************************************/

/********************************** Helper Functions ******************************************/

double hlp_getDistance(float x1, float y1, float z1, float x2, float y2, float z2)
{
	return sqrt(pow((x2-x1),2.0f) + pow((y2-y1),2.0f) + pow((z2-z1),2.0f)); 
}

double hlp_getDistanceToPlayer(anyID idClient)
{
	return hlp_getDistance(self->posX, self->posY, self->posZ, players[idClient].posX, players[idClient].posY, players[idClient].posZ);
}

void hlp_muteClient(anyID &idClient)
{
	// Checks if given client is already muted. If not - mutes him. Else - does nothing.
	int clientMuted = -1;
	anyID clientArray[2] = {idClient,0};
	int errorCode = ts3Functions.getClientVariableAsInt(connectionHandlerID, idClient, CLIENT_IS_MUTED, &clientMuted);
	if(errorCode != ERROR_ok)
	{
		//printf("PLUGIN: Failed to read mute status for client %d\n", idClient);
	}
	if(clientMuted == 0)
	{
		 ts3Functions.requestMuteClients(connectionHandlerID, clientArray, NULL); 
		if(errorCode != ERROR_ok)
		{
			//printf("PLUGIN: Failed to mute client with ID %d. Error code: %d\n", idClient, errorCode);
		}
		else
		{
			//printf("PLUGIN: Client with ID %d muted.\n",idClient);
		}
	}
}

void hlp_unmuteClient(anyID &idClient)
{
	// Checks if client is muted. If he is - unmutes him. Else - does nothing.
	anyID clientArray[2] = {idClient,0};
	int clientMuted;

	int errorCode = ts3Functions.getClientVariableAsInt(connectionHandlerID, idClient, CLIENT_IS_MUTED, &clientMuted);
	if(errorCode != ERROR_ok)
	{
		//printf("PLUGIN: Failed to read mute status for client %d\n", idClient);
	}
	else
	{
		if(clientMuted == 1)
		{
			errorCode = ts3Functions.requestUnmuteClients(connectionHandlerID, clientArray, NULL); 
			if(errorCode != ERROR_ok)
			{
				//printf("PLUGIN: Failed to unmute client with ID %d. Error code: %d\n", idClient, errorCode);
			}
			else
			{
				//printf("PLUGIN: Remote client with ID %d unmuted.\n",idClient);
			}
		}
	}
}

BOOL hlp_checkVad()
{
	char* vad; // Is "true" or "false"
	if(ts3Functions.getPreProcessorConfigValue(connectionHandlerID, "vad", &vad) == ERROR_ok)
	{
		if(strcmp(vad,"true") == 0)
		{
			ts3Functions.freeMemory(vad);
			return TRUE;
		}
		else
		{
			ts3Functions.freeMemory(vad);
			return FALSE;
		}
	}
	else
	{
		//printf("PLUGIN: Failed to get VAD value.\n");
		return FALSE;
	}
}

void hlp_enableVad()
{
	if(ts3Functions.setPreProcessorConfigValue(connectionHandlerID, "vad", "true") == ERROR_ok)
	{
		//printf("PLUGIN: VAD succesfully enabled.\n");
	}
	else
	{
		//printf("PLUGIN: Failure enabling VAD.\n");
	}
}

void hlp_disableVad()
{
	if(ts3Functions.setPreProcessorConfigValue(connectionHandlerID, "vad", "false") == ERROR_ok)
	{
		//printf("PLUGIN: VAD succesfully disabled.\n");
	}
	else
	{
		//printf("PLUGIN: Failure disabling VAD.\n");
	}
}

void hlp_enableMic()
{
	if(ts3Functions.setClientSelfVariableAsInt(connectionHandlerID, CLIENT_INPUT_DEACTIVATED, INPUT_ACTIVE) == ERROR_ok)
	{
		int errorCode = ts3Functions.flushClientSelfUpdates(connectionHandlerID, NULL);
		if(errorCode == ERROR_ok || errorCode == ERROR_ok_no_update)
		{
			//printf("PLUGIN: Microphone now enabled\n");
		}
		else
		{
			//printf("PLUGIN: Failed to flush client self updates. Error code: %d\n", errorCode);
		}
	}
	else
	{
		//printf("PLUGIN: Failed to set INPUT_ACTIVE client variable.\n");
	}
}

void hlp_disableMic()
{
	if(ts3Functions.setClientSelfVariableAsInt(connectionHandlerID, CLIENT_INPUT_DEACTIVATED, INPUT_DEACTIVATED) == ERROR_ok)
	{
		int errorCode = ts3Functions.flushClientSelfUpdates(connectionHandlerID, NULL);
		if(errorCode == ERROR_ok || errorCode == ERROR_ok_no_update)
		{
			//printf("PLUGIN: Microphone now disabled\n");
		}
		else
		{
			//printf("PLUGIN: Failed to flush client self updates. Error code: %d\n", errorCode);
		}
	}
	else
	{
		//printf("PLUGIN: Failed to set INPUT_DEACTIVATED client variable.\n");
	}
}

void hlp_sendPluginCommand(string &commandText, anyID idClient, BOOL isBroadcast)
{
	if(isBroadcast == TRUE)
	{
		ts3Functions.sendPluginCommand(connectionHandlerID, pluginID, commandText.c_str(), PluginCommandTarget_CURRENT_CHANNEL, NULL, NULL);
		//printf("PLUGIN: Sent a broadcast message.\n");
	}
	else
	{
		ts3Functions.sendPluginCommand(connectionHandlerID, pluginID, commandText.c_str(), PluginCommandTarget_CLIENT, &idClient, NULL);
		//printf("PLUGIN: Sent a message to specific client %d\n", idClient);
	}
}

BOOL hlp_majorSelfDataChange()
{
	// Checks if a major data change has occured.
	if(self->vehId != oldSelf->vehId					|| self->isOut != oldSelf->isOut					||
		self->kvChanArray[0] != oldSelf->kvChanArray[0] || self->kvChanArray[1] != oldSelf->kvChanArray[1]	||
		self->kvChanArray[2] != oldSelf->kvChanArray[2] || self->kvChanArray[3] != oldSelf->kvChanArray[3]	||
		self->kvActive != oldSelf->kvActive				|| self->kvSide != oldSelf->kvSide					||
		self->dvChanArray[0] != oldSelf->dvChanArray[0] || self->dvChanArray[1] != oldSelf->dvChanArray[1]	||
		self->dvChanArray[2] != oldSelf->dvChanArray[2] || self->dvChanArray[3] != oldSelf->dvChanArray[3]	||
		self->dvActive != oldSelf->dvActive				|| self->dvSide != oldSelf->dvSide					)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

int hlp_getChannel(anyID idClient, BOOL isSw) // Возвращает номер МОЕГО канала, на котором я могу слышать игрока
{
	int channel;
	for(channel = 0; channel < 4; channel++)
	{
		if(isSw == TRUE)
		{
			if(players[idClient].kvChanArray[players[idClient].kvActive] == self->kvChanArray[channel] && self->kvChanArray[channel] != 0.0f && self->kvSide == players[idClient].kvSide && self->TAN != 1)
			{
				return channel;
			}
		}
		else
		{
			if(players[idClient].dvChanArray[players[idClient].dvActive] == self->dvChanArray[channel] && self->dvChanArray[channel] != 0.0f  && self->dvSide == players[idClient].dvSide && self->TAN != 2)
			{
				return channel;
			}
		}
	}
	return -1;
}

int hlp_getCrossChannel(anyID idClient, BOOL isSw)
{
	int channel;
	for (channel = 0; channel < 4; channel++)
	{
		if(isSw == TRUE)
		{
			if(players[idClient].dvChanArray[players[idClient].dvActive] == self->kvChanArray[channel] && self->kvChanArray[channel] != 0.0f && self->kvSide == players[idClient].dvSide && self->TAN != 1)
			{
				return channel;
			}
		}
		else
		{
			if(players[idClient].kvChanArray[players[idClient].kvActive] == self->dvChanArray[channel] && self->dvChanArray[channel] != 0.0f && self->dvSide == players[idClient].kvSide && self->TAN != 2)
			{
				return channel;
			}
		}
	}
	return -1;
}

void hlp_timerThread(void* pArguments)
{
	int ticks = 0;

	while(stopRequested != TRUE)
	{
		if(timerReset == TRUE)
		{
			ticks = 0;
			timerReset = FALSE;
		}

		if(ticks > 5 && inRt == TRUE)
		{
			chnl_moveFromRt();
			timerReset = TRUE;
		}

		ticks++;
		Sleep(1000);
	}

	CloseHandle(timerThreadHndl);
	timerThreadHndl = INVALID_HANDLE_VALUE;
}

void hlp_setMetaData(string data)
{
	if(connected == TRUE)
	{
		if(ts3Functions.setClientSelfVariableAsString(connectionHandlerID, CLIENT_META_DATA, data.c_str()) == ERROR_ok)
		{
			//printf("PLUGIN: Metadata set.\n");

			if(ts3Functions.flushClientSelfUpdates(connectionHandlerID, NULL) == ERROR_ok)
			{
				//printf("PLUGIN: Metadata flush success.\n");
			}
			else
			{
				//printf("PLUGIN: Metadata flush failure.\n");
			}
		}
		else
		{
			//printf("PLUGIN: Failed to set metadata.\n");
		}
	}
	else
	{
		//printf("PLUGIN: Not connected to a server. Skipping metadata update.\n");
	}
}

string hlp_generateMetaData()
{
	string pipeState;

	if(pipeOpen == TRUE)
		pipeState = "Yes";
	else
		pipeState = "No";

	return string(ts3plugin_name()) + string(" ") + string(ts3plugin_version()) + string("\nGame connected: ") + pipeState; 
}

void hlp_playPositionedWave(anyID idClient, const char* path)
{
	uint64 waveHandle;
	TS3_VECTOR waveVector;
	waveVector.x = idClient;
	waveVector.y = 0.0f;
	waveVector.z = 0.0f;

	if(ts3Functions.playWaveFileHandle(connectionHandlerID, path, 0, &waveHandle) != ERROR_ok)
	{
		//printf("PLUGIN: Failed to play wave file.\n");
	}
	else
	{
		if(ts3Functions.set3DWaveAttributes(connectionHandlerID, waveHandle, &waveVector) != ERROR_ok)
		{
			//printf("PLUGIN: Failed set 3D wave attributes for wave file.\n");
		}
	}
}

int hlp_getRadioTalkState(anyID idClient)
{
	if(players.contains(idClient))
	{
		switch(players[idClient].TAN)
		{
		case 1:
			if(players[idClient].hearableKV != -1)
				return 2;
			else if(players[idClient].hearableCrossDV != -1)
				return 3;
			else
				return 1;
			break;
		case 2:
			if(players[idClient].hearableDV != -1)
				return 4;
			else if(players[idClient].hearableCrossKV != -1)
				return 5;
			else
				return 1;
			break;
		default:
			return 1;
			break;
		}
	}
	else
		return -1;

}

void hlp_getRadioTalkStateAll()
{
	QHash<anyID, argsComOTH>::iterator i = players.begin();
	while(i != players.end())
	{
		hlp_getRadioTalkState(i.key());
		i++;
	}
}

void hlp_getClientCalculations(anyID idClient)
{
	if(players.contains(idClient))
	{
		// Save current frame data for future use
		players[idClient].oldHearableKV			= players[idClient].hearableKV;
		players[idClient].oldHearableDV			= players[idClient].hearableDV;
		players[idClient].oldHearableCrossKV	= players[idClient].hearableCrossKV;
		players[idClient].oldHearableCrossDV	= players[idClient].oldHearableCrossDV;
		players[idClient].oldTalking			= players[idClient].talking;

		// Make new calculations based on the current data
		players[idClient].hearableKV		= hlp_getChannel(idClient, TRUE);
		players[idClient].hearableDV		= hlp_getChannel(idClient, FALSE);
		players[idClient].hearableCrossKV	= hlp_getCrossChannel(idClient, TRUE);
		players[idClient].hearableCrossDV	= hlp_getCrossChannel(idClient,FALSE);
		players[idClient].talking			= hlp_getRadioTalkState(idClient);
	}
}

void hlp_getClientCalculationsAll()
{
	QHash<anyID, argsComOTH>::iterator i = players.begin();
	while(i != players.end())
	{
		hlp_getClientCalculations(i.key());
		i++;
	}
}
/********************************* Helper Functions END ***************************************/

/********************************* Command Parsing & Processing Functions START ****************************/
void prs_commandText(string &commandText, anyID &idClient, anyID &targetId)
{
	int parseCode = commandCheck(commandText, *self, *other, *miniOther, *gameType);

	switch(parseCode)
	{
	case 0:
		//printf("PARSER: Unknown command.\n");
		break;
	case 1:
		//printf("PARSER: Corrupted command text. Failure of regexp validation.\n");
		break;
	case 10:
		//printf("PARSER: POS command parsed.\n");
		prs_parsePOS();
		break;
	case 102:
		//printf("PARSER: Failure parsing POS command.\n");
		break;
	case 11:
		//printf("PARSER: OTH command parsed.\n");
		prs_parseOTH(idClient);
		break;
	case 112:
		//printf("PARSER: Failure parsing OTH command.\n");
		break;
	case 12:
		//printf("PARSER: MIN command parsed.\n");
		prs_parseMIN(idClient);
		break;
	case 122:
		//printf("PARSER: Failure parsing MIN command.\n");
		break;
	case 13:
		//printf("PARSER: REQ command parsed.\n");
		prs_parseREQ(idClient, targetId);	
		break;
	case 14:
		//printf("PARSER: VER command parsed.\n");
		prs_parseVER();
		break;
	}
}

void prs_parsePOS()
{
	if(inRt == FALSE)
	{
		// Enque a game version request.
		string verRequest;
		msg_generateVER(verRequest);
		outgoingMessages.push(verRequest);

		if(gameType->game != 0)
		{
			// Move client to RT.
			chnl_moveToRt();
		}
	}
	timerReset = TRUE;

	string generatedMessage;

	// Check if we are newly joined.
	if(oldSelf == NULL)
	{
		// Inform everyone of our position and request they do the same.
		msg_generateOTH(generatedMessage);
		hlp_sendPluginCommand(generatedMessage, myId, TRUE);
				
		anyID targetId = (anyID)0;
		msg_generateREQ(generatedMessage, targetId);
		hlp_sendPluginCommand(generatedMessage, myId, TRUE);
	}
	else
	{
		if(hlp_majorSelfDataChange() == TRUE)
		{
			// Major data change. Inform all clients of it
			msg_generateOTH(generatedMessage);
			hlp_sendPluginCommand(generatedMessage, myId, TRUE);

			// Recalculate hearability for all clients
			hlp_getClientCalculationsAll();
		}
		else
		{
			// Minor data change.
			msg_generateMIN(generatedMessage);
			hlp_sendPluginCommand(generatedMessage, myId, TRUE);
		}
	}
	// Set own position and update positions of all known clients.
	pos_self();

	// Check if microphone needs to be enabled or disabled.
	if(self->TAN != 0 && oldSelf->TAN == 0)
	{
		// Disable VAD so as not to interfere with microphone functioning.
		vadEnabled = hlp_checkVad();
		if(vadEnabled == TRUE)
			hlp_disableVad();
		
		// Enable microphone.
		hlp_enableMic();
	}
	else if(self->TAN == 0 && oldSelf->TAN != 0)
	{
		hlp_disableMic();
		
		// Enable VAD if it was disabled.
		if(vadEnabled == TRUE)
		{
			hlp_enableVad();
			vadEnabled = FALSE;
		}
	}

	// Save data from this iteration for future use.
	*oldSelf = *self;

	// Send a status reply back to the plug-in.
	string statusMessage;
	msg_generateSTT(statusMessage);
	outgoingMessages.push(statusMessage);
}

void prs_parseOTH(anyID &idClient)
{
	// Check if a player with such an id exists
	if(players.contains(idClient))
	{
		// Renew his data.
		players[idClient].posX			= other->posX;
		players[idClient].posY			= other->posY;
		players[idClient].posZ			= other->posZ;
		players[idClient].Dir			= other->Dir;
		players[idClient].Mode			= other->Mode;
		players[idClient].isOut			= other->isOut;
		players[idClient].vehId			= other->vehId;
		players[idClient].kvChanArray[0] = other ->kvChanArray[0];
		players[idClient].kvChanArray[1] = other ->kvChanArray[1];
		players[idClient].kvChanArray[2] = other ->kvChanArray[2];
		players[idClient].kvChanArray[3] = other ->kvChanArray[3];
		players[idClient].kvActive		= other->kvActive;
		players[idClient].kvSide		= other->kvSide;
		players[idClient].dvChanArray[0] = other ->dvChanArray[0];
		players[idClient].dvChanArray[1] = other ->dvChanArray[1];
		players[idClient].dvChanArray[2] = other ->dvChanArray[2];
		players[idClient].dvChanArray[3] = other ->dvChanArray[3];
		players[idClient].dvActive		= other->dvActive;
		players[idClient].dvSide		= other->dvSide;
		players[idClient].TAN			= other->TAN;
	}
	else
	{
		// Create a new record
		players.insert(idClient, *other);
	}
	// Calculate if we can hear him.
	hlp_getClientCalculations(idClient);

	// Position him
	pos_client(idClient);
}

void prs_parseMIN(anyID &idClient)
{
	// Check if this player is known to us.
	if(players.contains(idClient))
	{
		// Update variables.
		players[idClient].posX	= miniOther->posX;
		players[idClient].posY	= miniOther->posY;
		players[idClient].posZ	= miniOther->posZ;
		players[idClient].Dir	= miniOther->Dir;
		players[idClient].Mode	= miniOther->Mode;
		players[idClient].TAN	= miniOther->TAN;

		pos_client(idClient);
	}
	else
	{
		// We don't have full info on this person, and thus have to request it from him.
		string reqMessage;
		msg_generateREQ(reqMessage, idClient);
		hlp_sendPluginCommand(reqMessage, idClient, TRUE);
	}
}

void prs_parseREQ(anyID &idClient, anyID &targetId)
{
	if(targetId == myId || targetId == 0)
	{
		// Generate an OTH message and reply to the sender of REQ message.
		string othMessage;
		msg_generateOTH(othMessage);
		hlp_sendPluginCommand(othMessage, idClient, TRUE);
	}
}

void prs_parseVER()
{
}

/**********************89*********** Command Parsing & Processing Functions END ******************************/

void RadioNoiseDSP(float slevel, short * samples, int sampleCount)
{
	float l = 1/(slevel+0.1f)/64;
	int i;
	for (i = 0; i < sampleCount; i++)
	{
		float d = (float)samples[i]/SHRT_MAX;
		float pdl;
		float pdh1;
		float pdh2;
		float k = (((float)rand()/RAND_MAX)*(1-slevel)*2+1);
		float n = ((float)rand()/RAND_MAX)*2-1;
		d *= k;
		//noise
		d += n*l;
		if (i > 0)
		{
			d = pdl + 0.15 * (d - pdl);
			pdl = d;
		}
		else
		{
			pdl = d;
		}
		if (i > 0)
		{
			float pd = d;
			d = 0.85 * (pdh1 + d - pdh2);
			pdh1 = d;
			pdh2 = pd;
		}
		else
		{
			pdh1 = d;
			pdh2 = d;
		}
		d *= 2;
		if (d > 1)
			d = 1;
		else if (d < -1)
			d = -1;
		samples[i] = d*(SHRT_MAX-1);
	}
}

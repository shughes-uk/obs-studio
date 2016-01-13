#pragma once

#include "jansson.h"
#include <deque>
#include <hash_map>
#include <hash_set>
#include <string>
#include <websocket.h>
#define REQ_GET_VERSION "GetVersion"
#define REQ_GET_AUTH_REQUIRED "GetAuthRequired"

#define REQ_GET_CURRENT_SCENE "GetCurrentScene"
#define REQ_GET_SCENE_LIST "GetSceneList"
#define REQ_SET_CURRENT_SCENE "SetCurrentScene"

#define REQ_SET_SOURCES_ORDER "SetSourceOrder"
#define REQ_SET_SOURCE_RENDER "SetSourceRender"
#define REQ_SET_SCENEITEM_POSITION_AND_SIZE "SetSceneItemPositionAndSize"

#define REQ_TOGGLE_MUTE "ToggleMute"
#define REQ_GET_VOLUMES "GetVolumes"
#define REQ_SET_VOLUME "SetVolume"

#define REQ_GET_STREAMING_STATUS "GetStreamingStatus"
#define REQ_STARTSTOP_STREAMING    "StartStopStreaming"

#define REQ_SET_PROFILE "SetProfile"

struct OBSAPIMessageHandler;

typedef json_t*(OBSAPIMessageHandler::*MessageFunction)(OBSAPIMessageHandler*, json_t*);

struct eqstr
{
	bool operator()(const char* s1, const char* s2) const
	{
		return strcmp(s1, s2) == 0;
	}
};

struct OBSAPIMessageHandler
{
	OBSServerWorker* parent;
		
		
	/*Message ID to Function Map*/
	stdext::hash_map<std::string, MessageFunction> messageMap;

	bool mapInitialized;
	void initializeMessageMap();

	/* Message Handlers */
	json_t* HandleGetStreamingStatus(OBSAPIMessageHandler* handler, json_t* message);
	json_t* HandleStartStopStreaming(OBSAPIMessageHandler* handler, json_t* message);
	json_t* HandleSetProfile(OBSAPIMessageHandler* handler, json_t* message);
	json_t* HandleGetVersion(OBSAPIMessageHandler* handler, json_t* message);
	json_t* HandleGetAuthRequired(OBSAPIMessageHandler* handler, json_t* message);
	json_t* OBSAPIMessageHandler::HandleGetSceneList(OBSAPIMessageHandler* handler, json_t* message);
	json_t* OBSAPIMessageHandler::HandleSetCurrentScene(OBSAPIMessageHandler* handler, json_t* message);
	
	struct lws *wsi;
	int ringbuffer_tail = 0;

	std::deque<json_t *> messagesToSend;

	OBSAPIMessageHandler(struct lws *ws, OBSServerWorker *_parent);

	bool HandleReceivedMessage(void *in, size_t len);
};
json_t* GetScenesJson();
bool EnumSources(void *data, obs_source_t *source);
bool EnumSceneItems(obs_scene_t *scene, obs_sceneitem_t *sceneitem, void *param);
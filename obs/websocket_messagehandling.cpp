#include "websocket_messagehandling.h"
#include "obs.h"
void OBSAPIMessageHandler::initializeMessageMap()
{
	messageMap[REQ_GET_STREAMING_STATUS] = &OBSAPIMessageHandler::HandleGetStreamingStatus;
	messageMap[REQ_STARTSTOP_STREAMING] = &OBSAPIMessageHandler::HandleStartStopStreaming;
	messageMap[REQ_GET_VERSION] = &OBSAPIMessageHandler::HandleGetVersion;
	messageMap[REQ_GET_AUTH_REQUIRED] = &OBSAPIMessageHandler::HandleGetAuthRequired;
	messageMap[REQ_GET_SCENE_LIST] = &OBSAPIMessageHandler::HandleGetSceneList;
	messageMap[REQ_SET_CURRENT_SCENE] = &OBSAPIMessageHandler::HandleSetCurrentScene;
	
	mapInitialized = true;
}

OBSAPIMessageHandler::OBSAPIMessageHandler(struct lws *ws, OBSServerWorker *_parent) :wsi(ws), mapInitialized(false)
{
	if (!mapInitialized)
	{
		initializeMessageMap();
	}
	parent = _parent;
}

json_t* GetOkResponse(json_t* id = NULL)
{
	json_t* ret = json_object();
	json_object_set_new(ret, "status", json_string("ok"));
	if (id != NULL && json_is_string(id))
	{
		json_object_set(ret, "message-id", id);
	}

	return ret;
}

json_t* GetErrorResponse(const char * error, json_t *id = NULL)
{
	json_t* ret = json_object();
	json_object_set_new(ret, "status", json_string("error"));
	json_object_set_new(ret, "error", json_string(error));

	if (id != NULL && json_is_string(id))
	{
		json_object_set(ret, "message-id", id);
	}

	return ret;
}

bool OBSAPIMessageHandler::HandleReceivedMessage(void *in, size_t len)
{
	json_error_t err;
	json_t* message = json_loads((char *)in, JSON_DISABLE_EOF_CHECK, &err);
	if (message == NULL)
	{
		/* failed to parse the message */
		return false;
	}
	json_t* type = json_object_get(message, "request-type");
	json_t* id = json_object_get(message, "message-id");
	if (!json_is_string(type))
	{
		this->messagesToSend.push_back(GetErrorResponse("message type not specified", id));
		json_decref(message);
		return true;
	}

	const char* requestType = json_string_value(type);
	MessageFunction messageFunc = messageMap[requestType];
	json_t* ret = NULL;

	if (messageFunc != NULL)
	{
		ret = (this->*messageFunc)(this, message);
	}
	else
	{
		this->messagesToSend.push_back(GetErrorResponse("message type not recognized", id));
		json_decref(message);
		return true;
	}

	if (ret != NULL)
	{
		if (json_is_string(id))
		{
			json_object_set(ret, "message-id", id);
		}

		json_decref(message);
		this->messagesToSend.push_back(ret);
		return true;
	}
	else
	{
		this->messagesToSend.push_back(GetErrorResponse("no response given", id));
		json_decref(message);
		return true;
	}

	return false;
}

/* Message Handlers */

json_t* OBSAPIMessageHandler::HandleGetVersion(OBSAPIMessageHandler* handler, json_t* message)
{
	json_t* ret = GetOkResponse();

	json_object_set_new(ret, "version", json_real(1.1));

	return ret;
}

json_t* OBSAPIMessageHandler::HandleGetAuthRequired(OBSAPIMessageHandler* handler, json_t* message)
{
	json_t* ret = GetOkResponse();

	json_object_set_new(ret, "authRequired", json_boolean(false));
	
	return ret;
}

json_t* OBSAPIMessageHandler::HandleGetStreamingStatus(OBSAPIMessageHandler* handler, json_t* message)
{
	json_t* ret = GetOkResponse();
	json_object_set_new(ret, "streaming", json_boolean(parent->OBSStreaming));
	json_object_set_new(ret, "preview-only", json_boolean(false));

	return ret;
}

json_t* OBSAPIMessageHandler::HandleStartStopStreaming(OBSAPIMessageHandler* handler, json_t* message)
{
	parent->EmitStartStopStream();
	return GetOkResponse();
}

json_t* OBSAPIMessageHandler::HandleSetCurrentScene(OBSAPIMessageHandler* handler, json_t* message)
{
	json_t* newScene = json_object_get(message, "scene-name");
	const char* name = json_string_value(newScene);
	if (newScene != NULL && json_typeof(newScene) == JSON_STRING)
	{
		obs_source_t* source = obs_get_source_by_name(name);
		obs_set_output_source(0, source);
		obs_source_release(source);
	}
	return GetOkResponse();
}
json_t* OBSAPIMessageHandler::HandleSetProfile(OBSAPIMessageHandler* handler, json_t* message)
{
	//json_t* profileName = json_object_get(message, "profileName");
	/*
	if (profileName != NULL && json_typeof(profileName) == JSON_STRING)
	{
	String name = json_string_value(profileName);

	OBSSetProfile(name);
	}*/
	return GetOkResponse();
}
json_t* OBSAPIMessageHandler::HandleGetSceneList(OBSAPIMessageHandler* handler, json_t* message)
{
	return GetScenesJson();
}


json_t* GetScenesJson()
{
	json_t* ret = GetOkResponse();
	json_t* scenes = json_array();
	obs_source_t *currentScene = obs_get_output_source(0);
	const char   *sceneName = obs_source_get_name(currentScene);

	obs_enum_sources(EnumSources, scenes);
	json_object_set_new(ret, "scenes", scenes);
	json_object_set_new(ret, "current-scene", json_string(sceneName));

	obs_source_release(currentScene);
	return ret;
}
bool EnumSources(void *param, obs_source_t *source)
{
	json_t *scenelist = reinterpret_cast<json_t*>(param);
	json_t* new_scene = json_object();
	json_t* scene_items = json_array();
	const char *name = obs_source_get_name(source);

	obs_scene_t *scene = obs_scene_from_source(source);
	if (scene)
	{
		json_object_set_new(new_scene, "name", json_string(name));
		obs_scene_enum_items(scene, EnumSceneItems, scene_items);
		json_object_set_new(new_scene, "sources", scene_items);
		json_array_insert_new(scenelist, 0, new_scene);
	}
	return true;
}

bool EnumSceneItems(obs_scene_t *scene, obs_sceneitem_t *sceneitem, void *param)
{
	json_t *scene_items = reinterpret_cast<json_t*>(param);
	json_t *scene_item = json_object();
	obs_source_t *source = obs_sceneitem_get_source(sceneitem);
	const char *name = obs_source_get_name(source);
	bool visible = obs_sceneitem_visible(sceneitem);

	json_object_set_new(scene_item, "name", json_string(name));
	json_object_set_new(scene_item, "x", json_real(0));
	json_object_set_new(scene_item, "y", json_real(0));
	json_object_set_new(scene_item, "cx", json_real(0));
	json_object_set_new(scene_item, "cy", json_real(0));
	json_object_set_new(scene_item, "render", json_boolean(visible));

	json_array_insert_new(scene_items, 0, scene_item);
	return true;
}
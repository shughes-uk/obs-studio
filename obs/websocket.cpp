/********************************************************************************
Copyright (C) 2013 Hugh Bailey <obs.jim@gmail.com>
Copyright (C) 2013 William Hamilton <bill@ecologylab.net>
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/
#include <stdio.h>
#include <string>
#include <time.h>
#include "websocket.h"
#include <QtCore>
#include "websocket_messagehandling.h"
#include "libwebsockets.h"
#include <obs.hpp>

#define MAX_MESSAGE_QUEUE 32

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag;

	if (NULL != tv) {
		GetSystemTimeAsFileTime(&ft);

		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		/*converting file time to unix epoch*/
		tmpres /= 10;  /*convert into microseconds*/
		tmpres -= DELTA_EPOCH_IN_MICROSECS;
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
	}

	if (NULL != tz) {
		if (!tzflag) {
			_tzset();
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}

	return 0;
}

const int port = 4444;
bool running;


enum protocols {	
	PROTOCOL_HTTP = 0,
	PROTOCOL_OBS_API
};

struct a_message {
	void *payload;
	size_t len;
};
static struct a_message ringbuffer[MAX_MESSAGE_QUEUE];
static int ringbuffer_head;
static OBSServerWorker *static_worker;
 
static void push_json_to_ringbuffer(json_t* data)
{
	char *messageText = json_dumps(data, 0);
	int len = strlen(messageText);
	if (ringbuffer[ringbuffer_head].payload)
		free(ringbuffer[ringbuffer_head].payload);

	ringbuffer[ringbuffer_head].payload =
		malloc(LWS_SEND_BUFFER_PRE_PADDING + len);
	ringbuffer[ringbuffer_head].len = len;

	memcpy((char *)ringbuffer[ringbuffer_head].payload +
		LWS_SEND_BUFFER_PRE_PADDING, messageText, len);

	if (ringbuffer_head == (MAX_MESSAGE_QUEUE - 1))
		ringbuffer_head = 0;
	else
		ringbuffer_head++;
}

OBSWebsocketServer::OBSWebsocketServer() : worker(), socketThread()
{
	worker.moveToThread(&socketThread);
	connect(&socketThread, SIGNAL(started()), &worker, SLOT(run()));
	connect(&worker, &OBSServerWorker::startStream, this, &OBSWebsocketServer::OBS_Start_Stream);
	connect(&worker, &OBSServerWorker::stopStream, this, &OBSWebsocketServer::OBS_Stop_Stream);
	connect(&worker,&OBSServerWorker::StartStopStream, this, &OBSWebsocketServer::OBS_Start_Stop_Stream);

	socketThread.start();
}
OBSWebsocketServer::~OBSWebsocketServer()
{

}
void OBSWebsocketServer::OBS_Scene_Selection_Changed(OBSSource source)
{
	json_t* update = json_object();
	const char* name = obs_source_get_name(source);
	json_object_set_new(update, "update-type", json_string("SwitchScenes"));
	json_object_set_new(update, "scene-name", json_string(name));
	push_json_to_ringbuffer(update);
}
void OBSWebsocketServer::OBS_Scene_Or_Source_Changed()
{
	push_json_to_ringbuffer(GetScenesJson());
}
void OBSWebsocketServer::OBS_Started_Streaming()
{
	json_t* update = json_object();
	json_object_set_new(update, "update-type", json_string("StreamStarting"));
	json_object_set_new(update, "preview-only", json_boolean(false));
	push_json_to_ringbuffer(update);
	worker.OBSStreaming = true;
}
void OBSWebsocketServer::OBS_Stopped_Streaming()
{
	json_t* update = json_object();
	json_object_set_new(update, "update-type", json_string("StreamStopping"));
	json_object_set_new(update, "preview-only", json_boolean(false));
	push_json_to_ringbuffer(update);
	worker.OBSStreaming = false;
}
void OBSWebsocketServer::OBS_Started_Recording()
{
	json_t* update = json_object();
	json_object_set_new(update, "update-type", json_string("StreamStarting"));
	json_object_set_new(update, "preview-only", json_boolean(false));
	push_json_to_ringbuffer(update);
	worker.OBSStreaming = true;

}
void OBSWebsocketServer::OBS_Stopped_Recording()
{
	json_t* update = json_object();
	json_object_set_new(update, "update-type", json_string("StreamStopping"));
	json_object_set_new(update, "preview-only", json_boolean(false));
	push_json_to_ringbuffer(update);
	worker.OBSStreaming = false;
}

OBSServerWorker::OBSServerWorker()
{
	static_worker = this;
}

OBSServerWorker::~OBSServerWorker()
{

}
void OBSServerWorker::EmitStartStopStream()
{
	emit StartStopStream();
}
void OBSServerWorker::EmitStartStream()
{
	emit startStream();
}

void OBSServerWorker::EmitStopStream()
{
	emit stopStream();
}

void OBSServerWorker::run()
{
	qDebug() << "From worker thread: " << QThread::currentThreadId();

	struct lws_context_creation_info info;
	const char *interface = NULL;
	int opts = 0;
	unsigned int ms, oldms = 0;
	struct lws_context *context;
	struct lws_protocols protocols[] = {

		{
			"http-only",		/* name */
			callback_http,		/* callback */
			32,	/* per_session_data_size */
			0,			/* max frame size / rx buffer */
		},
		{
			"obsapi",
			callback_obsapi,
			sizeof(OBSAPIMessageHandler*)
		},
		{
			NULL, NULL, 0        /* End of list */
		}
	};
	running = true;

	memset(&info, 0, sizeof info);

	info.port = 4444;
	info.iface = interface;
	
	info.protocols = protocols;
	info.ssl_cert_filepath = NULL;
	info.ssl_private_key_filepath = NULL;
	info.gid = -1;
	info.uid = -1;
	info.user = this;
	info.max_http_header_pool = 1;
	info.options = opts;

	context = lws_create_context(&info);

	unsigned int oldus = 0;
	int n = 0;
	while (n >= 0 && running)
	{
		struct timeval tv;

		gettimeofday(&tv, NULL);

		ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
		if ((ms - oldms) > 50) {
			lws_callback_on_writable_all_protocol(context,
				&protocols[PROTOCOL_OBS_API]);
			oldms = ms;
		}
		n = lws_service(context, 50);
	}

	lws_context_destroy(context);
}
int OBSServerWorker::callback_obsapi(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int n;
	OBSAPIMessageHandler **userp = (OBSAPIMessageHandler**)user;

	if (userp)
	{
		OBSAPIMessageHandler *messageHandler = *(userp);

		switch (reason) {

		case LWS_CALLBACK_ESTABLISHED:
			fprintf(stderr, "callback_obsapi: "
				"LWS_CALLBACK_ESTABLISHED\n");

			/* initiate handler */
			*userp = new OBSAPIMessageHandler(wsi, static_worker);
			
			messageHandler = *(userp);
			messageHandler->ringbuffer_tail = ringbuffer_head;
			break;

		case LWS_CALLBACK_PROTOCOL_DESTROY:
			for (n = 0; n < sizeof ringbuffer / sizeof ringbuffer[0]; n++)
				if (ringbuffer[n].payload)
					free(ringbuffer[n].payload);
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
			while (messageHandler->ringbuffer_tail != ringbuffer_head)
			{
				n = lws_write(wsi, (unsigned char *)
					ringbuffer[messageHandler->ringbuffer_tail].payload +
					LWS_SEND_BUFFER_PRE_PADDING,
					ringbuffer[messageHandler->ringbuffer_tail].len,
					LWS_WRITE_TEXT);
				if (n < 0) {
					lwsl_err("ERROR %d writing to mirror socket\n", n);
					return -1;
				}
				if (n < (int)ringbuffer[messageHandler->ringbuffer_tail].len)
					lwsl_err("mirror partial write %d vs %d\n",
					n, ringbuffer[messageHandler->ringbuffer_tail].len);

				if (messageHandler->ringbuffer_tail == (MAX_MESSAGE_QUEUE - 1))
					messageHandler->ringbuffer_tail = 0;
				else
					messageHandler->ringbuffer_tail++;

				if (((ringbuffer_head - messageHandler->ringbuffer_tail) &
					(MAX_MESSAGE_QUEUE - 1)) == (MAX_MESSAGE_QUEUE - 15))
					lws_rx_flow_allow_all_protocol(lws_get_context(wsi),
					lws_get_protocol(wsi));

				if (lws_partial_buffered(wsi) ||
					lws_send_pipe_choked(wsi)) {
					lws_callback_on_writable(wsi);
					break;

				}
			}
			if (!messageHandler->messagesToSend.empty())
			{
				json_t *message = messageHandler->messagesToSend.front();
				messageHandler->messagesToSend.pop_front();

				char *messageText = json_dumps(message, 0);

				if (messageText != NULL)
				{
					int sendLength = strlen(messageText);

					/* copy json text into memory buffer properly framed for libwebsockets */
					char* messageBuf = (char*)malloc(LWS_SEND_BUFFER_PRE_PADDING + sendLength + LWS_SEND_BUFFER_POST_PADDING);
					memcpy(messageBuf + LWS_SEND_BUFFER_PRE_PADDING, messageText, sendLength);


					n = lws_write(wsi, (unsigned char *)
						messageBuf + LWS_SEND_BUFFER_PRE_PADDING,
						sendLength, LWS_WRITE_TEXT);
					if (n < 0) {
						fprintf(stderr, "ERROR writing to socket");
					}
					free(messageBuf);
				}
				free((void *)messageText);
				json_decref(message);

				lws_callback_on_writable(wsi);
			}
			break;

		case LWS_CALLBACK_RECEIVE:
			if (messageHandler->HandleReceivedMessage(in, len))
			{
				lws_callback_on_writable(wsi);
			}
			break;

		case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
			/* you could return non-zero here and kill the connection */
			break;

		case LWS_CALLBACK_CLOSED:
			/* free user data */
			delete(*userp);
			break;
		default:
			break;
		}
	}

	return 0;
}

int OBSServerWorker::callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	return 0;
}
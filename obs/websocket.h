#pragma once
#include <libwebsockets.h>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <vector>
#include <obs.hpp>


#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

#ifndef _TIMEZONE_DEFINED 
struct timezone
{
	int  tz_minuteswest; /* minutes W of Greenwich */
	int  tz_dsttime;     /* type of dst correction */
};


#endif
class OBSServerWorker : public QObject {
	Q_OBJECT
	struct lws_context* context;

public:
	explicit OBSServerWorker();
	virtual ~OBSServerWorker();
	static int callback_obsapi(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
	static int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
	void EmitStartStopStream();
	void EmitStartStream();
	void EmitStopStream();
	bool OBSStreaming = false;
signals:
	void StartStopStream();
	void startStream();
	void stopStream();
public slots:
	void run();
	friend class OBSWebsocketServer;

};

struct cached_source
{
	std::string name;
	bool active;
};
struct cached_scene
{
	std::string name;
	bool active;
	std::vector<cached_source> sources;
};
#define MAX_SCENES = 32
class OBSWebsocketServer : public QObject {
	Q_OBJECT

public:
	OBSWebsocketServer();
	~OBSWebsocketServer();
	OBSServerWorker worker;

private:
	bool running;
	std::vector<cached_scene> scenes;
	QThread socketThread;

public slots:
	void OBS_Stopped_Recording();
	void OBS_Stopped_Streaming();
	void OBS_Started_Recording();
	void OBS_Started_Streaming();
	void OBS_Scene_Or_Source_Changed();
	void OBS_Scene_Selection_Changed(OBSSource source);


signals:
	void OBS_Start_Stop_Stream();
	void OBS_Start_Stream();
	void OBS_Stop_Stream();
	void finished();
	void error(QString err);


};

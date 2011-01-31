#include <QDir>
#include <QDebug>
#include <QWebSettings>
#include <QWebSecurityOrigin>
#include <QVariantMap>
#include <QDesktopServices>
#include <QApplication>
#include <QUrl>
#include <onering.h>
#include "application.h"
#include "oneringapp.h"
#include "app.h"
#include "oneringview.h"
#include "dataloader.h"
#include "debugger.h"
#include "json.h"

// plugins
#include "menu.h"
#include "systemtrayicon.h"
#include "hotkey.h"

static QUrl getAbsUrl(const QString &url, const QString &appname)
{
	QUrl u(url);

	if (u.isRelative()) {
		u.setScheme("onering");
		u.setHost(appname);
	}
	return u;
}

#ifdef Q_WS_MAC
static OSStatus appleEventProcessor(const AppleEvent *ae, AppleEvent *event,
		long handlerRefCon)
{
	Q_UNUSED(event);

	qDebug() << "xxx";
	Application *app = (Application *) handlerRefCon;

	OSType aeID = typeWildCard;
	OSType aeClass = typeWildCard;

	AEGetAttributePtr(ae, keyEventClassAttr, typeType, 0,
			&aeClass, sizeof(aeClass), 0);
	AEGetAttributePtr(ae, keyEventIDAttr, typeType, 0,
			&aeID, sizeof(aeID), 0);

	if (aeClass == kCoreEventClass) {
		if (aeID == kAEReopenApplication) {
			app->emitReopen();
		}
		return noErr;
	}

	return eventNotHandledErr;
}
#endif

Application::Application(int &argc, char **argv)
	: QApplication(argc, argv),
	  quiting(false)
{
}

Application::~Application()
{
	quiting = true;
}

int Application::load(const char* appname)
{
#ifdef Q_OS_MAC
	QDir plugins_dir = QCoreApplication::applicationDirPath();
	if (!plugins_dir.path().startsWith("/System/")) {
		plugins_dir.cd("../plugins");
		QCoreApplication::setLibraryPaths(QStringList(plugins_dir.path()));
	}
#endif
	qDebug() << "plugins dir:" << QCoreApplication::libraryPaths();

	QWebSettings *settings = QWebSettings::globalSettings();
	settings->setAttribute(QWebSettings::JavascriptCanOpenWindows, true);
	settings->setAttribute(QWebSettings::JavascriptCanAccessClipboard, true);
	settings->setAttribute(QWebSettings::LocalStorageEnabled, true);
	settings->setAttribute(QWebSettings::LocalStorageDatabaseEnabled, true);
	settings->setAttribute(QWebSettings::LocalContentCanAccessRemoteUrls, true);
	settings->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);

	QWebSecurityOrigin::addLocalScheme("onering");

	register_onering_app("onering");

	// register plugin apps
	register_menu_app("menu");
	register_systray_app("systray");
	register_hotkey_app("hotkey");

	QUrl initurl;
	initurl.setScheme("onering");
	initurl.setHost(appname);
	initurl.setPath("/init");
	QByteArray response = call_app_body("GET", initurl);

	QVariantMap props = Json::parse(QString::fromUtf8(response)).toMap();

	QString s;
	QUrl url;
	QDir dir;

	QString real_appname = props.value("appname", appname).toString();
	qDebug() << "appname:" << real_appname;
	QCoreApplication::setApplicationName(real_appname);

	s = props.value("storageDir").toString();
	dir = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
	s = QDir::cleanPath(dir.filePath(s));
	qDebug() << "data location:" << s;
	settings->setLocalStoragePath(s);

	s = props.value("icon").toString();
	if (!s.isEmpty()) {
		url = getAbsUrl(s, appname);
		DataLoader *dataloader = new DataLoader();
		Debugger::traceObj(dataloader);
		connect(dataloader, SIGNAL(got(QByteArray&)),
				this, SLOT(setWindowIconByData(QByteArray&)));
		connect(dataloader, SIGNAL(got(QByteArray&)),
				dataloader, SLOT(deleteLater()));
		dataloader->load(url);
	}

	s = props["url"].toString();
	if (s.isEmpty()) {
		s = "/";
	}
	props["url"] = getAbsUrl(s, appname);

	OneRingView* window = OneRingApp::createWindow(props);

#ifdef Q_WS_MAC
	// Install Reopen Application Event (Dock Clicked)
	qDebug() << "install";
	m_appleEventProcessorUPP = AEEventHandlerUPP(appleEventProcessor);
	AEInstallEventHandler(kCoreEventClass, kAEReopenApplication,
			m_appleEventProcessorUPP, (SRefCon)this, true);
#endif
	Debugger::traceObj(window);
	window->show();
	return 0;
}

void Application::setWindowIconByData(QByteArray &data)
{
	qDebug() << "setWindowIconByData";
	QPixmap pixmap;
	pixmap.loadFromData(data);
	setWindowIcon(QIcon(pixmap));
}

#ifdef Q_WS_MAC
void Application::emitReopen()
{
	emit dockClicked();
}

bool Application::macEventFilter(EventHandlerCallRef caller, EventRef event)
{
    UInt32 eKind = GetEventKind(event);
    UInt32 eClass = GetEventClass(event);
    qDebug() << "xxxxxxxxxxxx";
    if (eClass == kEventClassApplication && eKind == kEventAppActivated) {
    	qDebug() << "!!!!!!!!!!!!!";
    }

    return(QApplication::macEventFilter(caller, event));
}
#endif

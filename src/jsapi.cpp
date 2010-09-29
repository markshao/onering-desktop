#include <QDebug>
#include <qwebview.h>
#include <qwebframe.h>
#ifdef DEBUG
#include <QWebInspector>
#endif
#include <QIcon>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSystemTrayIcon>
#include <QApplication>
#include "jsapi.h"
#include "oneringview.h"
#include "networkaccessmanager.h"
#include "systemtrayicon.h"
#include "menu.h"
#include "hotkey.h"

JsApi::JsApi(QObject *parent)
	: QObject(parent)
{
}

void JsApi::setWebView(QWebView *view)
{
	QWebPage *page = view->page();
	frame = page->mainFrame();

	attachObject();
	connect(frame, SIGNAL(javaScriptWindowObjectCleared()), this, SLOT(attachObject()));
}

void JsApi::setWindow(QWidget *window)
{
	this->window = window;
}

void JsApi::attachObject()
{
	frame->addToJavaScriptWindowObject(QString("_OneRing"), this);
}

void JsApi::invokeCallback(const QString &funcname)
{
	qDebug() << "invokeCallback" << funcname;

	frame->evaluateJavaScript(QString("ONERING.callback('%1'); null;").arg(funcname));
}

void JsApi::createWindow(const QString &url, int width, int height, const QString &props_json)
{
	qDebug() << "JsApi::createWindow" << url << width << height << props_json;

	Qt::WindowFlags flags = 0;
	QScriptValue props = parseJSON(props_json);
	if (props.isObject() && props.property("popup").toBool()) {
		flags |= Qt::Popup;
	}
	OneRingView *window = new OneRingView(0, flags);
	window->load(frame->baseUrl().resolved(url));
	window->resize(width, height);
	window->show();
}

void JsApi::Window_hide()
{
	window->hide();
}

void JsApi::Window_maximize()
{
	window->showMaximized();
}

void JsApi::Window_showNormal()
{
	window->showNormal();
}

void JsApi::Window_enableContextMenu()
{
	static_cast<OneRingView*>(window)->enableContextMenu();
}

void JsApi::Window_disableContextMenu()
{
	static_cast<OneRingView*>(window)->disableContextMenu();
}

void JsApi::log(const QString &s)
{
	qDebug() << "JsApi::log" << s;
}

void JsApi::showInspector()
{
	qDebug() << "JsApi::showInspector";
#ifdef DEBUG
	if (!inspector.page()) {
		inspector.setPage(frame->page());
	}
	inspector.show();
	inspector.activateWindow();  // put inspector at the top most
#endif
}

void JsApi::exit()
{
	qApp->quit();
}

long JsApi::SystemTrayIcon_new()
{
	qDebug() << "JsApi::systemTrayIcon_new";

	SystemTrayIcon *icon = new SystemTrayIcon();
	return (long)icon;
}

void JsApi::SystemTrayIcon_load(long handler, const QString &url)
{
	qDebug() << "JsApi::systemTrayIcon_load" << handler << url;

	SystemTrayIcon *icon = (SystemTrayIcon *)handler;
	icon->load(frame->baseUrl().resolved(url));
}

void JsApi::SystemTrayIcon_bind(long handler, const QString &event, const QString &callback_funcname)
{
	qDebug() << "JsApi::systemTrayIcon_bind" << handler << event << callback_funcname;

	SystemTrayIcon *icon = (SystemTrayIcon *)handler;

	if (event == "click") {
		registerCallback(icon, event, callback_funcname);
		connect(icon, SIGNAL(activated(const QString&)),
			this, SLOT(callback(const QString&)));
	}
}

void JsApi::SystemTrayIcon_setContextMenu(long handler, long menu_handler)
{
	SystemTrayIcon *icon = (SystemTrayIcon *)handler;
	Menu *menu = (Menu *)menu_handler;
	icon->setContextMenu(menu);
}

QString JsApi::SystemTrayIcon_getGeometry(long handler)
{
	SystemTrayIcon *icon = (SystemTrayIcon *)handler;

	QRect rect = icon->geometry();
	return QString("{top:%1, left:%2, width:%3, height:%4}")
		.arg(rect.top()).arg(rect.left())
		.arg(rect.width()).arg(rect.height());
}

long JsApi::Menu_new()
{
	Menu *menu = new Menu();
	return (long)menu;
}

void JsApi::Menu_addSeparator(long handler)
{
	Menu *menu = (Menu *)handler;
	menu->addSeparator();
}

void JsApi::Menu_addItem(long handler, const QString &title, const QString &callback)
{
	Menu *menu = (Menu *)handler;
	MenuItem *item = menu->addItem(title);
	registerCallback(item, "", callback);
	connect(item, SIGNAL(triggered()), this, SLOT(callback()));
}

long JsApi::HotKey_new(const QString &keyseq, const QString &callback)
{
	HotKey *hotkey = new HotKey(QKeySequence(keyseq), this);
	registerCallback(hotkey, "", callback);
	connect(hotkey, SIGNAL(activated()), this, SLOT(callback()));
	return (long)hotkey;
}

void JsApi::HotKey_delete(long handler)
{
	HotKey *hotkey = (HotKey *)handler;
	hotkey->setDisabled();
	delete hotkey;
}

// private methods

void JsApi::callback(const QString &event)
{
	qDebug() << "callback" << event;

	QObject *sender = QObject::sender();
	const QList<QString>& funcnames = callbacks[EventSource(sender, event)];
	for (int i = 0; i < funcnames.size(); ++i) {
		invokeCallback(funcnames.at(i));
	}
}

void JsApi::callback()
{
	callback("");
}

void JsApi::registerCallback(QObject *sender, const QString &event, const QString &callback_funcname)
{
	if (!callback_funcname.isEmpty()) {
		qDebug() << "registerCallback" << sender << event << callback_funcname;
		callbacks[EventSource(sender, event)] << callback_funcname;
	}
}

QScriptValue JsApi::parseJSON(const QString &json)
{
	return engine.evaluate("("+json+")");
}

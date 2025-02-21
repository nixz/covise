/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#if 0
#define setfill qtsetfill
#define setprecision qtsetprecision
#define setw qtsetw
#endif
#include "TUIApplication.h"
#include <QApplication>
#if 0
#undef setfill
#undef setprecision
#undef setw
#endif

#include <QSocketNotifier>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QCloseEvent>

#include "TUIElement.h"
#define IOMANIPH

#include <QImage>
#include <QPixmap>
#include <QToolBar>
#include <QToolButton>
#include <QMenuBar>
#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QMessageBox>
#include <QString>
#include <QSplitter>
#include <QTimer>
#include <QLayout>
#include <QStyleFactory>
#include <QAction>
#include <QSignalMapper>
#include <QTabWidget>
#include <QToolTip>
#include <QFont>
#if !defined _WIN32_WCE && !defined ANDROID_TUI
#include <config/CoviseConfig.h>
#include <config/coConfig.h>
#endif

#include "TUIApplication.h"
#include "TUITab.h"
#include "TUITextureTab.h"
#include "TUISGBrowserTab.h"
#include "TUIAnnotationTab.h"
#ifndef WITHOUT_VIRVO
#if !defined _WIN32_WCE && !defined ANDROID_TUI
#include "TUIFunctionEditorTab.h"
#endif
#endif
#include "TUIColorTab.h"
#include "TUITabFolder.h"
#include "TUIButton.h"
#include "TUIColorTriangle.h"
#include "TUIColorButton.h"
#include "TUIToggleButton.h"
#include "TUIToggleBitmapButton.h"
#include "TUIComboBox.h"
#include "TUIListBox.h"
#include "TUIFloatSlider.h"
#include "TUISlider.h"
#include "TUINavElement.h"
#include "TUIFloatEdit.h"
#include "TUIProgressBar.h"
#include "TUIIntEdit.h"
#include "TUILabel.h"
#include "TUIFrame.h"
#include "TUIScrollArea.h"
#include "TUISplitter.h"
#include "TUIFileBrowserButton.h"
#include "TUIMap.h"
//#include "TUITextSpinEdit.h"
#include "TUILineEdit.h"
#include "TUITextEdit.h"
#include "TUIPopUp.h"
#include "TUIUITab.h"

#ifdef TABLET_PLUGIN
#include "../mapeditor/handler/MEMainHandler.h"
#include "../mapeditor/widgets/MEUserInterface.h"
#else
#include "icons/exit.xpm"
#include "icons/covise.xpm"
#endif

#if !defined _WIN32_WCE && !defined ANDROID_TUI
#include <net/tokenbuffer.h>
#else
#include "wce_msg.h"
#endif

TUIMainWindow *TUIMainWindow::appwin = 0;

//======================================================================

TUIMainWindow *TUIMainWindow::getInstance()
{
    if (appwin)
        return appwin;

    appwin = new TUIMainWindow(0);
    return appwin;
}

//======================================================================
#ifdef TABLET_PLUGIN

TUIMainWindow::TUIMainWindow(QWidget *parent)
    : QFrame(parent)
    , port(31802)
    , lastID(-10)
    , serverSN(NULL)
    , clientSN(NULL)
    , dialog(NULL)
    , sConn(NULL)
    , clientConn(NULL)
    , lastElement(NULL)
{

    setFrameStyle(QFrame::Panel | QFrame::Sunken);
    setContentsMargins(2, 2, 2, 2);
    setFont(mainFont);
    setWindowTitle("COVISE:TabletUI");
    mainFrame = this;

    // main layout
    mainGrid = new QGridLayout(mainFrame);

    // init some values
    appwin = this;

    port = covise::coCoviseConfig::getInt("port", "COVER.TabletPC", 31802);
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN); // otherwise writes to a closed socket kill the application.
#endif

    // initialize two timer
    // timer.....waits for disconneting vrb clients
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timerDone()));

    resize(500, 200);
}

#else

/// ============================================================

TUIMainWindow::TUIMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , port(31802)
    , lastID(-10)
    , serverSN(NULL)
    , clientSN(NULL)
    , dialog(NULL)
    , sConn(NULL)
    , clientConn(NULL)
    , lastElement(NULL)
{
    // init some values
    appwin = this;

#if !defined _WIN32_WCE && !defined ANDROID_TUI
    port = covise::coCoviseConfig::getInt("port", "COVER.TabletPC", 31802);
#else
    port = 31802;
#endif
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN); // otherwise writes to a closed socket kill the application.
#endif

    // initialize two timer
    // timer.....waits for disconneting vrb clients

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timerDone()));

// create the menus and toolbar buttons
//createMenubar();
#ifndef _WIN32_WCE
    createToolbar();
#endif

    // widget that contains the main windows(mainFrame)

    QWidget *w;
#ifdef _WIN32_WCE
    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setBackgroundRole(QPalette::Dark);
    w = scrollArea;
#else
    w = new QWidget(this);
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(w);
#endif

    QVBoxLayout *vbox = new QVBoxLayout(w);

// main windows
#ifdef _WIN32_WCE
    mainFrame = new QFrame(w);
    mainFrame->setContentsMargins(1, 1, 1, 1);
#else
    mainFrame = new QFrame();
    mainFrame->setFrameStyle(QFrame::WinPanel | QFrame::Sunken);
    mainFrame->setContentsMargins(2, 2, 2, 2);
#endif
    vbox->addWidget(mainFrame, 1);

    // main layout
    mainGrid = new QGridLayout(mainFrame);

    setFont(mainFont);

#ifdef _WIN32_WCE
    setWindowTitle("COVISE: PocketUI");
    setCentralWidget(w);
#else
    setWindowTitle("COVISE: TabletUI");
    setCentralWidget(scrollArea);
#endif

    // set a logo &size
    setWindowIcon(QPixmap(logo));
#ifdef _WIN32_WCE
    setMaximumWidth(480);
    setMaximumHeight(480);
#else
    resize(800, 600);
#endif
}
#endif

TUIMainWindow::~TUIMainWindow()
{
#ifdef TABLET_PLUGIN
    closeServer();
#else
    delete sConn;
#endif
}

void TUIMainWindow::setPort(int p)
{
    port = p;
}

//------------------------------------------------------------------------
// show this message after 2 sec
// wait 2 more sec to disconnect clients or exit
//------------------------------------------------------------------------
void TUIMainWindow::timerDone()
{
    timer->stop();
}

//------------------------------------------------------------------------
void TUIMainWindow::closeServer()
//------------------------------------------------------------------------
{
    delete serverSN;
    serverSN = NULL;

    connections->remove(sConn);
    delete sConn;
    sConn = NULL;

    if (clientConn)
    {
        connections->remove(clientConn);
        delete clientConn;
        clientConn = NULL;
    }
}

//------------------------------------------------------------------------
int TUIMainWindow::openServer()
//------------------------------------------------------------------------
{
    delete sConn;
    sConn = new covise::ServerConnection(port, 0, (covise::sender_type)0);
    if (sConn->getSocket() == NULL)
    {
        fprintf(stderr, "Could not open server port %d\n", port);
        delete sConn;
        sConn = NULL;
        return (-1);
    }
    struct linger linger;
    linger.l_onoff = 0;
    linger.l_linger = 0;

    setsockopt(sConn->get_id(NULL), SOL_SOCKET, SO_LINGER, (char *)&linger, sizeof(linger));

    sConn->listen();
    if (!sConn->is_connected()) // could not open server port
    {
        fprintf(stderr, "Could not open server port %d\n", port);
        delete sConn;
        sConn = NULL;
        return (-1);
    }
    connections = new covise::ConnectionList();
    connections->add(sConn);
    msg = new covise::Message;

    serverSN = new QSocketNotifier(sConn->get_id(NULL), QSocketNotifier::Read);

    //cerr << "listening on port " << port << endl;
// weil unter windows manchmal Messages verloren gehen
// der SocketNotifier wird nicht oft genug aufgerufen)
#if defined(_WIN32) || defined(__APPLE__)
    m_periodictimer = new QTimer;
    QObject::connect(m_periodictimer, SIGNAL(timeout()), this, SLOT(processMessages()));
    m_periodictimer->start(1000);
#endif

    QObject::connect(serverSN, SIGNAL(activated(int)), this, SLOT(processMessages()));
    return 0;
}

//------------------------------------------------------------------------
bool TUIMainWindow::serverRunning()
//------------------------------------------------------------------------
{
    return sConn && sConn->is_connected();
}

//------------------------------------------------------------------------
void TUIMainWindow::processMessages()
//------------------------------------------------------------------------
{
    //qDebug() << "process message called";
    covise::Connection *conn;
    while ((conn = connections->check_for_input(0.0001f)))
    {
        if (conn == sConn) // connection to server port
        {
            if (clientConn == NULL) // only accept connections if not already connected to a COVER
            {
                clientConn = sConn->spawn_connection();
                struct linger linger;
                linger.l_onoff = 0;
                linger.l_linger = 0;
                setsockopt(clientConn->get_id(NULL), SOL_SOCKET, SO_LINGER, (char *)&linger, sizeof(linger));

                clientSN = new QSocketNotifier(clientConn->get_id(NULL), QSocketNotifier::Read);
                QObject::connect(clientSN, SIGNAL(activated(int)),
                                 this, SLOT(processMessages()));

                // create connections for texture Thread and SceneGraph Browser Thread

                //qDebug() << "open texConn";
                texConn = new covise::ServerConnection(&port, 0, (covise::sender_type)0);
                texConn->listen();
                covise::TokenBuffer tb;
                tb << port;
                send(tb);

                //qDebug() << "sent port" << port;

                if (texConn->acceptOne(60) < 0)
                {
                    qDebug() << "Could not open server port" << port;
                    delete texConn;
                    texConn = NULL;
                    return;
                }
                if (!texConn->getSocket())
                {
                    qDebug() << "Could not get Socket" << port;
                    delete texConn;
                    texConn = NULL;
                    return;
                }

                linger.l_onoff = 0;
                linger.l_linger = 0;
                setsockopt(texConn->get_id(NULL), SOL_SOCKET, SO_LINGER, (char *)&linger, sizeof(linger));

                if (!texConn->is_connected()) // could not open server port
                {
                    fprintf(stderr, "Could not open server port %d\n", port);
                    delete texConn;
                    texConn = NULL;
                    return;
                }

                sgConn = new covise::ServerConnection(&port, 0, (covise::sender_type)0);
                sgConn->listen();
                covise::TokenBuffer stb;
                stb << port;
                send(stb);

                if (sgConn->acceptOne(60) < 0)
                {
                    fprintf(stderr, "Could not open server port %d\n", port);
                    delete sgConn;
                    sgConn = NULL;
                    return;
                }
                if (!sgConn->getSocket())
                {
                    fprintf(stderr, "Could not open server port %d\n", port);
                    delete sgConn;
                    sgConn = NULL;
                    return;
                }

                linger.l_onoff = 0;
                linger.l_linger = 0;
                setsockopt(sgConn->get_id(NULL), SOL_SOCKET, SO_LINGER, (char *)&linger, sizeof(linger));

                if (!sgConn->is_connected()) // could not open server port
                {
                    fprintf(stderr, "Could not open server port %d\n", port);
                    delete sgConn;
                    sgConn = NULL;
                    return;
                }

                toCOVERTexture = texConn;
                toCOVERSG = sgConn;

                connections->add(clientConn); //add new connection;
            }
            else
            {
                covise::Connection *tmpconn = sConn->spawn_connection();
                delete tmpconn;
            }
        }
        else
        {
            if (conn->recv_msg(msg))
            {
                if (msg)
                {
                    if (handleClient(msg))
                    {
                        return; // we have been deleted, exit immediately
                    }
                }
            }
        }
    }
}

//------------------------------------------------------------------------
void TUIMainWindow::addElementToLayout(TUIElement *elem)
//------------------------------------------------------------------------
{
    mainGrid->addWidget(elem->getWidget(), elem->getXpos(), elem->getYpos());
}

//------------------------------------------------------------------------
void TUIMainWindow::addElement(TUIElement *e)
//------------------------------------------------------------------------
{
    elements.push_back(e);
}

//------------------------------------------------------------------------
void TUIMainWindow::removeElement(TUIElement *e)
//------------------------------------------------------------------------
{
    elements.remove(e);
}

//------------------------------------------------------------------------
void TUIMainWindow::send(covise::TokenBuffer &tb)
//------------------------------------------------------------------------
{
    if (clientConn == NULL)
        return;
    covise::Message m(tb);
    m.type = covise::COVISE_MESSAGE_TABLET_UI;
    clientConn->send_msg(&m);
}

//------------------------------------------------------------------------
TUIElement *TUIMainWindow::createElement(int id, int type, QWidget *w, int parent, QString name)
//------------------------------------------------------------------------
{

    //cerr << "TUIMainWindow::createElement info: creating '" << name.toStdString()
    //         << "' of type " << type << " for parent " << id << endl;
    switch (type)
    {
    case TABLET_TEXT_FIELD:
        return new TUILabel(id, type, w, parent, name);
    case TABLET_BUTTON:
        return new TUIButton(id, type, w, parent, name);
    case TABLET_FILEBROWSER_BUTTON:
        return new TUIFileBrowserButton(id, type, w, parent, name);
    case TABLET_TAB:
        return new TUITab(id, type, w, parent, name);
    case TABLET_TEXTURE_TAB:
        return new TUITextureTab(id, type, w, parent, name);
    case TABLET_BROWSER_TAB:
        return new TUISGBrowserTab(id, type, w, parent, name);
    case TABLET_ANNOTATION_TAB:
        return new TUIAnnotationTab(id, type, w, parent, name);
#ifndef WITHOUT_VIRVO
#if !defined _WIN32_WCE && !defined ANDROID_TUI
    case TABLET_FUNCEDIT_TAB:
        return new TUIFunctionEditorTab(id, type, w, parent, name);
#endif
#endif
    case TABLET_COLOR_TAB:
        return new TUIColorTab(id, type, w, parent, name);
    case TABLET_FRAME:
        return new TUIFrame(id, type, w, parent, name);
    case TABLET_SCROLLAREA:
        return new TUIScrollArea(id, type, w, parent, name);
    case TABLET_SPLITTER:
        return new TUISplitter(id, type, w, parent, name);
    case TABLET_TOGGLE_BUTTON:
        return new TUIToggleButton(id, type, w, parent, name);
    case TABLET_BITMAP_TOGGLE_BUTTON:
        return new TUIToggleBitmapButton(id, type, w, parent, name);
    case TABLET_COMBOBOX:
        return new TUIComboBox(id, type, w, parent, name);
    case TABLET_LISTBOX:
        return new TUIListBox(id, type, w, parent, name);
    case TABLET_FLOAT_SLIDER:
        return new TUIFloatSlider(id, type, w, parent, name);
    case TABLET_SLIDER:
        return new TUISlider(id, type, w, parent, name);
    case TABLET_FLOAT_EDIT_FIELD:
        return new TUIFloatEdit(id, type, w, parent, name);
    case TABLET_INT_EDIT_FIELD:
        return new TUIIntEdit(id, type, w, parent, name);
    case TABLET_TAB_FOLDER:
        return new TUITabFolder(id, type, w, parent, name);
    case TABLET_MAP:
        return new TUIMap(id, type, w, parent, name);
    case TABLET_PROGRESS_BAR:
        return new TUIProgressBar(id, type, w, parent, name);
    case TABLET_NAV_ELEMENT:
        return new TUINavElement(id, type, w, parent, name);
    //     case TABLET_TEXT_SPIN_EDIT_FIELD:
    //       return new TUITextSpinEdit(id, type, w, parent, name);
    case TABLET_EDIT_FIELD:
        return new TUILineEdit(id, type, w, parent, name);
    case TABLET_COLOR_TRIANGLE:
        return new TUIColorTriangle(id, type, w, parent, name);
    case TABLET_COLOR_BUTTON:
        return new TUIColorButton(id, type, w, parent, name);
    case TABLET_TEXT_EDIT_FIELD:
        return new TUITextEdit(id, type, w, parent, name);
    case TABLET_POPUP:
        return new TUIPopUp(id, type, w, parent, name);
    case TABLET_UI_TAB:
        return new TUIUITab(id, type, w, parent, name);
    default:
        std::cerr << "TUIapplication::createElement info: unknown element type: " << type << std::endl;
        break;
    }
    return NULL;
}

//------------------------------------------------------------------------
void TUIMainWindow::deActivateTab(TUITab *activedTab)
//------------------------------------------------------------------------
{
    std::list<TUIElement *>::iterator iter;
    for (iter = elements.begin(); iter != elements.end(); iter++)
    {
        (*iter)->deActivate(activedTab);
    }
}

//------------------------------------------------------------------------
TUIElement *TUIMainWindow::getElement(int ID)
//------------------------------------------------------------------------
{
    std::list<TUIElement *>::iterator iter;
    for (iter = elements.begin(); iter != elements.end(); iter++)
    {
        if ((*iter)->getID() == ID)
        {
            return *iter;
            break;
        }
    }
    return NULL;
}

//------------------------------------------------------------------------
QWidget *TUIMainWindow::getWidget(int ID)
//------------------------------------------------------------------------
{
    std::list<TUIElement *>::iterator iter;
    for (iter = elements.begin(); iter != elements.end(); iter++)
    {
        if ((*iter)->getID() == ID)
        {
            return (*iter)->getWidget();
            break;
        }
    }
    return mainFrame;
}

//------------------------------------------------------------------------
bool TUIMainWindow::handleClient(covise::Message *msg)
//------------------------------------------------------------------------
{
    if((msg->type == covise::COVISE_MESSAGE_SOCKET_CLOSED) || (msg->type == covise::COVISE_MESSAGE_CLOSE_SOCKET))
    {
        delete clientSN;
        clientSN = NULL;
        connections->remove(msg->conn); //remove connection;
        delete msg->conn;
        msg->conn = NULL;
        clientConn = NULL;
        lastElement = NULL;
        lastID = -10;

        //remove all UI Elements
        while (elements.size())
        {
            TUIElement *ele = *(elements.begin()); // destructor removes the element from the list
            delete ele;
        }

#ifdef TABLET_PLUGIN
        MEUserInterface::instance()->removeTabletUI();
#endif
        return true; // we have been deleted, exit immediately
    }
    covise::TokenBuffer tb(msg);
    switch (msg->type)
    {
    case covise::COVISE_MESSAGE_TABLET_UI:
    {
        int type;
        tb >> type;
        int ID;
        //if(ID >= 0)
        // {
        switch (type)
        {

        case TABLET_CREATE:
        {
            tb >> ID;
            int elementType, parent;
            char *name;
            tb >> elementType;
            tb >> parent;
            tb >> name;
            //cerr << "TUIApplication::handleClient info: Create: ID: " << ID << " Type: " << elementType << " name: "<< name << " parent: " << parent << std::endl;
            TUIContainer *parentElem = (TUIContainer *)getElement(parent);

            QWidget *parentWidget;
            if (parentElem)
                parentWidget = parentElem->getWidget();
            else
                parentWidget = mainFrame;

            TUIElement *newElement = createElement(ID, elementType, parentWidget, parent, name);
            if (newElement)
            {
                lastElement = newElement;
                if (parentElem)
                    parentElem->addElement(lastElement);
                lastID = ID;
                QString parentName;
                if (parentElem)
                    parentName = parentElem->getName();
                std::string blacklist = "COVER.TabletPC.Blacklist:";
                QString qname(name);
                qname.replace(".", "").replace(":", "");
                blacklist += qname.toStdString();
// TODO: won't work for items with identical names but different parents - a random item will be found
//std::string value = covise::coCoviseConfig::getEntry(blacklist);

#if !defined _WIN32_WCE && !defined ANDROID_TUI
                std::string parent = covise::coCoviseConfig::getEntry("parent", blacklist);
                if (covise::coCoviseConfig::isOn(blacklist, false) && (parent.empty() || parent == parentName.toStdString()))
                {
                    newElement->setHidden(true);
                }
#endif
            }
        }
        break;
        case TABLET_SET_VALUE:
        {
            int type;
            tb >> type;
            tb >> ID;
            //cerr << "TUIApplication::handleClient info: Set Value ID: " << ID <<" Type: "<< type << endl;
            if (ID == lastID && (lastElement))
            {
                lastElement->setValue(type, tb);
            }
            else
            {
                TUIElement *ele = getElement(ID);
                if (ele)
                {
                    lastElement = ele;
                    lastID = ID;
                    ele->setValue(type, tb);
                }
                else
                {
                    std::cerr << "TUIApplication::handleClient warn: element not available in setValue: " << ID << std::endl;
                }
            }
        }
        break;
        case TABLET_REMOVE:
        {
            tb >> ID;
            if (ID == lastID)
            {
                lastElement = NULL;
                lastID = -10;
            }

            TUIElement *ele = getElement(ID);
            if (ele)
            {
                delete ele;
            }
            else
            {
#ifdef DEBUG
                std::cerr << "TUIApplication::handleClient warn: element not available in remove: " << ID << std::endl;
#endif
            }
        }
        break;

        default:
        {
            std::cerr << "TUIApplication::handleClient err: unhandled message type " << type << std::endl;
        }
        break;
        }
        //}
    }
    break;
    default:
    {
        if (msg->type > 0)
            std::cerr << "TUIApplication::handleClient err: unknown COVISE message type " << msg->type << " " << covise::covise_msg_types_array[msg->type] << std::endl;
    }
    break;
    }
    return false;
}

//------------------------------------------------------------------------
// close the application
//------------------------------------------------------------------------
void TUIMainWindow::closeEvent(QCloseEvent *ce)
{

    closeServer();

    //remove all UI Elements
    while (elements.size())
    {
        TUIElement *ele = *(elements.begin());
        delete ele;
    }

    ce->accept();
}

//------------------------------------------------------------------------
// short info
//------------------------------------------------------------------------
void TUIMainWindow::about()
{
    QMessageBox::about(this, "Tablet PC UI for COVER ",
                       "This is the new beautiful COVER User Interface");
}

//------------------------------------------------------------------------
// font callback
//------------------------------------------------------------------------
#ifndef TABLET_PLUGIN
void TUIMainWindow::fontCB(const QString &string)
{
    mainFont.setPixelSize(string.toInt());
    this->setFont(mainFont);
}

#else
void TUIMainWindow::fontCB(const QString &)
{
}
#endif

//------------------------------------------------------------------------
// style callback
//------------------------------------------------------------------------
#ifndef TABLET_PLUGIN
void TUIMainWindow::styleCB(const QString &string)
{
    QStyle *s = QStyleFactory::create(string);
    if (s)
        QApplication::setStyle(s);
}

#else
void TUIMainWindow::styleCB(const QString &)
{
}
#endif

#ifndef TABLET_PLUGIN

//------------------------------------------------------------------------
// create all stuff for the menubar
//------------------------------------------------------------------------
void TUIMainWindow::createMenubar()
{

    // File menu
    QMenu *file = menuBar()->addMenu("&File");
    _exit = new QAction(QPixmap(qexit), "&Quit", this);
    _exit->setShortcut(Qt::CTRL + Qt::Key_Q);
    _exit->setToolTip("Close the tablet UI");
    connect(_exit, SIGNAL(triggered()), qApp, SLOT(closeAllWindows()));
    file->addAction(_exit);

    // Help menu
    QMenu *help = menuBar()->addMenu("&Help");
    _help = new QAction("&About", this);
    _help->setShortcut(Qt::Key_F1);
    connect(_help, SIGNAL(triggered()), this, SLOT(about()));
    help->addAction(_help);
}

//------------------------------------------------------------------------
// create all stuff for the toolbar
//------------------------------------------------------------------------
void TUIMainWindow::createToolbar()
{
    QToolBar *toolbar = addToolBar("TabletUI Toolbar");

#if !defined _WIN32_WCE && !defined ANDROID_TUI
    bool visible = covise::coCoviseConfig::isOn("toolbar", "COVER.TabletPC", true);
#else
    bool visible = false;
#endif
    toolbar->setVisible(visible);

    // quit
    //toolbar->addAction(_exit);
    //toolbar->addSeparator();

    // fontsizes
    // label
    QLabel *l = new QLabel("FontSizes :", toolbar);
    l->setFont(mainFont);
    l->setToolTip("Select a new font size");
    toolbar->addWidget(l);

    // content
    QStringList list;
    list << "9"
         << "10"
         << "12"
         << "14"
         << "16"
         << "18"
         << "20"
         << "22"
         << "24";

    // combobox
    QComboBox *fontsize = new QComboBox();
#ifndef TABLET_PLUGIN
#if !defined _WIN32_WCE && !defined ANDROID_TUI
    std::string configFontsize = covise::coCoviseConfig::getEntry("fontsize", "COVER.TabletPC");
#else
    std::string configFontsize;
#endif
    if (!configFontsize.empty())
    {
        QString qfs = QString::fromStdString(configFontsize);
        if (!list.contains(qfs))
            list << qfs;
        mainFont.setPixelSize(qfs.toInt());
        this->setFont(mainFont);
    }
#endif
    QString ss;
    ss.setNum(mainFont.pointSize());
    if (!list.contains(ss))
    {
        list << ss;
    }
    fontsize->insertItems(0, list);

    int index = fontsize->findText(ss);
    fontsize->setCurrentIndex(index);
    toolbar->addWidget(fontsize);
    connect(fontsize, SIGNAL(activated(const QString &)),
            this, SLOT(fontCB(const QString &)));
    toolbar->addSeparator();

    //styles
    // label
    l = new QLabel("QtStyles :", toolbar);
    l->setFont(mainFont);
    l->setToolTip("Select another style");
    toolbar->addWidget(l);

    // content
    QStringList styles = QStyleFactory::keys();
    if (!styles.contains("Default"))
        styles.append("Default");
    styles.sort();

    // combobox
    QComboBox *qtstyles = new QComboBox(toolbar);
    qtstyles->insertItems(0, styles);
    toolbar->addWidget(qtstyles);

#if !defined _WIN32_WCE && !defined ANDROID_TUI
    // yac/covise configuration environment
    covise::coConfigGroup *mapConfig = new covise::coConfigGroup("Map Editor");
    mapConfig->addConfig(covise::coConfigDefaultPaths::getDefaultLocalConfigFilePath() + "mapqt.xml", "local", true);
    covise::coConfig::getInstance()->addConfig(mapConfig);

    QString currStyle = mapConfig->getValue("System.UserInterface.QtStyle");
    if (!currStyle.isEmpty())
    {
        QStyle *s = QStyleFactory::create(currStyle);
        if (s)
            QApplication::setStyle(s);
        qtstyles->setCurrentIndex(qtstyles->findText(currStyle));
    }
    else
#endif
    {
        QStyle *s = QStyleFactory::create("Default");
        if (s)
            QApplication::setStyle(s);
        qtstyles->setCurrentIndex(qtstyles->findText("Default"));
    }

    connect(qtstyles, SIGNAL(activated(const QString &)),
            this, SLOT(styleCB(const QString &)));
}
#endif

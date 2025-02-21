/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

/**************************************************************************
** ODD: OpenDRIVE Designer
**   Frank Naegele (c) 2010
**   <mail@f-naegele.de>
**   1/18/2010
**
**************************************************************************/

#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>

#include <src/util/odd.hpp>

class QMdiArea;
class QMdiSubWindow;

class QUndoGroup;
class QUndoView;

class QAction;
class QActionGroup;

class QMenu;

class QLabel;

class ProjectWidget;

class ToolManager;
class ToolAction;

class PrototypeManager;
class SignalManager;
class WizardManager;
class OsmImport;

#include "src/gui/projectionsettings.hpp"
#include "src/gui/importsettings.hpp"
#include "src/gui/lodsettings.hpp"

namespace Ui
{
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

    //################//
    // FUNCTIONS      //
    //################//

public:
    explicit MainWindow(QWidget *parent = NULL);
    virtual ~MainWindow();

    // Project //
    //
    ProjectWidget *getActiveProject();

    // Manager //
    //
    ToolManager *getToolManager() const
    {
        return toolManager_;
    }
    PrototypeManager *getPrototypeManager() const
    {
        return prototypeManager_;
    }
    SignalManager *getSignalManager() const
    {
        return signalManager_;
    }

    // Menus //
    //
    QMenu *getFileMenu() const
    {
        return fileMenu_;
    }
    QMenu *getEditMenu() const
    {
        return editMenu_;
    }
    QMenu *getWizardsMenu() const
    {
        return wizardsMenu_;
    }
    QMenu *getViewMenu() const
    {
        return viewMenu_;
    }
    QMenu *getProjectMenu() const
    {
        return projectMenu_;
    }
    QMenu *getHelpMenu() const
    {
        return helpMenu_;
    }

    const QString & getCovisedir()
    {
        return covisedir_;
    }

    // StatusBar //
    //
    void updateStatusBarPos(const QPointF &pos);

    // Undo //
    //
    QUndoGroup *getUndoGroup() const
    {
        return undoGroup_;
    }

    // ProjectTree //
    //
    void setProjectTree(QWidget *widget);

    // ProjectSettings //
    //
    void setProjectSettings(QWidget *widget);

    void open(QString fileName);
    void openTile(QString fileName);

private:
    // Init functions //
    //
    void createMenus();
    void createToolBars();
    void createStatusBar();

    void createActions();

    void createMdiArea();
    void createPrototypes();
    void createSignals();
    void createTools();
    void createUndo();
    void createTree();
    void createSettings();
    void createWizards();

    ProjectionSettings *projectionSettings;
    ImportSettings *importSettings;
    LODSettings *lodSettings;

    // Program Settings //
    //
    //	void						readSettings();
    //	void						writeSettings();

    // Project //
    //
    ProjectWidget *createProject();
    QMdiSubWindow *findProject(const QString &fileName);

    //################//
    // EVENTS         //
    //################//

protected:
    virtual void changeEvent(QEvent *event);
    virtual void closeEvent(QCloseEvent *event);

//################//
// SIGNALS        //
//################//

signals:
    void hasActiveProject(bool);

    //################//
    // SLOTS          //
    //################//

public slots:

    // Project Handling //
    //
    void activateProject();

    // Tools //
    //
    void toolAction(ToolAction *);

private slots:

    // Menu Slots //
    //
    void newFile();
    void open();
    void save();
    void openTile();
    void saveAs();
    void exportSpline();
    void changeSettings();
    void changeImportSettings();
    void importIntermapRoad();
    void importCarMakerRoad();
    void importCSVRoad();
    void importOSMRoad();
    void about();
    void openRecentFile();
    void changeLODSettings();

    //################//
    // PROPERTIES     //
    //################//

private:
    // Main GUI //
    //
    Ui::MainWindow *ui;
    OsmImport *osmi;

    // Official Menus //
    //
    QMenu *fileMenu_;
    QMenu *editMenu_;
    QMenu *wizardsMenu_;
    QMenu *viewMenu_;
    QMenu *projectMenu_;
    QMenu *helpMenu_;

    // Official ToolBars //
    //
    QToolBar *fileToolBar_;

    // Manager //
    //
    ToolManager *toolManager_;
    PrototypeManager *prototypeManager_;
    WizardManager *wizardManager_;
    SignalManager *signalManager_;

    // UI elements //
    //
    QMdiArea *mdiArea_;

    QDockWidget *undoDock_;
    QUndoGroup *undoGroup_;
    QUndoView *undoView_;

    QDockWidget *toolDock_;

    QDockWidget *treeDock_;
    QWidget *emptyTreeWidget_;

    QDockWidget *settingsDock_;
    QWidget *emptySettingsWidget_;

    // StatusBar //
    //
    QLabel *locationLabel_;

    // Project Menu //
    //
    QActionGroup *projectActionGroup;

    // Covise Directory Path //
    //
    QString covisedir_;
};

#endif // MAINWINDOW_HPP

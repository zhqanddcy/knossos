#include <QSettings>
#include <QDir>
#include <QFileInfoList>
#include <QToolBar>
#include <QMenu>

#include "widgets/GuiConstants.h"
#include "scripting.h"
#include "decorators/floatcoordinatedecorator.h"
#include "decorators/coordinatedecorator.h"
#include "decorators/colordecorator.h"
#include "decorators/treelistdecorator.h"
#include "decorators/nodelistdecorator.h"
#include "decorators/nodecommentdecorator.h"
#include "decorators/segmentlistdecorator.h"
#include "decorators/meshdecorator.h"
#include "decorators/transformdecorator.h"
#include "decorators/pointdecorator.h"
#include "proxies/skeletonproxy.h"
#include "proxies/pythonproxy.h"

#include "geometry/render.h"
#include "geometry/point.h"
#include "geometry/transform.h"
#include "geometry/shape.h"

#include "eventmodel.h"
#include "highlighter.h"
#include "skeleton/skeletonizer.h"
#include "viewer.h"
#include "widgets/mainwindow.h"

void PythonQtInit() {
    PythonQt::init(PythonQt::RedirectStdOut);
#ifdef QtAll
    PythonQt_QtAll::init();
#endif
}

Scripting::Scripting() {
    state->scripting = this;

    PythonQtInit();
    PythonQtObjectPtr ctx = PythonQt::self()->getMainModule();

    skeletonProxy = new SkeletonProxy();
    pythonProxy = new PythonProxy();
    signalRelay = new SignalRelay();
    PythonQt::self()->registerClass(&EmitOnCtorDtor::staticMetaObject);

    colorDecorator = new ColorDecorator();
    coordinateDecorator = new CoordinateDecorator();
    floatCoordinateDecorator = new FloatCoordinateDecorator();
    meshDecorator = new MeshDecorator();
    nodeListDecorator = new NodeListDecorator();
    nodeCommentDecorator = new NodeCommentDecorator();
    segmentListDecorator = new SegmentListDecorator();
    treeListDecorator = new TreeListDecorator();
//    transformDecorator = new TransformDecorator();
//    pointDecorator = new PointDecorator();

    ctx.evalScript("import sys");
    ctx.evalScript("sys.argv = ['']");  // <- this is needed to import the ipython module from the site-package
#ifdef Q_OS_OSX
    // as ipython does not export it's sys paths after the installation we refer to that site-package
    ctx.evalScript("sys.path.append('/Library/Python/2.7/site-packages')");
#endif
    ctx.evalScript("plugin_container = []");

    ctx.addObject("signalRelay", signalRelay);
    ctx.addObject("skeleton", skeletonProxy);
    ctx.addObject("knossos", pythonProxy);
    ctx.addObject("knossos_global_viewer", state->viewer);
    ctx.addObject("knossos_global_mainwindow", state->viewer->window);
    ctx.addObject("knossos_global_eventmodel", state->viewer->eventModel);
    ctx.addObject("knossos_global_skeletonizer", &Skeletonizer::singleton());
    ctx.addObject("knossos_global_segmentation", &Segmentation::singleton());
    ctx.addObject("knossos_global_loader", &Loader::Controller::singleton());
    ctx.addVariable("GL_POINTS", GL_POINTS);
    ctx.addVariable("GL_LINES", GL_LINES);
    ctx.addVariable("GL_LINE_STRIP", GL_LINE_STRIP);
    ctx.addVariable("GL_LINE_LOOP", GL_LINE_LOOP);
    ctx.addVariable("GL_TRIANGLES", GL_TRIANGLES);
    ctx.addVariable("GL_TRIANGLES_STRIP", GL_TRIANGLE_STRIP);
    ctx.addVariable("GL_TRIANGLE_FAN", GL_TRIANGLE_FAN);
    ctx.addVariable("GL_QUADS", GL_QUADS);
    ctx.addVariable("GL_QUAD_STRIP", GL_QUAD_STRIP);
    ctx.addVariable("GL_POLYGON", GL_POLYGON);
    addWidgets(ctx);

    QString module("internal");

    PythonQt::self()->addDecorators(colorDecorator);
    PythonQt::self()->registerCPPClass("color4F", "", module.toLocal8Bit().data());

    PythonQt::self()->addDecorators(coordinateDecorator);
    PythonQt::self()->registerCPPClass("Coordinate", "", module.toLocal8Bit().data());

    PythonQt::self()->addDecorators(floatCoordinateDecorator);
    PythonQt::self()->registerCPPClass("floatCoordinate", "", module.toLocal8Bit().data());

    PythonQt::self()->addDecorators(meshDecorator);
    PythonQt::self()->registerCPPClass("mesh", "", module.toLocal8Bit().data());

    PythonQt::self()->addDecorators(nodeListDecorator);
    PythonQt::self()->registerCPPClass("Node", "", module.toLocal8Bit().data());

    PythonQt::self()->addDecorators(nodeCommentDecorator);
    PythonQt::self()->registerCPPClass("NodeComment", "", module.toLocal8Bit().data());

    PythonQt::self()->addDecorators(segmentListDecorator);
    PythonQt::self()->registerCPPClass("Segment", "", module.toLocal8Bit().data());

    PythonQt::self()->addDecorators(treeListDecorator);
    PythonQt::self()->registerCPPClass("Tree", "", module.toLocal8Bit().data());

    changeWorkingDirectory();
    executeFromUserDirectory();

#ifdef Q_OS_LINUX //in linux there’s an explicit symlink to a python 2 binary
    ctx.evalFile(QString("sys.path.append('%1')").arg("./python2"));
#else
    ctx.evalFile(QString("sys.path.append('%1')").arg("./python"));
#endif

    autoStartTerminal();
}

QVariant getSettingsValue(const QString &key) {
    QSettings settings;
    settings.beginGroup(PYTHON_PROPERTY_WIDGET);
    auto value = settings.value(key);
    settings.endGroup();
    return value;
}

void Scripting::autoStartTerminal() {
    auto value = getSettingsValue(PYTHON_AUTOSTART_TERMINAL);
    if (value.isNull()) { return; }
    auto autoStartFolder = value.toBool();
    if (autoStartFolder) {
        qDebug() << "TRUE!";
        state->viewer->window->widgetContainer->pythonPropertyWidget->openTerminal();
    }
}

void Scripting::addScriptingObject(const QString &name, QObject *obj) {
    PythonQtObjectPtr ctx = PythonQt::self()->getMainModule();
    ctx.addObject(name, obj);
}

void Scripting::changeWorkingDirectory() {
    auto value = getSettingsValue(PYTHON_WORKING_DIRECTORY);
    if (value.isNull()) { return; }
    auto workingDir = value.toString();
    if (workingDir.isEmpty()) { return; }

    PythonQtObjectPtr ctx = PythonQt::self()->getMainModule();
    ctx.evalScript("import os");
    ctx.evalScript(QString("os.chdir('%1')").arg(workingDir));
}


void Scripting::executeFromUserDirectory() {
    auto value = getSettingsValue(PYTHON_AUTOSTART_FOLDER);
    if (value.isNull()) { return; }
    auto autoStartFolder = value.toString();
    if (autoStartFolder.isEmpty()) { return; }

    QDir scriptDir(autoStartFolder);
    QStringList endings;
    endings << "*.py";
    scriptDir.setNameFilters(endings);
    QFileInfoList entries = scriptDir.entryInfoList();
    PythonQtObjectPtr ctx = PythonQt::self()->getMainModule();
    foreach(const QFileInfo &script, entries) {
        QFile file(script.canonicalFilePath());
        if(!file.open(QIODevice::Text | QIODevice::ReadOnly)) {
            continue;
        }
        ctx.evalFile(script.canonicalFilePath());
    }
}

/** This methods create a pep8-style object name for all knossos widget and
 *  adds them to the python-context. Widgets with a leading Q are ignored
*/
void Scripting::addWidgets(PythonQtObjectPtr &context) {
    QWidgetList list = QApplication::allWidgets();
    foreach(QWidget *widget, list) {
        QString name = widget->metaObject()->className();
        QByteArray array;

        for(int i = 0; i < name.size(); i++) {
            if(name.at(i).isLower()) {
                array.append(name.at(i));
            } else if(name.at(i).isUpper()) {
                if(i == 0 && name.at(i) == 'Q') {
                    continue;
                } else if(i == 0 && name.at(i) != 'Q') {
                    array.append(name.at(i).toLower());
                } else {
                    array.append(QString("_%1").arg(name.at(i).toLower()));
                }
            }
        }

        context.addObject("widget_" + QString(array), widget);
    }
}

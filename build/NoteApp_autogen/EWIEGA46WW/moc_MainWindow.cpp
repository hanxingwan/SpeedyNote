/****************************************************************************
** Meta object code from reading C++ file 'MainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../MainWindow.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainWindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10MainWindowE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN10MainWindowE = QtMocHelpers::stringData(
    "MainWindow",
    "toggleBenchmark",
    "",
    "updateBenchmarkDisplay",
    "applyCustomColor",
    "updateThickness",
    "value",
    "changeTool",
    "index",
    "selectFolder",
    "saveCanvas",
    "saveAnnotated",
    "deleteCurrentPage",
    "loadPdf",
    "clearPdf",
    "saveCurrentPage",
    "switchPage",
    "pageNumber",
    "selectBackground",
    "updateZoom",
    "applyZoom",
    "updatePanRange",
    "updatePanX",
    "updatePanY",
    "forceUIRefresh",
    "switchTab",
    "addNewTab",
    "removeTabAt",
    "updateTabLabel",
    "toggleZoomSlider",
    "toggleThicknessSlider",
    "toggleFullscreen"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN10MainWindowE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      27,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  176,    2, 0x08,    1 /* Private */,
       3,    0,  177,    2, 0x08,    2 /* Private */,
       4,    0,  178,    2, 0x08,    3 /* Private */,
       5,    1,  179,    2, 0x08,    4 /* Private */,
       7,    1,  182,    2, 0x08,    6 /* Private */,
       9,    0,  185,    2, 0x08,    8 /* Private */,
      10,    0,  186,    2, 0x08,    9 /* Private */,
      11,    0,  187,    2, 0x08,   10 /* Private */,
      12,    0,  188,    2, 0x08,   11 /* Private */,
      13,    0,  189,    2, 0x08,   12 /* Private */,
      14,    0,  190,    2, 0x08,   13 /* Private */,
      15,    0,  191,    2, 0x08,   14 /* Private */,
      16,    1,  192,    2, 0x08,   15 /* Private */,
      18,    0,  195,    2, 0x08,   17 /* Private */,
      19,    0,  196,    2, 0x08,   18 /* Private */,
      20,    0,  197,    2, 0x08,   19 /* Private */,
      21,    0,  198,    2, 0x08,   20 /* Private */,
      22,    1,  199,    2, 0x08,   21 /* Private */,
      23,    1,  202,    2, 0x08,   23 /* Private */,
      24,    0,  205,    2, 0x08,   25 /* Private */,
      25,    1,  206,    2, 0x08,   26 /* Private */,
      26,    0,  209,    2, 0x08,   28 /* Private */,
      27,    1,  210,    2, 0x08,   29 /* Private */,
      28,    0,  213,    2, 0x08,   31 /* Private */,
      29,    0,  214,    2, 0x08,   32 /* Private */,
      30,    0,  215,    2, 0x08,   33 /* Private */,
      31,    0,  216,    2, 0x08,   34 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    6,
    QMetaType::Void, QMetaType::Int,    8,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   17,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    6,
    QMetaType::Void, QMetaType::Int,    6,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    8,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    8,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_ZN10MainWindowE.offsetsAndSizes,
    qt_meta_data_ZN10MainWindowE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN10MainWindowE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MainWindow, std::true_type>,
        // method 'toggleBenchmark'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateBenchmarkDisplay'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'applyCustomColor'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateThickness'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'changeTool'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'selectFolder'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'saveCanvas'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'saveAnnotated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'deleteCurrentPage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'loadPdf'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'clearPdf'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'saveCurrentPage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'switchPage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'selectBackground'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateZoom'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'applyZoom'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updatePanRange'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updatePanX'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'updatePanY'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'forceUIRefresh'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'switchTab'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'addNewTab'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'removeTabAt'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'updateTabLabel'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'toggleZoomSlider'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'toggleThicknessSlider'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'toggleFullscreen'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->toggleBenchmark(); break;
        case 1: _t->updateBenchmarkDisplay(); break;
        case 2: _t->applyCustomColor(); break;
        case 3: _t->updateThickness((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->changeTool((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->selectFolder(); break;
        case 6: _t->saveCanvas(); break;
        case 7: _t->saveAnnotated(); break;
        case 8: _t->deleteCurrentPage(); break;
        case 9: _t->loadPdf(); break;
        case 10: _t->clearPdf(); break;
        case 11: _t->saveCurrentPage(); break;
        case 12: _t->switchPage((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 13: _t->selectBackground(); break;
        case 14: _t->updateZoom(); break;
        case 15: _t->applyZoom(); break;
        case 16: _t->updatePanRange(); break;
        case 17: _t->updatePanX((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 18: _t->updatePanY((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 19: _t->forceUIRefresh(); break;
        case 20: _t->switchTab((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 21: _t->addNewTab(); break;
        case 22: _t->removeTabAt((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 23: _t->updateTabLabel(); break;
        case 24: _t->toggleZoomSlider(); break;
        case 25: _t->toggleThicknessSlider(); break;
        case 26: _t->toggleFullscreen(); break;
        default: ;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN10MainWindowE.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 27)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 27;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 27)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 27;
    }
    return _id;
}
QT_WARNING_POP
